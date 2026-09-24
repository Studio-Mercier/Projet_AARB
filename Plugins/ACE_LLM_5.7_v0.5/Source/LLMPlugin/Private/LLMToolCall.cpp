// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMToolCall.h"
#include "LLMToolCallList.h"
#include "ace.h"
#include "LLMUtilities.h"

ULLMToolCall::ULLMToolCall() = default;

ULLMToolCall* ULLMToolCall::CreateToolCall(UObject* WorldContextObject,
	                                       ULLMToolCallList* ParentToolCallsList,
	                                       ACEToolCall* nativeObject,
	                                       FString Id,
	                                       FString Name,
	                                       FString Arguments)
{
	if (ParentToolCallsList == nullptr)
	{
		return nullptr;
	}

	// Make UE wrapper
	UObject* Outer = WorldContextObject;
	ULLMToolCall* ToolCall = NewObject<ULLMToolCall>(Outer);

	ToolCall->Id = Id;
	ToolCall->Name = Name;
	ToolCall->Arguments = Arguments;

	// Convert Strings
	FStringToNullTerminatedUtf8(Id, ToolCall->IdUtf8);
	FStringToNullTerminatedUtf8(Name, ToolCall->NameUtf8);
	FStringToNullTerminatedUtf8(Arguments, ToolCall->ArgumentsUtf8);

	if (nativeObject == nullptr)
	{
		ACEToolCall* newCallNativeObject{};
		ace_toolCallsAddCall(ParentToolCallsList->GetNativeObject(), &newCallNativeObject);
		ace_toolCallSetId(newCallNativeObject, Utf8OrEmpty(ToolCall->Id, ToolCall->IdUtf8));
		ace_toolCallSetName(newCallNativeObject, Utf8OrEmpty(ToolCall->Name, ToolCall->NameUtf8));
		ace_toolCallSetArguments(newCallNativeObject, Utf8OrEmpty(ToolCall->Arguments, ToolCall->ArgumentsUtf8));
	}
	else
	{
		ToolCall->m_nativeObject = nativeObject;
	}

	return ToolCall;
}