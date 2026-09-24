// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "ACELLMComponentTypes.generated.h"

class ULLMChatMessage;

/**
 * Sampling / streaming configuration surfaced directly on UACELLMComponent so that
 * inference options are component settings rather than a separate object the user must
 * construct. Defaults match the plugin's recommended values (and ULLMSettings defaults).
 * Converted once into a cached ULLMInferenceOptions and reused for every SendMessage.
 */
USTRUCT(BlueprintType)
struct FACELLMInferenceConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TopP = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (ClampMin = "0"))
	int32 TopK = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM", meta = (ClampMin = "0.0"))
	float RepeatPenalty = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool EnableStreaming = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool EnableThinkMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVIDIA ACE|LLM")
	bool OutputThinkTokens = false;
};

/** Read-only snapshot of the component / engine runtime state for diagnostics UI. */
USTRUCT(BlueprintType)
struct FACELLMRuntimeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NVIDIA ACE|LLM")
	FString ModelPath;

	UPROPERTY(BlueprintReadOnly, Category = "NVIDIA ACE|LLM")
	int32 MaxContextSize = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NVIDIA ACE|LLM")
	bool bIsReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "NVIDIA ACE|LLM")
	bool bIsInitializing = false;

	UPROPERTY(BlueprintReadOnly, Category = "NVIDIA ACE|LLM")
	bool bIsBusy = false;

	UPROPERTY(BlueprintReadOnly, Category = "NVIDIA ACE|LLM")
	int32 MessageCount = 0;
};

// ---------------------------------------------------------------------------
// Component event delegates (re-broadcast of the low-level async-task delegates,
// scoped to the owning Actor for friendlier Blueprint wiring).
// ---------------------------------------------------------------------------

/** Engine initialized (and, if bAutoCreateChat, the chat is built). Safe to SendMessage. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FACELLMReadySignature);

/** Engine init failed or an inference could not be started. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FACELLMErrorSignature, const FString&, ErrorMessage);

/** A SendMessage inference has begun (good for a "typing..." indicator). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FACELLMResponseStartedSignature);

/** Fired per generated token while streaming is enabled. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FACELLMTokenSignature, const FString&, Token, const FString&, PartialText);

/** Fired once when a new chat response is complete (the primary "new reply arrived" hook). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FACELLMResponseCompleteSignature, const FString&, FullResponse, ULLMChatMessage*, ResponseMessage);

/** Fired once per tool call in the latest response so the game can run the tool and SubmitToolResult. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FACELLMToolCalledSignature, const FString&, ToolName, const FString&, ToolId, const FString&, ToolArguments);

/** Fired after ResetConversation clears history. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FACELLMConversationResetSignature);
