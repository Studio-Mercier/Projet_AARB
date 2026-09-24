// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ACELLMComponentTypes.h"
#include "ACELLMSubsystem.generated.h"

/**
 * Process-wide owner / arbiter of the single global ACE LLM engine (ACEContext + ACEModel).
 *
 * The ACE/llama/ggml-cuda(+CIG) backend is a process-global that cannot be safely destroyed
 * and re-created within one process, so per-Actor components must NOT own the engine lifetime.
 * Components register interest via InitializeEngineAsync (ref-counted); the subsystem creates
 * the engine exactly once and multicasts OnEngineReady. Late callers that ask after the engine
 * is already up receive an immediate ready broadcast.
 *
 * The subsystem itself is per-GameInstance (so its ready/ref-count state resets each PIE
 * session), while the underlying engine persists at the module level across PIE — on a fresh
 * PIE session CreateLLMEngine simply reuses the live context and returns immediately.
 */
UCLASS()
class LLMPLUGIN_API UACELLMSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/**
	 * Register interest in the engine and ensure it is (being) created. Idempotent.
	 * Broadcasts OnEngineReady when the engine is up (immediately if already ready).
	 * The actual CreateLLMEngine call is deferred to the next tick so the calling frame
	 * is not blocked inline and late delegate binders still receive the callback.
	 */
	void InitializeEngineAsync(UObject* WorldContextObject, const FString& ModelFile, int32 MaxContextSize);

	/**
	 * Release one component's interest in the engine. When the last interested component
	 * releases AND any of them requested teardown, DestroyLLMEngine() is invoked (a PIE-safe
	 * no-op; the real free happens deterministically at module shutdown).
	 */
	void ReleaseEngine(bool bRequestDestroy);

	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	bool IsEngineReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	bool IsEngineInitializing() const { return bInitializing; }

	FString GetLoadedModelPath() const { return LoadedModelPath; }
	int32 GetLoadedContextSize() const { return LoadedContextSize; }

	/** Broadcast to all interested components when the engine becomes ready. */
	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMReadySignature OnEngineReady;

	/** Broadcast to all interested components when engine initialization fails. */
	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMErrorSignature OnEngineError;

private:
	void PerformDeferredInit();

	int32 RefCount = 0;
	bool  bReady = false;
	bool  bInitializing = false;
	bool  bDestroyRequested = false;

	FString PendingModelFile;
	int32   PendingContextSize = 4096;
	TWeakObjectPtr<UObject> PendingWorldContext;

	FString LoadedModelPath;
	int32   LoadedContextSize = 0;
};
