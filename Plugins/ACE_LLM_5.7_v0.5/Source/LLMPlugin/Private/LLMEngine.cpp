// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMEngine.h"
#include "LLMModule.h"
#include "LLMSettings.h"
#include "Modules/ModuleManager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMemory.h"

#if PLATFORM_WINDOWS
#include "ID3D12DynamicRHI.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
    // Bridge ACE SDK log lines into the dedicated LogACELLM category and map
    // the SDK level onto a comparable UE verbosity so warnings/errors stand out
    // in the Output Log instead of being buried in LogTemp Display lines.
    void ace_logfn(ACELogLevel level, const char* msg, void* /*userData*/)
    {
        if (!msg)
        {
            return;
        }

        const FString Message(UTF8_TO_TCHAR(msg));

        switch (level)
        {
        case ACELogLevel_Error:
            UE_LOG(LogACELLM, Error, TEXT("[SDK] %s"), *Message);
            break;
        case ACELogLevel_Warn:
            UE_LOG(LogACELLM, Warning, TEXT("[SDK] %s"), *Message);
            break;
        case ACELogLevel_InferenceDebug:
            UE_LOG(LogACELLM, Verbose, TEXT("[SDK] %s"), *Message);
            break;
        default:
            UE_LOG(LogACELLM, Log, TEXT("[SDK] %s"), *Message);
            break;
        }
    }

#if PLATFORM_WINDOWS
    // Snapshot of DXGI memory accounting for the adapter that hosts a given
    // D3D12 device. Used to report VRAM usage before and after model load.
    struct FAdapterMemorySnapshot
    {
        FString AdapterName;
        uint64  DedicatedBudget   = 0;
        uint64  DedicatedCurrent  = 0;
        uint64  SharedBudget      = 0;
        uint64  SharedCurrent     = 0;
        bool    bValid            = false;
    };

    FAdapterMemorySnapshot QueryAdapterMemory(ID3D12Device* Device)
    {
        using Microsoft::WRL::ComPtr;
        FAdapterMemorySnapshot Snap;
        if (!Device)
        {
            return Snap;
        }

        const LUID DeviceLuid = Device->GetAdapterLuid();

        ComPtr<IDXGIFactory4> Factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&Factory))))
        {
            return Snap;
        }

        ComPtr<IDXGIAdapter1> Adapter;
        if (FAILED(Factory->EnumAdapterByLuid(DeviceLuid, IID_PPV_ARGS(&Adapter))))
        {
            return Snap;
        }

        DXGI_ADAPTER_DESC1 Desc{};
        if (SUCCEEDED(Adapter->GetDesc1(&Desc)))
        {
            Snap.AdapterName = FString(Desc.Description);
        }

        ComPtr<IDXGIAdapter3> Adapter3;
        if (FAILED(Adapter.As(&Adapter3)))
        {
            return Snap;
        }

        DXGI_QUERY_VIDEO_MEMORY_INFO Local{};
        DXGI_QUERY_VIDEO_MEMORY_INFO NonLocal{};
        if (SUCCEEDED(Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL,     &Local)) &&
            SUCCEEDED(Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &NonLocal)))
        {
            Snap.DedicatedBudget  = Local.Budget;
            Snap.DedicatedCurrent = Local.CurrentUsage;
            Snap.SharedBudget     = NonLocal.Budget;
            Snap.SharedCurrent    = NonLocal.CurrentUsage;
            Snap.bValid           = true;
        }
        return Snap;
    }

    void LogAdapterMemory(const TCHAR* Label, const FAdapterMemorySnapshot& Snap)
    {
        if (!Snap.bValid)
        {
            UE_LOG(LogACELLM, Log, TEXT("[Engine] %s: <DXGI memory query unavailable>"), Label);
            return;
        }
        const double ToMiB = 1.0 / (1024.0 * 1024.0);
        UE_LOG(LogACELLM, Log,
            TEXT("[Engine] %s adapter='%s'  VRAM used=%.1f / budget=%.1f MiB  shared used=%.1f / budget=%.1f MiB"),
            Label,
            *Snap.AdapterName,
            Snap.DedicatedCurrent * ToMiB, Snap.DedicatedBudget * ToMiB,
            Snap.SharedCurrent    * ToMiB, Snap.SharedBudget    * ToMiB);
    }

    void LogVRAMDelta(const FAdapterMemorySnapshot& Before, const FAdapterMemorySnapshot& After)
    {
        if (!Before.bValid || !After.bValid)
        {
            return;
        }
        const double ToMiB = 1.0 / (1024.0 * 1024.0);
        const int64 DedicatedDelta = static_cast<int64>(After.DedicatedCurrent) - static_cast<int64>(Before.DedicatedCurrent);
        const int64 SharedDelta    = static_cast<int64>(After.SharedCurrent)    - static_cast<int64>(Before.SharedCurrent);
        UE_LOG(LogACELLM, Log,
            TEXT("[Engine] VRAM delta after load: dedicated %+.1f MiB, shared %+.1f MiB"),
            DedicatedDelta * ToMiB, SharedDelta * ToMiB);
    }

    // Create a dedicated D3D12 command queue for the ACE/NVIGI CUDA-in-Graphics
    // (CIG) handoff.
    //
    // Why: handing NVIGI Unreal's *primary* graphics queue (RHIGetCommandQueue)
    // makes the long-running LLM inference serialize on the very queue the
    // renderer drives every frame. The first inference usually slips through, but
    // a second one can stall outstanding graphics work (e.g. the GPU skin cache)
    // long enough to trip the OS GPU watchdog (TDR) and surface as
    // DXGI_ERROR_DEVICE_HUNG / device-removed. This was reproducible on UE 5.5.
    //
    // Giving CIG its own queue lets the hardware scheduler interleave inference
    // with rendering instead of blocking the frame queue behind it. The caller
    // owns the returned queue (refcount 1) and must Release() it after the ACE
    // context is destroyed (handled by LLMModule::ShutdownModule).
    ID3D12CommandQueue* CreateDedicatedInferenceQueue(ID3D12Device* Device)
    {
        if (Device == nullptr)
        {
            return nullptr;
        }
        D3D12_COMMAND_QUEUE_DESC Desc = {};
        Desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        Desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        Desc.NodeMask = 0;
        ID3D12CommandQueue* Queue = nullptr;
        const HRESULT Hr = Device->CreateCommandQueue(&Desc, IID_PPV_ARGS(&Queue));
        if (FAILED(Hr) || Queue == nullptr)
        {
            UE_LOG(LogACELLM, Warning,
                TEXT("[Engine] Failed to create dedicated CIG command queue (hr=0x%08x); falling back to the shared RHI graphics queue."),
                static_cast<uint32>(Hr));
            return nullptr;
        }
        Queue->SetName(L"ACE_LLM_InferenceQueue");
        return Queue;
    }
#endif

    // Short human-readable name for a backend tier, used in the failover logs.
    const TCHAR* BackendDisplayName(EACELLMBackend Backend)
    {
        switch (Backend)
        {
        case EACELLMBackend::RTX: return TEXT("RTX (CIG)");
        case EACELLMBackend::GPU: return TEXT("GPU (D3D12)");
        case EACELLMBackend::CPU: return TEXT("CPU");
        default:                  return TEXT("Unknown");
        }
    }
}

ULLMEngine::ULLMEngine()
{
}

bool ULLMEngine::CreateLLMEngine(UObject* WorldContextObject,
    FString ModelFilename,
    int     MaxContextSize)
{
    const double StartSeconds = FPlatformTime::Seconds();

    UE_LOG(LogACELLM, Log, TEXT("[Engine] ===== CreateLLMEngine() ====="));
    UE_LOG(LogACELLM, Log, TEXT("[Engine] Args: ModelFilename='%s'  MaxContextSize=%d"),
        ModelFilename.IsEmpty() ? TEXT("<empty -> use project setting>") : *ModelFilename,
        MaxContextSize);

    // MaxContextSize is forwarded to ace_modelLoadParamsSetInt as an unsigned int,
    // so a non-positive value would wrap to a huge number and request an invalid
    // context window. Clamp to the default and warn instead.
    constexpr int DefaultMaxContextSize = 4096;
    if (MaxContextSize <= 0)
    {
        UE_LOG(LogACELLM, Warning,
            TEXT("[Engine] MaxContextSize=%d is invalid (must be > 0); clamping to %d"),
            MaxContextSize, DefaultMaxContextSize);
        MaxContextSize = DefaultMaxContextSize;
    }

    // -------------------------------------------------------------------
    // Stage 1: locate plugin module + verify single-context invariant.
    // -------------------------------------------------------------------
    LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());

    // Reuse across PIE sessions. The ACE/llama/ggml-cuda(+CIG) backend is a
    // process-global that does NOT survive destroy+recreate within one process:
    // re-initializing after a teardown corrupts the CUDA backend and aborts on the
    // next ace_Model_Chat (Play->Stop->Play crash in ggml_cuda; see Saved/Crashes
    // UECC-...). Initialize once per editor process and reuse afterwards; the engine
    // is freed once in LLMModule::ShutdownModule().
    if (Module.GetContext() != nullptr)
    {
        // An engine is already live. Rather than fail or re-initialize, reuse it:
        // the ACE/llama/ggml-cuda(+CIG) backend is a process-global that cannot be
        // safely destroyed and re-created within one process, so we keep the single
        // context/model alive for the editor/process lifetime and reuse it here.
        // The engine is freed once in LLMModule::ShutdownModule().
        UE_LOG(LogACELLM, Log,
            TEXT("[Engine] Reusing existing engine (persisted across PIE). Context=%p Model=%p"),
            Module.GetContext(), Module.GetModel());
        return true;
    }
    UE_LOG(LogACELLM, Verbose, TEXT("[Engine] Stage 1: module resolved, no existing context (clean state)."));

    // -------------------------------------------------------------------
    // Stage 2: resolve the model path against project settings + content dir.
    // -------------------------------------------------------------------
    const ULLMSettings* Settings = GetDefault<ULLMSettings>();
    const FString RawArg = ModelFilename;
    if (Settings && ModelFilename.IsEmpty())
    {
        ModelFilename = Settings->ModelFile.FilePath;
        UE_LOG(LogACELLM, Log, TEXT("[Engine] Stage 2: pulled model path from Project Settings -> '%s'"), *ModelFilename);
    }

    FString AbsolutePath;
    if (FPaths::IsRelative(ModelFilename))
    {
        AbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + ModelFilename);
    }
    else
    {
        AbsolutePath = FPaths::ConvertRelativePathToFull(ModelFilename);
    }
    UE_LOG(LogACELLM, Log, TEXT("[Engine] Stage 2: resolved '%s' -> '%s'"), *RawArg, *AbsolutePath);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*AbsolutePath))
    {
        UE_LOG(LogACELLM, Error, TEXT("[Engine] Stage 2 FAILED: model file does not exist: %s"), *AbsolutePath);
        return false;
    }
    const int64 ModelBytes = PlatformFile.FileSize(*AbsolutePath);
    UE_LOG(LogACELLM, Log, TEXT("[Engine] Stage 2: model file exists, size=%.1f MiB"),
        ModelBytes / (1024.0 * 1024.0));

    // -------------------------------------------------------------------
    // Stage 3: RHI detection + D3D12/CIG handoff prep.
    // -------------------------------------------------------------------
    const FPlatformMemoryStats MemBefore = FPlatformMemory::GetStats();
    UE_LOG(LogACELLM, Log,
        TEXT("[Engine] Stage 3: RHI='%s'  Avail RAM=%.1f GiB  Used RAM=%.1f GiB (pre-load)"),
        *FApp::GetGraphicsRHI(),
        MemBefore.AvailablePhysical / (1024.0 * 1024.0 * 1024.0),
        MemBefore.UsedPhysical      / (1024.0 * 1024.0 * 1024.0));

    ACEResult res = ACEResultOk;
    ACENvIGIParams nvIgiParams{};
    ACEInitArgs initArgs = {
        ACE_INIT_ARGS_VERSION,
        &nvIgiParams,
        nullptr,
        0,
        nullptr,
        ace_logfn,
        nullptr,
        ACELogLevel_InferenceDebug,
        {ACEDeviceType_None},
        {0, 0, 0},
    };

    // -------------------------------------------------------------------
    // Stage 4: select the hardware backend and initialize the ACE context,
    // with automatic failover RTX(CIG) -> GPU(D3D12) -> CPU. The preferred
    // backend comes from project settings (defaults to RTX). Whichever the
    // user picks, we step down the chain if a tier is unsupported or fails to
    // initialize, so the engine still loads on machines without a CIG-capable
    // GPU (mirrors the failover behaviour of the ACE_ASR / ACE_TTS plugins).
    // -------------------------------------------------------------------
    const ULLMSettings* LLMSettings = GetDefault<ULLMSettings>();
    const EACELLMBackend PreferredBackend = LLMSettings ? LLMSettings->PreferredBackend : EACELLMBackend::RTX;

    TArray<EACELLMBackend, TInlineAllocator<3>> BackendChain;
    switch (PreferredBackend)
    {
    case EACELLMBackend::RTX:
        BackendChain.Add(EACELLMBackend::RTX);
        BackendChain.Add(EACELLMBackend::GPU);
        BackendChain.Add(EACELLMBackend::CPU);
        break;
    case EACELLMBackend::GPU:
        BackendChain.Add(EACELLMBackend::GPU);
        BackendChain.Add(EACELLMBackend::CPU);
        break;
    case EACELLMBackend::CPU:
    default:
        BackendChain.Add(EACELLMBackend::CPU);
        break;
    }
    UE_LOG(LogACELLM, Log,
        TEXT("[Engine] Stage 4: preferred backend = %s (%d-tier failover chain)"),
        BackendDisplayName(PreferredBackend), BackendChain.Num());

#if PLATFORM_WINDOWS
    ID3D12Device*          device = nullptr; // non-null only if a D3D12 tier succeeds
    FAdapterMemorySnapshot VRAMBefore;
    const FString GraphicsRHIStr = FApp::GetGraphicsRHI();
    const bool bIsD3D12 = GraphicsRHIStr.Contains(TEXT("D3D12")) ||
                          GraphicsRHIStr.Contains(TEXT("DirectX 12"));
#endif

    ACEContext*    ctx           = nullptr;
    EACELLMBackend ActiveBackend = EACELLMBackend::CPU;

    for (int32 TierIdx = 0; TierIdx < BackendChain.Num() && ctx == nullptr; ++TierIdx)
    {
        const EACELLMBackend Tier        = BackendChain[TierIdx];
        const bool           bHasNextTier = (TierIdx + 1 < BackendChain.Num());

        // Reset device args to CPU defaults for each attempt.
        initArgs.deviceArgs = ACEDeviceArgs{ ACEDeviceType_None };

#if PLATFORM_WINDOWS
        ID3D12Device*       attemptDevice     = nullptr;
        ID3D12CommandQueue* attemptQueue      = nullptr;
        bool                bAttemptOwnsQueue = false;

        if (Tier == EACELLMBackend::RTX || Tier == EACELLMBackend::GPU)
        {
            if (!bIsD3D12)
            {
                UE_LOG(LogACELLM, Warning,
                    TEXT("[Engine] Stage 4: backend %s needs D3D12 but RHI is '%s' - skipping"),
                    BackendDisplayName(Tier), *GraphicsRHIStr);
                continue;
            }

            attemptDevice = GetD3D12Device(0);

            if (Tier == EACELLMBackend::RTX)
            {
                // RTX/CIG: dedicated isolated command queue so inference does not
                // serialize on Unreal's primary graphics queue (avoids the
                // 2nd-inference TDR / DXGI_ERROR_DEVICE_HUNG seen on UE 5.5). Fall
                // back to the shared RHI queue only if a dedicated one cannot be made.
                attemptQueue      = CreateDedicatedInferenceQueue(attemptDevice);
                bAttemptOwnsQueue = (attemptQueue != nullptr);
                if (!bAttemptOwnsQueue)
                {
                    attemptQueue = GetD3D12Queue();
                }
            }
            else // GPU
            {
                // Standard D3D12 handoff on the engine's main graphics queue
                // (no dedicated CIG queue).
                attemptQueue = GetD3D12Queue();
            }

            if (!attemptDevice || !attemptQueue)
            {
                if (bAttemptOwnsQueue && attemptQueue)
                {
                    attemptQueue->Release();
                }
                UE_LOG(LogACELLM, Warning,
                    TEXT("[Engine] Stage 4: backend %s device/queue null - skipping"),
                    BackendDisplayName(Tier));
                continue;
            }

            ACEDeviceArgs devArgs = {};
            devArgs.deviceType                    = ACEDeviceType_D3D12;
            devArgs.deviceInfo.d3d12.device       = attemptDevice;
            devArgs.deviceInfo.d3d12.commandQueue = attemptQueue;
            initArgs.deviceArgs = devArgs;

            UE_LOG(LogACELLM, Log,
                TEXT("[Engine] Stage 4: backend %s prepared (device=%p queue=%p %s)"),
                BackendDisplayName(Tier), attemptDevice, attemptQueue,
                bAttemptOwnsQueue ? TEXT("dedicated CIG queue") : TEXT("shared RHI graphics queue"));
        }
        else
        {
            UE_LOG(LogACELLM, Log, TEXT("[Engine] Stage 4: backend CPU (ACEDeviceType_None)"));
        }
#else
        if (Tier != EACELLMBackend::CPU)
        {
            UE_LOG(LogACELLM, Warning,
                TEXT("[Engine] Stage 4: backend %s unsupported on non-Windows - skipping"),
                BackendDisplayName(Tier));
            continue;
        }
        UE_LOG(LogACELLM, Log, TEXT("[Engine] Stage 4: backend CPU (ACEDeviceType_None)"));
#endif

        const double InitCtxStart = FPlatformTime::Seconds();
        ACEContext*  attemptCtx   = nullptr;
        res = ace_initContext(&attemptCtx, &initArgs);
        const double InitCtxMs = (FPlatformTime::Seconds() - InitCtxStart) * 1000.0;

        if (res == ACEResultOk && attemptCtx)
        {
            ctx           = attemptCtx;
            ActiveBackend = Tier;
            UE_LOG(LogACELLM, Log,
                TEXT("[Engine] Stage 4: ace_initContext OK with backend %s (ctx=%p) in %.1f ms"),
                BackendDisplayName(Tier), ctx, InitCtxMs);

#if PLATFORM_WINDOWS
            if (Tier == EACELLMBackend::RTX || Tier == EACELLMBackend::GPU)
            {
                device = attemptDevice;
                Module.SetD3D12Device(attemptDevice);
                Module.SetD3D12CommandQueue(attemptQueue);
                Module.SetOwnsCommandQueue(bAttemptOwnsQueue);
                VRAMBefore = QueryAdapterMemory(attemptDevice);
                LogAdapterMemory(TEXT("VRAM pre-load"), VRAMBefore);
            }
#endif
        }
        else
        {
            UE_LOG(LogACELLM, Warning,
                TEXT("[Engine] Stage 4: ace_initContext FAILED with backend %s -> %u (%s) after %.1f ms%s"),
                BackendDisplayName(Tier), res, ACEResultToString(res), InitCtxMs,
                bHasNextTier ? TEXT(" - failing over to next backend") : TEXT(""));
#if PLATFORM_WINDOWS
            // Release a dedicated queue created for a failed RTX attempt so the next
            // tier (or shutdown) does not leak it.
            if (bAttemptOwnsQueue && attemptQueue)
            {
                attemptQueue->Release();
            }
#endif
        }
    }

    if (ctx == nullptr)
    {
        UE_LOG(LogACELLM, Error,
            TEXT("[Engine] Stage 4 FAILED: all backends in the failover chain were exhausted; engine not created"));
        return false;
    }
    UE_LOG(LogACELLM, Log,
        TEXT("[Engine] Stage 4: active backend = %s"), BackendDisplayName(ActiveBackend));

    // -------------------------------------------------------------------
    // Stage 5: build ACEModelLoadParams.
    // -------------------------------------------------------------------
    ACEModelLoadParams* loadParams = nullptr;
    res = ace_createModelLoadParams(ctx, &loadParams);
    if (res != ACEResultOk || !loadParams)
    {
        UE_LOG(LogACELLM, Error,
            TEXT("[Engine] Stage 5 FAILED: ace_createModelLoadParams returned %u (%s)"),
            res, ACEResultToString(res));
        ace_destroyContext(ctx);
        return false;
    }

    res = ace_modelLoadParamsSetInt(loadParams, ACEModelLoadParam_MaxContextSize, static_cast<unsigned int>(MaxContextSize));
    if (res != ACEResultOk)
    {
        UE_LOG(LogACELLM, Warning,
            TEXT("[Engine] Stage 5: setting MaxContextSize=%d returned %u (%s)"),
            MaxContextSize, res, ACEResultToString(res));
    }
    res = ace_modelLoadParamsSetInt(loadParams, ACEModelLoadParam_InferenceOutputBufferSize, 0);
    if (res != ACEResultOk)
    {
        UE_LOG(LogACELLM, Warning,
            TEXT("[Engine] Stage 5: setting InferenceOutputBufferSize=0 returned %u (%s)"),
            res, ACEResultToString(res));
    }
    UE_LOG(LogACELLM, Log, TEXT("[Engine] Stage 5: ModelLoadParams ready (MaxContextSize=%d)"), MaxContextSize);

    // -------------------------------------------------------------------
    // Stage 6: load the GGUF model (this is the slow synchronous step).
    // -------------------------------------------------------------------
    UE_LOG(LogACELLM, Log, TEXT("[Engine] Stage 6: calling ace_loadModel - synchronous, may take several seconds..."));
    const double LoadStart = FPlatformTime::Seconds();
    ACEModel* model = nullptr;
    res = ace_loadModel(ctx, &model, TCHAR_TO_UTF8(*AbsolutePath), loadParams);
    const double LoadMs = (FPlatformTime::Seconds() - LoadStart) * 1000.0;

    if (res != ACEResultOk)
    {
        UE_LOG(LogACELLM, Error,
            TEXT("[Engine] Stage 6 FAILED: ace_loadModel returned %u (%s) after %.1f ms"),
            res, ACEResultToString(res), LoadMs);
        ace_destroyModelLoadParams(loadParams);
        ace_destroyContext(ctx);
        return false;
    }
    UE_LOG(LogACELLM, Log,
        TEXT("[Engine] Stage 6: ace_loadModel OK (model=%p) in %.1f ms"),
        model, LoadMs);

    // ace_loadModel copies what it needs from the params block, so free it
    // immediately to avoid leaking it for the lifetime of the context.
    ace_destroyModelLoadParams(loadParams);
    loadParams = nullptr;

    // -------------------------------------------------------------------
    // Stage 7: publish handles to the module + post-load resource snapshot.
    // -------------------------------------------------------------------
    Module.SetContext(ctx);
    Module.SetModel(model);

#if PLATFORM_WINDOWS
    if (bIsD3D12 && device)
    {
        const FAdapterMemorySnapshot VRAMAfter = QueryAdapterMemory(device);
        LogAdapterMemory(TEXT("VRAM post-load"), VRAMAfter);
        LogVRAMDelta(VRAMBefore, VRAMAfter);
    }
#endif

    const FPlatformMemoryStats MemAfter = FPlatformMemory::GetStats();
    const double TotalMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
    UE_LOG(LogACELLM, Log,
        TEXT("[Engine] DONE: engine ready in %.1f ms  | RAM used delta=%+.1f MiB  | ctx=%p  model=%p"),
        TotalMs,
        (int64(MemAfter.UsedPhysical) - int64(MemBefore.UsedPhysical)) / (1024.0 * 1024.0),
        ctx, model);
    UE_LOG(LogACELLM, Log, TEXT("[Engine] ==============================="));
    return true;
}

bool ULLMEngine::DestroyLLMEngine()
{
    // Intentionally a NO-OP during PIE. Destroying the ACE context tears down the
    // process-global llama/ggml-cuda(+CIG) backend, which cannot be re-initialized in
    // the same process -- the next Play's inference aborts in ggml_cuda (evidence:
    // Saved/Crashes UECC-..., abort in RunChatInference->ace_Model_Chat after
    // Play->Stop->Play). The engine is persisted for the editor/process lifetime and
    // freed once (model before context) in LLMModule::ShutdownModule().
    LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());
    UE_LOG(LogACELLM, Log,
        TEXT("[Engine] DestroyLLMEngine: no-op (engine persists across PIE; freed at module shutdown). ctx=%p model=%p"),
        Module.GetContext(), Module.GetModel());
    return true;
}

ID3D12Device* ULLMEngine::GetD3D12Device(int devIndex)
{
#if PLATFORM_WINDOWS
    FDynamicRHI* DynamicRHI = GDynamicRHI;
    if (!DynamicRHI || DynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D12)
    {
        UE_LOG(LogACELLM, Warning, TEXT("[Engine] GetD3D12Device: GDynamicRHI is %s"),
            DynamicRHI ? TEXT("not D3D12") : TEXT("null"));
        return nullptr;
    }
    ID3D12DynamicRHI* D3D12RHI = static_cast<ID3D12DynamicRHI*>(DynamicRHI);
    return D3D12RHI->RHIGetDevice(devIndex);
#else
    return nullptr;
#endif
}

ID3D12CommandQueue* ULLMEngine::GetD3D12Queue()
{
#if PLATFORM_WINDOWS
    FDynamicRHI* DynamicRHI = GDynamicRHI;
    if (!DynamicRHI || DynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D12)
    {
        UE_LOG(LogACELLM, Warning, TEXT("[Engine] GetD3D12Queue: GDynamicRHI is %s"),
            DynamicRHI ? TEXT("not D3D12") : TEXT("null"));
        return nullptr;
    }
    ID3D12DynamicRHI* D3D12RHI = static_cast<ID3D12DynamicRHI*>(DynamicRHI);
    return D3D12RHI->RHIGetCommandQueue();
#else
    return nullptr;
#endif
}
