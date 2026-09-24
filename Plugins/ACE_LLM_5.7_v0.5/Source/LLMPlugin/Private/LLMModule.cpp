// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMModule.h"
#include "Core.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include <string>
#include <Windows.h>
#include <thread>
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d12.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "ace.h"
#include "ace_result.h"
#include "LLMSettings.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

DEFINE_LOG_CATEGORY(LogACELLM);

#define LOCTEXT_NAMESPACE "LLMModule"

const TCHAR* ACEResultToString(uint32 Result)
{
	switch (Result)
	{
	case ACEResultOk:                              return TEXT("Ok");
	case ACEResultNullContext:                     return TEXT("NullContext");
	case ACEResultWaitTimeout:                     return TEXT("WaitTimeout");
	case ACEResultWaitFailed:                      return TEXT("WaitFailed");
	case ACEResultInvalidParameter:                return TEXT("InvalidParameter");
	case ACEResultOutOfMemory:                     return TEXT("OutOfMemory");
	case ACEResultInvalidDriverVersion:            return TEXT("InvalidDriverVersion");
	case ACEResultException:                       return TEXT("Exception");
	case ACEResultContextMismatch:                 return TEXT("ContextMismatch");
	case ACEResultNvIgiFailedToLoadSDK:            return TEXT("NvIgiFailedToLoadSDK");
	case ACEResultNvIgiFailedToInitSLM:            return TEXT("NvIgiFailedToInitSLM");
	case ACEResultRagDBNotFound:                   return TEXT("RagDBNotFound");
	case ACEResultFailedToInitRag:                 return TEXT("FailedToInitRag");
	case ACEResultRagQueryFailed:                  return TEXT("RagQueryFailed");
	case ACEResultRagInvalidSearchType:            return TEXT("RagInvalidSearchType");
	case ACEResultRagLexicalDBNotLoaded:           return TEXT("RagLexicalDBNotLoaded");
	case ACEResultRagSemanticDBNotLoaded:          return TEXT("RagSemanticDBNotLoaded");
	case ACEResultRagTooManySemanticDBsLoaded:     return TEXT("RagTooManySemanticDBsLoaded");
	case ACEResultRagTooManyLexicalDBsLoaded:      return TEXT("RagTooManyLexicalDBsLoaded");
	case ACEResultFailedToInitEmbedding:           return TEXT("FailedToInitEmbedding");
	case ACEResultEmbeddingModelNotFound:          return TEXT("EmbeddingModelNotFound");
	case ACEResultEmbeddingNullTokenizer:          return TEXT("EmbeddingNullTokenizer");
	case ACEResultEmbeddingCrossEncAlreadyLoaded:  return TEXT("EmbeddingCrossEncAlreadyLoaded");
	case ACEResultEmbeddingDimMismatch:            return TEXT("EmbeddingDimMismatch");
	case ACEResultSLMTokenizeFailed:               return TEXT("SLMTokenizeFailed");
	case ACEResultSLMDecodeFailed:                 return TEXT("SLMDecodeFailed");
	case ACEResultSLMTokenToPieceFailed:           return TEXT("SLMTokenToPieceFailed");
	case ACEResultAgentMaxToolExchanges:           return TEXT("AgentMaxToolExchanges");
	case ACEResultCancelled:                       return TEXT("Cancelled");
	case ACEResultInvalidState:                    return TEXT("InvalidState");
	case ACEResultInvalidFormat:                   return TEXT("InvalidFormat");
	case ACEResultBufferTooSmall:                  return TEXT("BufferTooSmall");
	case ACEResultUnsupportedVersion:              return TEXT("UnsupportedVersion");
	default:                                       return TEXT("Unknown");
	}
}

void LLMModule::StartupModule()
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	UE_LOG(LogACELLM, Log, TEXT("[Module] StartupModule: LLMPlugin loading"));
	UE_LOG(LogACELLM, Log, TEXT("[Module]   GraphicsRHI=%s  Avail RAM=%.1f GiB  Used RAM=%.1f GiB"),
		*FApp::GetGraphicsRHI(),
		MemStats.AvailablePhysical / (1024.0 * 1024.0 * 1024.0),
		MemStats.UsedPhysical      / (1024.0 * 1024.0 * 1024.0));

#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project",
			"Plugins",
			"NVIDIA ACE LLM",
			LOCTEXT("RuntimeSettingsName", "NVIDIA ACE LLM"),
			LOCTEXT("RuntimeSettingsDescription", "Configure my setting"),
			GetMutableDefault<ULLMSettings>());
		UE_LOG(LogACELLM, Log, TEXT("[Module]   Project settings page registered: Plugins -> NVIDIA ACE LLM"));
	}
	else
	{
		UE_LOG(LogACELLM, Warning, TEXT("[Module]   ISettingsModule unavailable - project settings page NOT registered"));
	}
#endif
}

void LLMModule::ShutdownModule()
{
	UE_LOG(LogACELLM, Log, TEXT("[Module] ShutdownModule: LLMPlugin unloading (context=%p model=%p)"),
		m_context, m_model);

	// Free the persisted ACE engine here -- once, at module/process shutdown. The
	// engine is intentionally kept alive across PIE sessions because the
	// llama/ggml-cuda(+CIG) backend cannot be safely destroyed and re-created within
	// one process (see ULLMEngine::DestroyLLMEngine). Destroy the model before the
	// context per the ACE ownership rule (ace.h @ ace_destroyContext).
	if (m_model != nullptr)
	{
		ace_destroyModel(m_model);
		m_model = nullptr;
		UE_LOG(LogACELLM, Log, TEXT("[Module]   ace_destroyModel done"));
	}
	if (m_context != nullptr)
	{
		const ACEResult cres = ace_destroyContext(m_context);
		m_context = nullptr;
		UE_LOG(LogACELLM, Log, TEXT("[Module]   ace_destroyContext -> %u (%s)"),
			cres, ACEResultToString(cres));
	}

	// Release the dedicated CIG command queue (if the plugin created one). This
	// must happen *after* ace_destroyContext so NVIGI no longer references it. The
	// RHI-owned primary graphics queue is never released here.
#if PLATFORM_WINDOWS
	if (m_bOwnsCommandQueue && m_d3d12CommandQueue != nullptr)
	{
		m_d3d12CommandQueue->Release();
		UE_LOG(LogACELLM, Log, TEXT("[Module]   released dedicated CIG command queue"));
	}
#endif
	m_bOwnsCommandQueue = false;

	// The D3D12 device/queue are owned by the RHI; we only hold non-owning copies
	// used at context creation. Clear them so the module ends in a clean state.
	m_d3d12Device = nullptr;
	m_d3d12CommandQueue = nullptr;

#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "NVIDIA ACE LLM");
	}
#endif
}

ACEContext* LLMModule::GetContext()
{
	return m_context;
}

void LLMModule::SetContext(ACEContext* ctx)
{
	m_context = ctx;
}

ID3D12Device* LLMModule::GetD3D12Device()
{
	return m_d3d12Device;
}

void LLMModule::SetD3D12Device(ID3D12Device* dev)
{
	m_d3d12Device = dev;
}

ID3D12CommandQueue* LLMModule::GetD3D12CommandQueue()
{
	return m_d3d12CommandQueue;
}

void LLMModule::SetD3D12CommandQueue(ID3D12CommandQueue* queue)
{
	m_d3d12CommandQueue = queue;
}

void LLMModule::SetOwnsCommandQueue(bool bOwns)
{
	m_bOwnsCommandQueue = bOwns;
}

ACEModel* LLMModule::GetModel()
{
	return m_model;
}

void LLMModule::SetModel(ACEModel* model)
{
	m_model = model;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(LLMModule, LLMPlugin)

