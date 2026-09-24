// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "LLMToolCall.generated.h"

struct ACEToolCall;
struct ACEToolCalls;
class ULLMToolCallList;

UCLASS(BlueprintType)
class ULLMToolCall : public UObject
{
	GENERATED_BODY()

public:
	ULLMToolCall();

	static ULLMToolCall* CreateToolCall(UObject* WorldContextObject,
		                                ULLMToolCallList* ParentToolCallsList,
		                                ACEToolCall* nativeObject,
		                                FString Id,
		                                FString Name,
		                                FString Arguments);

	ACEToolCall* GetNativeObject() const { return m_nativeObject; }

	bool SetNativeObject(ACEToolCall* toolCall) { m_nativeObject = toolCall; }

	FString Id;
	FString Name;
	FString Arguments;

private:
	ACEToolCall* m_nativeObject{};

	mutable TArray<char> IdUtf8;
	mutable TArray<char> NameUtf8;
	mutable TArray<char> ArgumentsUtf8;
};
