// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ACELLMComponentTypes.h"
#include "ACELLMComponent.generated.h"

class UACELLMSubsystem;
class ULLMChat;
class ULLMChatMessage;
class ULLMInferenceOptions;

/**
 * Drop-in Actor component that wraps the low-level ACE LLM API (ULLMEngine / ULLMChat /
 * ULLMInferenceOptions / ULLMInferenceTask / ULLMToolRunnerTask) in a single, Blueprint-friendly
 * surface — mirroring the ergonomics of UACEASRComponent.
 *
 * Typical use: drop on an NPC Actor, set a SystemPrompt, leave bAutoInitialize on, bind
 * OnResponseComplete, and call SendMessage with the player's text.
 */
UCLASS(ClassGroup = (ACE), meta = (BlueprintSpawnableComponent, DisplayName = "ACE LLM"))
class LLMPLUGIN_API UACELLMComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACELLMComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** If true, calls InitializeLLMAsync during BeginPlay. Disable to gate behind a loading screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool bAutoInitialize = false;

	/** Optional per-component .gguf override. Empty = use ULLMSettings::ModelFile. First initializer in the process wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (FilePathFilter = "gguf"))
	FFilePath ModelFileOverride;

	/** Token context window passed to CreateLLMEngine. Larger uses more VRAM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (ClampMin = "256"))
	int32 MaxContextSize = 4096;

	/** When true, auto-creates this component's ULLMChat on OnLLMReady so SendMessage works immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool bAutoCreateChat = true;

	/** Passed to CreateChatObject. When true, each assistant reply is appended to history for multi-turn memory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool bSaveChatHistory = true;

	/** MaxSize for CreateChatObject. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (ClampMin = "0"))
	int32 MaxHistoryMessages = 0;

	/** Seeded as the System message when the chat is created (and re-applied on ResetConversation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (MultiLine = true))
	FString SystemPrompt;

	/** Sampling options used by default for every SendMessage. Converted once into a cached ULLMInferenceOptions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	FACELLMInferenceConfig InferenceDefaults;

	/** When true, tool calls in a response are auto-dispatched via OnToolCalled and inference re-runs after SubmitToolResult. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool bAutoRunTools = true;

	/**
	 * Common-sense auto teardown. false (default) keeps the engine alive across PIE for instant reuse;
	 * true requests engine teardown on EndPlay (a PIE-safe no-op in the editor; a genuine free on
	 * session end in a packaged game).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool bDestroyEngineOnEndPlay = false;

	// ------------------------------------------------------------------
	// Control functions
	// ------------------------------------------------------------------

	/** Idempotent. Ensures the global engine is (being) created; fires OnLLMReady when it is up. */
	UFUNCTION(BlueprintCallable, Category = "NVIDIA ACE|LLM")
	void InitializeLLMAsync();

	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	bool IsLLMReady() const;

	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	bool IsLLMInitializing() const;

	/** True while an inference / tool loop is running (guards against overlapping SendMessage). */
	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	bool IsBusy() const { return bBusy; }

	/** Append a User message, run inference with the cached options, stream tokens, and complete via OnResponseComplete. */
	UFUNCTION(BlueprintCallable, Category = "NVIDIA ACE|LLM")
	void SendMessage(const FString& UserText);

	/** SendMessage variant with a one-off options / system-prompt override (override may be null/empty to use defaults). */
	UFUNCTION(BlueprintCallable, Category = "NVIDIA ACE|LLM")
	void SendMessageWithOptions(const FString& UserText, ULLMInferenceOptions* OptionsOverride, const FString& SystemPromptOverride);

	/**
	 * Report the outcome of a tool the LLM asked you to run (via OnToolCalled), then — once every
	 * pending tool call for this turn has been answered — automatically re-run inference so the
	 * model can compose its spoken reply.
	 *
	 * @param ToolId        The Id handed to you by OnToolCalled. Leave this EMPTY to answer the next
	 *                      pending tool call, which is all a single-tool NPC ever needs — so you do
	 *                      not have to stash or carry the Id.
	 * @param ResultContent What happened when YOU executed the tool. This is your game's own outcome
	 *                      report, authored by you — NOT something the LLM produced and NOT carried
	 *                      from OnResponseComplete. A plain sentence ("The lock clicked open") or a
	 *                      JSON object ({"success":true}) both work; the model reads it to write its
	 *                      reply.
	 */
	UFUNCTION(BlueprintCallable, Category = "NVIDIA ACE|LLM", meta = (AutoCreateRefTerm = "ToolId"))
	void SubmitToolResult(const FString& ToolId, const FString& ResultContent);

	/** Clear history then re-seed SystemPrompt. */
	UFUNCTION(BlueprintCallable, Category = "NVIDIA ACE|LLM")
	void ResetConversation();

	/** Rebuild the cached inference options from InferenceDefaults (call after changing fields at runtime). */
	UFUNCTION(BlueprintCallable, Category = "NVIDIA ACE|LLM")
	void RefreshInferenceOptions();

	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	ULLMChat* GetChat() const { return Chat; }

	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	ULLMInferenceOptions* GetInferenceOptions() const { return Options; }

	UFUNCTION(BlueprintPure, Category = "NVIDIA ACE|LLM")
	FACELLMRuntimeInfo GetRuntimeInfo() const;

	/** Releases this component's interest in the engine (teardown obeys the PIE-safe rules). */
	UFUNCTION(BlueprintCallable, Category = "NVIDIA ACE|LLM")
	void ShutdownLLM();

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMReadySignature OnLLMReady;

	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMErrorSignature OnLLMError;

	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMResponseStartedSignature OnResponseStarted;

	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMTokenSignature OnTokenStreamed;

	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMResponseCompleteSignature OnResponseComplete;

	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMToolCalledSignature OnToolCalled;

	UPROPERTY(BlueprintAssignable, Category = "NVIDIA ACE|LLM|Events")
	FACELLMConversationResetSignature OnConversationReset;

private:
	UACELLMSubsystem* ResolveSubsystem();
	void BindToSubsystem();
	void UnbindFromSubsystem();

	UFUNCTION()
	void HandleEngineReady();

	UFUNCTION()
	void HandleEngineError(const FString& ErrorMessage);

	UFUNCTION()
	void HandleTokenOutput(FString Token, FString PartialText);

	UFUNCTION()
	void HandleGeneratedResponse();

	void CreateChatInternal();
	void RunInference(ULLMInferenceOptions* InOptions, const FString& SystemPromptOverride);
	void CompleteResponse(ULLMChatMessage* ResponseMessage);

	UPROPERTY(Transient)
	TObjectPtr<UACELLMSubsystem> CachedSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<ULLMChat> Chat;

	UPROPERTY(Transient)
	TObjectPtr<ULLMInferenceOptions> Options;

	/** Options override for the current in-flight SendMessage (null = use the cached default Options). */
	UPROPERTY(Transient)
	TObjectPtr<ULLMInferenceOptions> ActiveOptions;

	bool bInitRequested = false;
	bool bComponentReady = false;
	bool bBusy = false;
	bool bBoundToSubsystem = false;
	int32 OutstandingToolResults = 0;

	/** Ids of tool calls from the current turn still awaiting a SubmitToolResult (front = next to answer). */
	TArray<FString> PendingToolCallIds;
};
