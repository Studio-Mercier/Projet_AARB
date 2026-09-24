// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "LLMToolCallList.generated.h"

class ULLMToolCall;
class ULLMChatMessage;
struct ACEToolCalls;

UCLASS(BlueprintType)
class ULLMToolCallList : public UObject
{
	GENERATED_BODY()

public:
	ULLMToolCallList();

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Create LLM Tool Call List", WorldContext = "WorldContextObject"))
	static ULLMToolCallList* CreateToolCallList(UObject* WorldContextObject, ULLMChatMessage* Parent);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Add LLM Tool Call", WorldContext = "WorldContextObject"))
	ULLMToolCall* AddToolCall(UObject* WorldContextObject,
		                      FString Id,
		                      FString Name,
		                      FString Arguments);

	int32 GetCount() const;

	ULLMToolCall* GetToolCall(int32 Index);

	bool DeleteToolCallAt(int32 Index);

	bool ClearToolCalls();

	ACEToolCalls* GetNativeObject() const { return m_nativeObject; }

private:
	ULLMChatMessage*      m_parentMessage{};
	ACEToolCalls*         m_nativeObject{};
	TArray<ULLMToolCall*> m_toolCalls{};
};
