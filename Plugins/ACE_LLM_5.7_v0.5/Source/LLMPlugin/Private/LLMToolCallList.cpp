// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMToolCallList.h"
#include "LLMToolCall.h"
#include "LLMChatMessage.h"
#include "ace.h"

ULLMToolCallList::ULLMToolCallList() = default;

ULLMToolCallList* ULLMToolCallList::CreateToolCallList(UObject* WorldContextObject, ULLMChatMessage* Parent)
{
	if (Parent == nullptr)
	{
		return nullptr;
	}

	if (Parent->GetNativeObject() == nullptr)
	{
		return nullptr;
	}

	UObject* Outer = WorldContextObject;
	ULLMToolCallList* List = NewObject<ULLMToolCallList>(Outer);

	List->m_parentMessage = Parent;

	ACEToolCalls*   nativeToolCalls{};
	ace_chatMessageGetToolCalls(Parent->GetNativeObject(), &nativeToolCalls);
	List->m_nativeObject = nativeToolCalls;

	size_t          toolCallCount{};
	ace_toolCallsGetCount(nativeToolCalls, &toolCallCount);

	for (int i = 0; i < toolCallCount; ++i)
	{
		ACEToolCall* currentTool{};
		const char*  id{};
		const char*  name{};
		const char*  arguments{};

		ace_toolCallsGetCall(nativeToolCalls, i, &currentTool);
		ace_toolCallGetId(currentTool, &id);
		ace_toolCallGetName(currentTool, &name);
		ace_toolCallGetArguments(currentTool, &arguments);

		ULLMToolCall* ueWrapper = ULLMToolCall::CreateToolCall(WorldContextObject, List, currentTool, FString(id), FString(name), FString(arguments));
		List->m_toolCalls.Add(ueWrapper);
	}

	return List;
}

int32 ULLMToolCallList::GetCount() const
{
	if (m_nativeObject == nullptr)
	{
		return 0;
	}

	return m_toolCalls.Num();
}

ULLMToolCall* ULLMToolCallList::GetToolCall(int32 Index)
{
	if (m_nativeObject == nullptr || Index < 0)
	{
		return nullptr;
	}

	return m_toolCalls[Index];
}

ULLMToolCall* ULLMToolCallList::AddToolCall(UObject* WorldContextObject,
	                                        FString Id,
	                                        FString Name,
	                                        FString Arguments)
{
	if (m_nativeObject == nullptr)
	{
		return nullptr;
	}

	ULLMToolCall* ueWrapper = ULLMToolCall::CreateToolCall(WorldContextObject, this, nullptr, Id, Name, Arguments);

	return ueWrapper;
}

bool ULLMToolCallList::DeleteToolCallAt(int32 Index)
{
	if (m_nativeObject == nullptr || Index < 0)
	{
		return false;
	}

	return ace_toolCallsDeleteCall(m_nativeObject, static_cast<size_t>(Index)) == ACEResultOk;
}

bool ULLMToolCallList::ClearToolCalls()
{
	if (m_nativeObject == nullptr)
	{
		return false;
	}

	return ace_toolCallsClear(m_nativeObject) == ACEResultOk;
}
