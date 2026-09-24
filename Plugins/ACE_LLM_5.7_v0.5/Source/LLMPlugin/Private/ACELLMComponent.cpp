// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT

#include "ACELLMComponent.h"

#include "ACELLMSubsystem.h"
#include "LLMChat.h"
#include "LLMChatMessage.h"
#include "LLMInferenceOptions.h"
#include "LLMInferenceTask.h"
#include "LLMToolCallList.h"
#include "LLMToolCall.h"
#include "LLMModule.h"
#include "LLMTypes.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"

UACELLMComponent::UACELLMComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UACELLMComponent::BeginPlay()
{
	Super::BeginPlay();

	BindToSubsystem();

	if (bAutoInitialize)
	{
		InitializeLLMAsync();
	}
}

void UACELLMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownLLM();
	UnbindFromSubsystem();

	Super::EndPlay(EndPlayReason);
}

UACELLMSubsystem* UACELLMComponent::ResolveSubsystem()
{
	if (CachedSubsystem)
	{
		return CachedSubsystem;
	}

	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			CachedSubsystem = GI->GetSubsystem<UACELLMSubsystem>();
		}
	}
	return CachedSubsystem;
}

void UACELLMComponent::BindToSubsystem()
{
	if (bBoundToSubsystem)
	{
		return;
	}

	if (UACELLMSubsystem* Subsystem = ResolveSubsystem())
	{
		Subsystem->OnEngineReady.AddDynamic(this, &UACELLMComponent::HandleEngineReady);
		Subsystem->OnEngineError.AddDynamic(this, &UACELLMComponent::HandleEngineError);
		bBoundToSubsystem = true;
	}
}

void UACELLMComponent::UnbindFromSubsystem()
{
	if (!bBoundToSubsystem)
	{
		return;
	}

	if (CachedSubsystem)
	{
		CachedSubsystem->OnEngineReady.RemoveDynamic(this, &UACELLMComponent::HandleEngineReady);
		CachedSubsystem->OnEngineError.RemoveDynamic(this, &UACELLMComponent::HandleEngineError);
	}
	bBoundToSubsystem = false;
}

void UACELLMComponent::InitializeLLMAsync()
{
	BindToSubsystem();

	UACELLMSubsystem* Subsystem = ResolveSubsystem();
	if (!Subsystem)
	{
		OnLLMError.Broadcast(TEXT("UACELLMSubsystem unavailable (no GameInstance)."));
		return;
	}

	bInitRequested = true;
	Subsystem->InitializeEngineAsync(this, ModelFileOverride.FilePath, MaxContextSize);
}

void UACELLMComponent::HandleEngineReady()
{
	// The subsystem broadcasts to all interested components. Only react if this component
	// asked to initialize and has not already completed its own setup (idempotent).
	if (!bInitRequested || bComponentReady)
	{
		return;
	}

	if (bAutoCreateChat)
	{
		CreateChatInternal();
		if (!Chat)
		{
			OnLLMError.Broadcast(TEXT("CreateChatObject failed after engine ready."));
			return;
		}
	}

	if (!Options)
	{
		RefreshInferenceOptions();
	}

	bComponentReady = true;
	OnLLMReady.Broadcast();
}

void UACELLMComponent::HandleEngineError(const FString& ErrorMessage)
{
	if (!bInitRequested || bComponentReady)
	{
		return;
	}
	OnLLMError.Broadcast(ErrorMessage);
}

void UACELLMComponent::CreateChatInternal()
{
	Chat = ULLMChat::CreateChatObject(this, MaxHistoryMessages, bSaveChatHistory);
	if (Chat && !SystemPrompt.IsEmpty())
	{
		Chat->MakeNewMessage(this, ELLMChatRole::System, SystemPrompt);
	}
}

void UACELLMComponent::RefreshInferenceOptions()
{
	const FACELLMInferenceConfig& Cfg = InferenceDefaults;
	Options = ULLMInferenceOptions::CreateLLMInferenceOptions(
		this,
		Cfg.Temperature,
		Cfg.TopP,
		Cfg.TopK,
		Cfg.MinP,
		Cfg.RepeatPenalty,
		Cfg.EnableStreaming,
		Cfg.EnableThinkMode,
		Cfg.OutputThinkTokens);
}

bool UACELLMComponent::IsLLMReady() const
{
	return bComponentReady && (!bAutoCreateChat || Chat != nullptr);
}

bool UACELLMComponent::IsLLMInitializing() const
{
	return bInitRequested && !bComponentReady;
}

void UACELLMComponent::SendMessage(const FString& UserText)
{
	SendMessageWithOptions(UserText, nullptr, FString());
}

void UACELLMComponent::SendMessageWithOptions(const FString& UserText, ULLMInferenceOptions* OptionsOverride, const FString& SystemPromptOverride)
{
	if (!IsLLMReady())
	{
		OnLLMError.Broadcast(TEXT("SendMessage called before the LLM is ready."));
		return;
	}
	if (bBusy)
	{
		UE_LOG(LogACELLM, Warning, TEXT("[Component] SendMessage ignored: an inference is already in progress."));
		return;
	}
	if (!Chat)
	{
		OnLLMError.Broadcast(TEXT("SendMessage called with no chat object (bAutoCreateChat is false)."));
		return;
	}

	Chat->MakeNewMessage(this, ELLMChatRole::User, UserText);

	bBusy = true;
	ActiveOptions = OptionsOverride ? OptionsOverride : Options.Get();
	OnResponseStarted.Broadcast();
	RunInference(ActiveOptions, SystemPromptOverride);
}

void UACELLMComponent::RunInference(ULLMInferenceOptions* InOptions, const FString& SystemPromptOverride)
{
	ULLMInferenceTask* Task = ULLMInferenceTask::RunLLMChat(this, Chat, InOptions, SystemPromptOverride);
	if (!Task)
	{
		bBusy = false;
		OnLLMError.Broadcast(TEXT("RunLLMChat failed to start (engine not initialized)."));
		return;
	}

	Task->OnTokenOutput.AddDynamic(this, &UACELLMComponent::HandleTokenOutput);
	Task->OnGeneratedResponse.AddDynamic(this, &UACELLMComponent::HandleGeneratedResponse);
	Task->Activate();
}

void UACELLMComponent::HandleTokenOutput(FString Token, FString PartialText)
{
	OnTokenStreamed.Broadcast(Token, PartialText);
}

void UACELLMComponent::HandleGeneratedResponse()
{
	ULLMChatMessage* Response = Chat ? Chat->GetLatestResponse() : nullptr;

	if (bAutoRunTools && Response)
	{
		ULLMToolCallList* ToolList = Response->GetToolCallList(this);
		const int32 ToolCount = ToolList ? ToolList->GetCount() : 0;

		if (ToolCount > 0)
		{
			// Set the expected count BEFORE broadcasting so synchronous SubmitToolResult calls
			// (game executes the tool inline) decrement against a complete total.
			OutstandingToolResults = ToolCount;

			// Cache the pending tool-call ids so the developer can answer with SubmitToolResult and
			// an EMPTY ToolId (we resolve "the next pending call" for them).
			PendingToolCallIds.Reset();
			for (int32 i = 0; i < ToolCount; ++i)
			{
				if (ULLMToolCall* Call = ToolList->GetToolCall(i))
				{
					PendingToolCallIds.Add(Call->Id);
				}
			}

			// Dispatch each tool call (same iteration ULLMToolRunnerTask performs internally).
			for (int32 i = 0; i < ToolCount; ++i)
			{
				if (ULLMToolCall* Call = ToolList->GetToolCall(i))
				{
					OnToolCalled.Broadcast(Call->Name, Call->Id, Call->Arguments);
				}
			}
			return; // completion deferred until all tool results are submitted
		}
	}

	CompleteResponse(Response);
}

void UACELLMComponent::SubmitToolResult(const FString& ToolId, const FString& ResultContent)
{
	if (!Chat)
	{
		return;
	}

	// Resolve which tool call this result answers. An explicit ToolId (from OnToolCalled) is
	// preferred; if it is empty we answer the next pending call, so a single-tool NPC never has to
	// carry the Id around.
	FString ResolvedId = ToolId;
	if (ResolvedId.IsEmpty() && PendingToolCallIds.Num() > 0)
	{
		ResolvedId = PendingToolCallIds[0];
	}
	PendingToolCallIds.RemoveSingle(ResolvedId);

	// Build the OpenAI-style tool-result message: { "role": "tool", "tool_call_id": <ResolvedId>,
	// "content": <ResultContent> }. The Tool-role message tells the model the action finished and
	// what happened; the model uses this text to compose its natural-language reply on the next
	// (turn 2) inference pass. The tool_call_id correlates this result with the specific tool call
	// the model emitted on turn 1 — without it some chat templates reject or ignore the result.
	if (ULLMChatMessage* ToolMessage = Chat->MakeNewMessage(this, ELLMChatRole::Tool, ResultContent))
	{
		if (!ResolvedId.IsEmpty())
		{
			if (ACEChatMessage* NativeMessage = ToolMessage->GetNativeObject())
			{
				const auto ToolIdUtf8 = StringCast<UTF8CHAR>(*ResolvedId);
				ace_chatMessageSetToolCallId(NativeMessage, reinterpret_cast<const char*>(ToolIdUtf8.Get()));
			}
		}
	}

	if (OutstandingToolResults > 0)
	{
		--OutstandingToolResults;
	}

	if (OutstandingToolResults <= 0)
	{
		OutstandingToolResults = 0;
		PendingToolCallIds.Reset();
		// Re-run inference (turn 2) to fold the tool output into a spoken reply.
		RunInference(ActiveOptions ? ActiveOptions : Options, FString());
	}
}

void UACELLMComponent::CompleteResponse(ULLMChatMessage* ResponseMessage)
{
	bBusy = false;
	ActiveOptions = nullptr;

	const FString FullResponse = ResponseMessage ? ResponseMessage->GetContent() : FString();
	OnResponseComplete.Broadcast(FullResponse, ResponseMessage);
}

void UACELLMComponent::ResetConversation()
{
	if (Chat)
	{
		Chat->DeleteAllMessages();
		if (!SystemPrompt.IsEmpty())
		{
			Chat->MakeNewMessage(this, ELLMChatRole::System, SystemPrompt);
		}
	}
	OnConversationReset.Broadcast();
}

FACELLMRuntimeInfo UACELLMComponent::GetRuntimeInfo() const
{
	FACELLMRuntimeInfo Info;
	if (CachedSubsystem)
	{
		Info.ModelPath = CachedSubsystem->GetLoadedModelPath();
		Info.MaxContextSize = CachedSubsystem->GetLoadedContextSize();
		Info.bIsReady = CachedSubsystem->IsEngineReady();
		Info.bIsInitializing = CachedSubsystem->IsEngineInitializing();
	}
	Info.bIsBusy = bBusy;
	Info.MessageCount = Chat ? Chat->GetMessageCount() : 0;
	return Info;
}

void UACELLMComponent::ShutdownLLM()
{
	if (UACELLMSubsystem* Subsystem = CachedSubsystem)
	{
		if (bInitRequested)
		{
			Subsystem->ReleaseEngine(bDestroyEngineOnEndPlay);
		}
	}

	bInitRequested = false;
	bComponentReady = false;
	bBusy = false;
	OutstandingToolResults = 0;
	Chat = nullptr;
	Options = nullptr;
	ActiveOptions = nullptr;
}
