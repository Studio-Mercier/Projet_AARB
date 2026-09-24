// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMChatMessage.h"
#include "LLMToolCallList.h"

ULLMChatMessage* ULLMChatMessage::CreateChatMessage(UObject* WorldContextObject,
	                                                ELLMChatRole Role,
	                                                FString Content)
{
	ULLMChatMessage* chatEntry = NewObject<ULLMChatMessage>();
	chatEntry->m_role = Role;
	chatEntry->m_content = Content;

	return chatEntry;
}

ULLMToolCallList* ULLMChatMessage::GetToolCallList(UObject* WorldContextObject)
{
	if (m_nativeObject == nullptr)
		return nullptr;

	if (m_toolCallList == nullptr)
	{
		if (BuildToolCallList(WorldContextObject))
			return m_toolCallList;
		else
			return nullptr;
	}

	return m_toolCallList;
}

bool ULLMChatMessage::BuildToolCallList(UObject* WorldContextObject)
{
	if (m_toolCallList == nullptr)
	{
		ACEToolCalls* nativeToolCalls{};
		size_t          toolCallCount{};

		if (ace_chatMessageGetToolCalls(m_nativeObject, &nativeToolCalls) != ACEResultOk)
		{
			UE_LOG(LogTemp, Display, TEXT("ULLMChatMessage::BuildToolCallList Error in ace_chatMessageGetToolCalls"));
			return false;
		}

		if (ace_toolCallsGetCount(nativeToolCalls, &toolCallCount) != ACEResultOk)
		{
			UE_LOG(LogTemp, Display, TEXT("ULLMChatMessage::BuildToolCallList Error in ace_toolCallsGetCount"));
			return false;
		}

		if (toolCallCount)
		{
			m_toolCallList = ULLMToolCallList::CreateToolCallList(WorldContextObject, this);
			return true;
		}
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("ULLMChatMessage::BuildToolCallList Warning tool call list already built"));
	return false;
}

void ULLMChatMessage::DebugPrintMessage(UObject* WorldContextObject)
{
	if (m_nativeObject == nullptr)
	{
		return;
	}

	const char* roleStr{};
	switch (m_role)
	{
	case ELLMChatRole::User:
		roleStr = "(user)";
		break;
	case ELLMChatRole::Assistant:
		roleStr = "(assistant)";
		break;
	case ELLMChatRole::Tool:
		roleStr = "(tool)";
		break;
	case ELLMChatRole::System:
		roleStr = "(system)";
		break;
	case ELLMChatRole::Invalid:
		roleStr = "(invalid)";
		break;
	}

	UE_LOG(LogTemp, Display, TEXT("Chat Element %hs: %s"), roleStr, *m_content);
}

void ULLMChatMessage::DebugPrintToolCalls(UObject* WorldContextObject)
{
	if (m_nativeObject)
	{
		if (!m_toolCallList)
		{
			GetToolCallList(WorldContextObject);
		}

		ACEToolCalls* toolCalls{};
		size_t        toolCallCount{};
		ACEToolCall* currentCall{};

		if (ace_chatMessageGetToolCalls(m_nativeObject, &toolCalls) != ACEResultOk)
		{
			UE_LOG(LogTemp, Display, TEXT("ULLMInferenceTask::RunChatInference Error Parsing Tool Calls"));
			return;
		}

		const char* content{};
		ace_toolCallsGetCount(toolCalls, &toolCallCount);

		for (int i = 0; i < toolCallCount; ++i)
		{
			const char* id{};
			const char* name{};
			const char* arguments{};
			ace_toolCallsGetCall(toolCalls, i, &currentCall);
			ace_toolCallGetId(currentCall, &id);
			ace_toolCallGetName(currentCall, &name);
			ace_toolCallGetArguments(currentCall, &arguments);

			UE_LOG(LogTemp, Display, TEXT("Chat Element Tool Call %i"), i);
			UE_LOG(LogTemp, Display, TEXT("Tool Call Name: %hs"), name);
			UE_LOG(LogTemp, Display, TEXT("Tool Call ID: %hs"), id);
			UE_LOG(LogTemp, Display, TEXT("Tool Call Arguments: %hs"), arguments);
		}
	}
}
