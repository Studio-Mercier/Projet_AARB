// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMChat.h"

#include "LLMModule.h"
#include "LLMSettings.h"
#include "LLMTool.h"

#include "Modules/ModuleManager.h"

ULLMChat::ULLMChat() = default;

ULLMChat* ULLMChat::CreateChatObject(UObject* WorldContextObject, int MaxSize, bool SaveChatHistory)
{
	UE_LOG(LogACELLM, Log,
		TEXT("[Chat] CreateChatObject(MaxSize=%d, SaveChatHistory=%s, Outer=%s)"),
		MaxSize,
		SaveChatHistory ? TEXT("true") : TEXT("false"),
		WorldContextObject ? *WorldContextObject->GetName() : TEXT("<null>"));

	LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());
	if (Module.GetContext() == nullptr)
	{
		UE_LOG(LogACELLM, Error,
			TEXT("[Chat] CreateChatObject FAILED: ACEContext is null. Call CreateLLMEngine before CreateChatObject."));
		return nullptr;
	}
	if (Module.GetModel() == nullptr)
	{
		UE_LOG(LogACELLM, Error,
			TEXT("[Chat] CreateChatObject FAILED: ACEModel is null. CreateLLMEngine did not finish successfully."));
		return nullptr;
	}

	// first make native OBJ (ACE SDK)
	ACEChat* nativeObj = nullptr;
	const ACEResult ChatRes = ace_createChat(Module.GetContext(), &nativeObj);
	if (ChatRes != ACEResultOk || !nativeObj)
	{
		UE_LOG(LogACELLM, Error,
			TEXT("[Chat] CreateChatObject FAILED: ace_createChat returned %u (%s)"),
			ChatRes, ACEResultToString(ChatRes));
		return nullptr;
	}
	UE_LOG(LogACELLM, Verbose, TEXT("[Chat] ace_createChat OK (native=%p)"), nativeObj);

	// then make UE wrapper OBJ
	UObject* Outer = WorldContextObject;
	ULLMChat* Obj = NewObject<ULLMChat>(Outer);
	Obj->m_nativeObject = nativeObj;
	Obj->m_maxSize = (MaxSize <= 0 ? INT_MAX : MaxSize);
	Obj->m_saveChatHistory = SaveChatHistory;

	int32 ToolsConsidered = 0;
	int32 ToolsRegistered = 0;
	const ULLMSettings* Settings = GetDefault<ULLMSettings>();
	if (Settings && Settings->bAutoRegisterTools)
	{
		for (const FLLMToolDefinition& Def : Settings->RegisteredTools)
		{
			++ToolsConsidered;
			if (!Def.bEnabled)
			{
				continue;
			}
			if (Def.Name.IsEmpty())
			{
				UE_LOG(LogACELLM, Warning,
					TEXT("[Chat] CreateChatObject skipping RegisteredTools entry with empty Name"));
				continue;
			}

			const int32 ToolCountBefore = Obj->m_toolArray.Num();
			ULLMTool* Tool = ULLMTool::CreateTool(WorldContextObject, Def.Name, Def.Description, Def.ParametersSchemaJson);
			// AddTool is idempotent and returns true even when it skips a duplicate,
			// so only count an entry as newly registered when the tool array actually grew.
			if (Tool && Obj->AddTool(Tool) && Obj->m_toolArray.Num() > ToolCountBefore)
			{
				++ToolsRegistered;
			}
		}
		UE_LOG(LogACELLM, Log,
			TEXT("[Chat] Auto-registered tools: %d of %d enabled entries (RegisteredTools.Num=%d)"),
			ToolsRegistered, ToolsConsidered, Settings->RegisteredTools.Num());
	}
	else
	{
		UE_LOG(LogACELLM, Verbose,
			TEXT("[Chat] Auto-register skipped (Settings=%p, bAutoRegisterTools=%s)"),
			Settings,
			(Settings && Settings->bAutoRegisterTools) ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogACELLM, Log,
		TEXT("[Chat] CreateChatObject OK (wrapper=%p, native=%p, maxSize=%d)"),
		Obj, nativeObj, Obj->m_maxSize);
	return Obj;
}

ULLMChatMessage* ULLMChat::MakeNewMessage(UObject* WorldContextObject, ELLMChatRole Role, FString Content)
{
	if (!m_nativeObject)
	{
		return nullptr;
	}

	// Dont add more than one system role to the chat object
	if (Role == ELLMChatRole::System)
	{
		for (auto msg : m_msgArray)
		{
			if (msg->GetRole() == ELLMChatRole::System)
			{
				UE_LOG(LogTemp, Display, TEXT("ULLMChat::AddMessage WARNING Cannot add another system role to the LLMChat Object"));
				return nullptr;
			}
		}
	}

	// first make native OBJ (ACE SDK)
	ACEChatMessage* nativeMsg = nullptr;
	if (ace_chatAdd(m_nativeObject, &nativeMsg) != ACEResultOk || nativeMsg == nullptr)
	{
		UE_LOG(LogTemp, Display, TEXT("ULLMChat::AddMessage Could not make native chat object"));
		return nullptr;
	}

	ace_chatMessageSetRole(nativeMsg, (ACEChatRole)Role);
	auto content_Converted = StringCast<UTF8CHAR>(*Content);
	ace_chatMessageSetContent(nativeMsg, reinterpret_cast<const char*>(content_Converted.Get()));

	// then make UE wrapper around native OBJ
	ULLMChatMessage* Message = ULLMChatMessage::CreateChatMessage(WorldContextObject, Role, Content);
	Message->SetNativeObject(nativeMsg);

	// finally add wrapper OBJ to the array
	if (m_maxSize == 1 && m_msgArray.Num())
	{
		DeleteMessage(0);
	}
	else
	while (m_msgArray.Num() >= m_maxSize)
	{
		if (m_msgArray[1] != nullptr)
		{
			DeleteMessage(1);
		}
	}

	m_msgArray.Add(Message);

	return Message;
}

bool ULLMChat::AddMessage(ULLMChatMessage* Message)
{
	if (m_maxSize == 1 && m_msgArray.Num())
	{
		DeleteMessage(0);
	}
	else while (m_msgArray.Num() >= m_maxSize)
	{
		if (m_msgArray[1] != nullptr)
		{
			DeleteMessage(1);
		}
	}

	m_msgArray.Add(Message);
	return true;
}

ULLMChatMessage* ULLMChat::GetMessage(int32 Index)
{
	if (!m_nativeObject || Index < 0 || Index> m_maxSize)
	{
		return nullptr;
	}

	return m_msgArray[Index];
}

void ULLMChat::DeleteMessage(int32 Index)
{
	if (!m_nativeObject || Index < 0 || Index > m_maxSize - 1)
	{
		return;
	}

	// first delete native object
	ACEChatMessage* removed = nullptr;
	if (ace_chatDelete(m_nativeObject, static_cast<size_t>(Index), &removed) == ACEResultOk && removed != nullptr)
	{
		ace_destroyChatMessage(removed);
	}

	// then delete UE wrapper object
	m_msgArray.RemoveAt(Index);
}

void ULLMChat::DeleteAllMessages()
{
	int count = GetMessageCount();

	for (int i = 0; i < count; ++i)
	{
		ACEChatMessage* removed = nullptr;
		if (ace_chatDelete(m_nativeObject, 0, &removed) == ACEResultOk && removed != nullptr)
		{
			ace_destroyChatMessage(removed);
		}
	}

	m_msgArray.Empty();
}

void ULLMChat::SetLatestResponse(ULLMChatMessage* message)
{
	m_latestResponse = message;
}

ELLMChatRole ULLMChat::GetMessageRole(int32 Index)
{
	if (!m_nativeObject || Index < 0 || Index > m_maxSize-1)
	{
		return ELLMChatRole::Invalid;
	}

	return m_msgArray[Index]->GetRole();
}

FString ULLMChat::GetMessageContent(int32 Index)
{
	if (!m_nativeObject || Index < 0 || Index > m_maxSize - 1)
	{
		return FString();
	}

	return m_msgArray[Index]->GetContent();
}

int32 ULLMChat::GetMessageCount() const
{
	if (!m_nativeObject)
	{
		return 0;
	}

	return m_msgArray.Num();
}

bool ULLMChat::AddTool(ULLMTool* tool)
{
	if (!tool)
	{
		return false;
	}

	for (const ULLMTool* Existing : m_toolArray)
	{
		if (Existing && Existing->Name.Equals(tool->Name, ESearchCase::CaseSensitive))
		{
			UE_LOG(LogTemp, Display, TEXT("ULLMChat::AddTool skipping duplicate tool '%s'"), *tool->Name);
			return true;
		}
	}

	if (ace_chatAddTool(m_nativeObject, tool->GetNativeObject()) != ACEResultOk)
	{
		return false;
	}
	m_toolArray.Add(tool);
	return true;
}

ULLMTool* ULLMChat::GetTool(int32 Index)
{
	if (Index >= m_toolArray.Num())
	{
		return nullptr;
	}
	return m_toolArray[Index];
}

bool ULLMChat::DeleteTool(int32 Index)
{
	if (Index >= m_toolArray.Num())
	{
		return false;
	}

	//native object
	if (ace_chatDeleteTool(m_nativeObject, Index) != ACEResultOk)
	{
		return false;
	}

	//UE wrapper
    m_toolArray.RemoveAt(Index);

	return true;
}

void ULLMChat::DebugPrintChatObject(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject)
{
	DebugPrintMessageArray(WorldContextObject, ChatObj, PrintNativeObject);
	DebugPrintToolArray(WorldContextObject, ChatObj, PrintNativeObject);
}

void ULLMChat::DebugPrintMessageArray(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject)
{
	for (int i=0; i < ChatObj->m_msgArray.Num(); ++i)
	{
		ChatObj->m_msgArray[i]->DebugPrintMessage(WorldContextObject);
	}

	if (PrintNativeObject)
	{
		ChatObj->DebugPrintNativeChatObject();
	}
}

void ULLMChat::DebugPrintToolArray(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject)
{
	for (int i = 0; i < ChatObj->m_toolArray.Num(); ++i)
	{
		UE_LOG(LogTemp, Display, TEXT("Tool [%i] Name: %s"), i, *ChatObj->m_toolArray[i]->Name);
		UE_LOG(LogTemp, Display, TEXT("Tool [%i] Desc: %s"), i, *ChatObj->m_toolArray[i]->Description);
		UE_LOG(LogTemp, Display, TEXT("Tool [%i] Params Schema: %s"), i, *ChatObj->m_toolArray[i]->ParametersSchemaJson);
	}

	if (PrintNativeObject)
	{
		ChatObj->DebugPrintNativeToolObjects();
	}
}

void ULLMChat::DebugPrintLatestResponse(UObject* WorldContextObject, ULLMChat* ChatObj, bool PrintNativeObject)
{
	if (!ChatObj)
	{
		UE_LOG(LogTemp, Display, TEXT("ULLMChat::DebugPrintLatestResponse Warning ChatObj is null"));
		return;
	}

	if (!ChatObj->GetLatestResponse())
	{
		UE_LOG(LogTemp, Display, TEXT("ULLMChat::DebugPrintLatestResponse Warning ChatObj->LatestResponse is null"));
	}

	UE_LOG(LogTemp, Display, TEXT("=== ChatObj->LatestResponse ==="));
	ChatObj->GetLatestResponse()->DebugPrintMessage(WorldContextObject);
	ChatObj->GetLatestResponse()->DebugPrintToolCalls(WorldContextObject);
	UE_LOG(LogTemp, Display, TEXT("==============================="));
}

void ULLMChat::DebugPrintNativeChatObject()
{
	int count = GetMessageCount();

	for (int i = 0; i < count; ++i)
	{
		ACEChatMessage* message{};
		const char* contentStr{};
		ACEChatRole role{};
		ace_chatGet(m_nativeObject, i, &message);
		ace_chatMessageGetContent(message, &contentStr);
		ace_chatMessageGetRole(message, &role);
		const char* roleStr{};
		switch (role)
		{
		case ACEChatRole_User:
			roleStr = "(user)";
			break;
		case ACEChatRole_Assistant:
			roleStr = "(assistant)";
			break;
		case ACEChatRole_Tool:
			roleStr = "(tool)";
			break;
		case ACEChatRole_System:
			roleStr = "(system)";
			break;
		}
		UE_LOG(LogTemp, Display, TEXT("Native Chat Element %i %hs: %hs"), i, roleStr, contentStr);
	}
}

void ULLMChat::DebugPrintNativeToolObjects()
{
	int i = 0;
	for (auto iter : m_toolArray)
	{
		const char* name{};
		const char* desc{};
		const char* params{};

		if (ace_toolGetName(iter->GetNativeObject(), &name) != ACEResultOk)
		{
		}
		if (ace_toolGetDescription(iter->GetNativeObject(), &desc) != ACEResultOk)
		{
		}
		if (ace_toolGetParameters(iter->GetNativeObject(), &params) != ACEResultOk)
		{
		}

		UE_LOG(LogTemp, Display, TEXT("Native Tool [%i] Name: %hs"), i, name);
		UE_LOG(LogTemp, Display, TEXT("Native Tool [%i] Desc: %hs"), i, desc);
		UE_LOG(LogTemp, Display, TEXT("Native Tool [%i] Params Schema: %hs"), i++, params);
	}
}

void ULLMChat::BeginDestroy()
{
	// first delete native OBJs
	if (m_nativeObject)
	{
		ace_destroyChat(m_nativeObject);
		m_nativeObject = nullptr;
	}

	// then delete the UE wrapper OBJS
	m_msgArray.Empty();

	m_toolArray.Empty();

	Super::BeginDestroy();
}