// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ace.h"
#include "LLMChatMessage.h"
#include "LLMChat.generated.h"

class ULLMTool;

UCLASS(BlueprintType)
class ULLMChat : public UObject
{
	GENERATED_BODY()

public:
	ULLMChat();

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Create Chat Object", WorldContext = "WorldContextObject"))
	static ULLMChat* CreateChatObject(UObject* WorldContextObject, int MaxSize, bool SaveChatHistory);

	// Message Section
	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (WorldContext = "WorldContextObject"))
	ULLMChatMessage* MakeNewMessage(UObject* WorldContextObject, ELLMChatRole Role, FString Content);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	bool AddMessage(ULLMChatMessage* message);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	ULLMChatMessage* GetMessage(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	void DeleteMessage(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	void DeleteAllMessages();

	void SetLatestResponse(ULLMChatMessage* message);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	ULLMChatMessage* GetLatestResponse() { return m_latestResponse; }

	// Tool Calling
	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	bool AddTool(ULLMTool* Tool);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	ULLMTool* GetTool(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	bool DeleteTool(int32 Index);

	// Getters
	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	ELLMChatRole GetMessageRole(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	FString GetMessageContent(int32 Index);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ACE LLM|Functions")
	int32 GetMessageCount() const;

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	bool IsChatHistoryEnabled() const { return m_saveChatHistory; }

	ACEChat* GetNativeObject() const { return m_nativeObject; }

	// Debugging
	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Debug Print Chat - Entire Chat Object", WorldContext = "WorldContextObject"))
	static void DebugPrintChatObject(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Debug Print Chat - Messages Only", WorldContext = "WorldContextObject"))
	static void DebugPrintMessageArray(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Debug Print Chat - Tools Only", WorldContext = "WorldContextObject"))
	static void DebugPrintToolArray(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Debug Print Chat - Latest LLM Response", WorldContext = "WorldContextObject"))
	static void DebugPrintLatestResponse(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject);

	void DebugPrintNativeChatObject();
	void DebugPrintNativeToolObjects();

	// Cleanup
	virtual void BeginDestroy() override;

protected:
	TArray<ULLMChatMessage*> m_msgArray{};        // Note: always add sytem role before any other roles (user/assistant/tool)
	int                      m_maxSize{};         // Max array size of m_msgArray
	ACEChat*                 m_nativeObject{};    // Underlying ACE object, do not use directly
	bool                     m_saveChatHistory{}; // Should the LLM response be added to this chat object
	TArray<ULLMTool*>        m_toolArray{};       // Array of tools, NOT toolCalls
	ULLMChatMessage*         m_latestResponse{};  // The generated LLM response will be cached here in case its needed later by the app, primarily used for tool calling purposes
};
