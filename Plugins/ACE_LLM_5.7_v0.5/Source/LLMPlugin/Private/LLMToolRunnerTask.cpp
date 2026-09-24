// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMToolRunnerTask.h"

#include "LLMToolCallList.h"
#include "LLMToolCall.h"
#include "LLMChat.h"
#include "LLMChatMessage.h"
#include "LLMModule.h"
#include "Modules/ModuleManager.h"

ULLMToolRunnerTask* ULLMToolRunnerTask::RunTools(UObject* WorldContextObject, ULLMChat* ChatObject)
{
    LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());

    if (Module.GetContext() == nullptr ||
        Module.GetModel() == nullptr)
    {
        UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::RunACEInference LLM Engine Not Intialized!"));
        return nullptr;
    }

    if (ChatObject)
    {
        ULLMChatMessage* msg = ChatObject->GetLatestResponse();
        if (msg)
        {
            ULLMToolCallList* toolList = msg->GetToolCallList(WorldContextObject);
            if (toolList)
            {
                ULLMToolRunnerTask* toolTask = NewObject<ULLMToolRunnerTask>();
                toolTask->m_worldContextObject = WorldContextObject;
                toolTask->m_toolCallList = toolList;
                return toolTask;
            }
        }
    }

    return nullptr;
}

void ULLMToolRunnerTask::Activate()
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
        int32 count = m_toolCallList->GetCount();

        for (int i = 0; i < count; ++i)
        {
            ULLMToolCall* currentToolCall = m_toolCallList->GetToolCall(i);

            OnToolCall.Broadcast(currentToolCall->Name, currentToolCall->Id, currentToolCall->Arguments);
        }
    }
}