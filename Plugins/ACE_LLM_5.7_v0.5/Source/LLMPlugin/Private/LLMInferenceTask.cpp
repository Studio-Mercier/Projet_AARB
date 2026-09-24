// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMInferenceTask.h"
#include "Windows.h"
#include <string>

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#endif
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "LLMChat.h"
#include "LLMModule.h"
#include "LLMInferenceOptions.h"
#include "LLMToolCall.h"
#include "LLMToolCallList.h"
#include "Modules/ModuleManager.h"
// Async(), EAsyncExecution and AsyncTask() live here. Include explicitly so this
// file compiles in non-editor targets (UnrealGame) where the UnrealEd shared PCH
// is not available to provide it transitively.
#include "Async/Async.h"

#define INFINITE_TIME 0xFFFFFFFF

void ace_logfn(ACELogLevel level, const char* msg, void* userData)
{
}

ULLMInferenceTask* ULLMInferenceTask::RunLLMChat(UObject* WorldContextObject, ULLMChat* ChatObj, ULLMInferenceOptions* Options, FString SystemPromptOverride)
{
    LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());

    if (Module.GetContext() == nullptr ||
        Module.GetModel() == nullptr)
    {
        UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::RunACEInference LLM Engine Not Intialized!"));
        return nullptr;
    }

    ULLMInferenceTask* infTask = NewObject<ULLMInferenceTask>();
    infTask->m_worldContextObject = WorldContextObject;
    infTask->m_chatObj = ChatObj;
    infTask->m_infOptions = Options;
    infTask->m_systemPromptOverride = SystemPromptOverride;
    return infTask;
}

void ULLMInferenceTask::Activate()
{
    UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::Activate"));
    if (m_isActivated)
    {
        return;
    }
    m_isActivated = true;

    Super::Activate();

    if (m_worldContextObject)
    {
        // spawn inference thread (producer)
        TFuture<int32> Future1 = Async(EAsyncExecution::Thread, [this] { return RunChatInference(); });

        // spawn output thread (consumer)
        TFuture<int32> Future2 = Async(EAsyncExecution::Thread, [this] { return GetChatInferenceOutputs(); });

        // When both futures are ready, dispatch completion back to the game thread
        Async(EAsyncExecution::ThreadPool, [this, Future1 = MoveTemp(Future1), Future2 = MoveTemp(Future2)]() mutable
        {
            const int32 Result1 = Future1.Get();
            const int32 Result2 = Future2.Get();

            // Must broadcast delegates on the game thread
            AsyncTask(ENamedThreads::GameThread, [this, Result1, Result2]()
            {
                OnBothThreadsComplete(Result1, Result2);
            });
        });
    }
}

void ULLMInferenceTask::OnBothThreadsComplete(int32 Result1, int32 Result2)
{
    UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::OnBothThreadsComplete"));

    if (OnGeneratedResponse.IsBound())
    {
        OnGeneratedResponse.Broadcast();
    }

    SetReadyToDestroy();
}

int32 ULLMInferenceTask::RunChatInference()
{
    LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());

    if (Module.GetContext() == nullptr ||
        Module.GetModel() == nullptr)
    {
        UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::RunACEInference LLM Engine Not Intialized!"));
        return 1;
    }

    ACEChatMessage* assistantEntry = nullptr;
    ACEResult       chatRes = ace_Model_Chat(Module.GetModel(),
                                             m_chatObj->GetNativeObject(),
                                             m_infOptions->GetNativeObject(),
                                             &assistantEntry);
    if (chatRes != ACEResultOk)
    {
        UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::RunChatInference Failed! Code %i"), chatRes);
        return 1;
    }

    TWeakObjectPtr<ULLMInferenceTask> weak_this(this);
    if (weak_this.Pin() == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("ULLMInferenceTask: error in weak_this ptr"));
        return 1;
    }

    // Role and Content
    ACEChatRole responseRole{};
    const char* responseContent{};
    ace_chatMessageGetRole(assistantEntry, &responseRole);
    ace_chatMessageGetContent(assistantEntry, &responseContent);

    // Create the wrapper object and also populate the tool call list under the hood
    ULLMChatMessage* assistantMessage = ULLMChatMessage::CreateChatMessage(m_worldContextObject,
                                                                           (ELLMChatRole)responseRole,
                                                                           FString(responseContent));
    assistantMessage->SetNativeObject(assistantEntry);
    assistantMessage->BuildToolCallList(m_worldContextObject);

    m_chatObj->SetLatestResponse(assistantMessage);

    if (m_chatObj->IsChatHistoryEnabled())
    {
        m_chatObj->AddMessage(assistantMessage);
    }

    return 1;
}

int32 ULLMInferenceTask::GetChatInferenceOutputs()
{
    LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());

    if (Module.GetContext() == nullptr ||
        Module.GetModel() == nullptr)
    {
        UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::RunACEInference LLM Engine Not Intialized!"));
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::GetChatInferenceOutputs"));
    TWeakObjectPtr<ULLMInferenceTask> weak_this(this);
    if (weak_this.Pin() == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("ULLMInferenceTask: error in weak_this ptr"));
        return 1;
    }

    bool done = false;
    ACEResult slmRes = ACEResultOk;
    std::string slmResult{};
    ACETokenEvent tokenEvent;
    FString token;
    FString tokenString; // all tokens so produced so far
    while (!done)
    {
        slmRes = ace_Model_GetNextEvent(Module.GetModel(), &tokenEvent, INFINITE_TIME);

        if (slmRes == ACEResultOk)
        {
            switch (tokenEvent.eventType)
            {
            case ACEEventType_Begin:
                slmResult = "";
                break;

            case ACEEventType_Data:
                slmResult += tokenEvent.tokenData;
                token = UTF8_TO_TCHAR(tokenEvent.tokenData);
                tokenString = UTF8_TO_TCHAR(slmResult.c_str());
                AsyncTask(ENamedThreads::GameThread, [weak_this, token, tokenString]
                    {
                        if (weak_this.IsValid())
                        {
                            weak_this->OnTokenOutput.Broadcast(token, tokenString);
                        }
                    });
                break;

            case ACEEventType_End:
                done = true;
                break;

            default:
                break;
            }
        }
        else if (slmRes == ACEResultWaitTimeout)
        {
            continue;
        }
        else
        {
            done = true;
        }
    }

    return 0;
}
