// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT

#include "ACELLMSubsystem.h"

#include "LLMEngine.h"
#include "LLMModule.h"
#include "LLMSettings.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void UACELLMSubsystem::Deinitialize()
{
	// At GameInstance teardown (end of PIE / packaged-game exit), honor a pending teardown
	// request. DestroyLLMEngine is a PIE-safe no-op; the real free happens at module shutdown.
	if (bDestroyRequested)
	{
		ULLMEngine::DestroyLLMEngine();
	}

	Super::Deinitialize();
}

void UACELLMSubsystem::InitializeEngineAsync(UObject* WorldContextObject, const FString& ModelFile, int32 MaxContextSize)
{
	++RefCount;

	if (bReady)
	{
		// Already up — notify interested components (idempotent on their side).
		OnEngineReady.Broadcast();
		return;
	}

	if (bInitializing)
	{
		// Creation already scheduled; the eventual broadcast will reach this caller too.
		return;
	}

	bInitializing = true;
	PendingModelFile = ModelFile;
	PendingContextSize = (MaxContextSize > 0 ? MaxContextSize : 4096);
	PendingWorldContext = WorldContextObject;

	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : (GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	if (World)
	{
		// Defer the (synchronous, multi-second) engine load to next tick so the calling
		// frame is not blocked and components can finish binding their ready delegates.
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UACELLMSubsystem::PerformDeferredInit));
	}
	else
	{
		PerformDeferredInit();
	}
}

void UACELLMSubsystem::PerformDeferredInit()
{
	bInitializing = false;

	UObject* WorldContext = PendingWorldContext.Get();
	const bool bLoaded = ULLMEngine::CreateLLMEngine(WorldContext, PendingModelFile, PendingContextSize);

	if (bLoaded)
	{
		bReady = true;
		LoadedContextSize = PendingContextSize;

		// Resolve the effective model path for runtime info (explicit override, else project setting).
		LoadedModelPath = PendingModelFile;
		if (LoadedModelPath.IsEmpty())
		{
			if (const ULLMSettings* Settings = GetDefault<ULLMSettings>())
			{
				LoadedModelPath = Settings->ModelFile.FilePath;
			}
		}

		OnEngineReady.Broadcast();
	}
	else
	{
		OnEngineError.Broadcast(TEXT("CreateLLMEngine failed. Check the model path in Project Settings -> Plugins -> NVIDIA ACE LLM."));
	}
}

void UACELLMSubsystem::ReleaseEngine(bool bRequestDestroy)
{
	if (bRequestDestroy)
	{
		bDestroyRequested = true;
	}

	if (RefCount > 0)
	{
		--RefCount;
	}

	if (RefCount == 0 && bDestroyRequested)
	{
		// PIE-safe no-op: the engine persists for the process; the real free happens once at
		// module shutdown (model before context). In a packaged single-session game this is
		// effectively "free on session end".
		ULLMEngine::DestroyLLMEngine();
		bReady = false;
		bDestroyRequested = false;
	}
}
