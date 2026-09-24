// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LLMTypes.h"
#include "ace.h"
#include <string>
#include "LLMChatMessage.generated.h"

class ULLMToolCallList;
struct ACEChatMessage;

UCLASS(BlueprintType)
class ULLMChatMessage : public UObject
{
	GENERATED_BODY()

public:

	// Role and Content can only be assigned at creation (this function)
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", Category = "ACE LLM|Functions", DisplayName = "Create Chat Message"))
	static ULLMChatMessage* CreateChatMessage(UObject* WorldContextObject,
		                                      ELLMChatRole Role,
		                                      FString Content);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", Category = "ACE LLM|Functions", DisplayName = "Get Tool Call List"))
	ULLMToolCallList* GetToolCallList(UObject* WorldContextObject);

	bool BuildToolCallList(UObject* WorldContextObject);

	// LLM Role
	ELLMChatRole GetRole() { return m_role; }

	// LLM Content
	FString GetContent() { return m_content; }

	void SetNativeObject(ACEChatMessage* Message) { m_nativeObject = Message; }

	ACEChatMessage* GetNativeObject() { return m_nativeObject; }

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Debug Print Message - Messages Only", WorldContext = "WorldContextObject"))
	void DebugPrintMessage(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Debug Print Message - Tool Calls Only", WorldContext = "WorldContextObject"))
	void DebugPrintToolCalls(UObject* WorldContextObject);

protected:
	ELLMChatRole      m_role = ELLMChatRole::User;
	FString           m_content{};
	ACEChatMessage*   m_nativeObject{};
	ULLMToolCallList* m_toolCallList{};
};
