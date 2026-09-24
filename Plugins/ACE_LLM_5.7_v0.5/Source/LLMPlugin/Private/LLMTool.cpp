// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMTool.h"
#include "LLMModule.h"
#include "Modules/ModuleManager.h"
#include "LLMUtilities.h"

ULLMTool::ULLMTool() = default;

ULLMTool* ULLMTool::CreateTool(UObject* WorldContextObject,
	                           const FString& InName,
	                           const FString& InDescription,
	                           const FString& InParametersSchemaJson)
{
	LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());
	if (Module.GetContext() == nullptr)
	{
		UE_LOG(LogTemp, Display, TEXT("ULLMChat::Create LLM Engine not initialized"));
		return nullptr;
	}

	// Make Native Object
	ACETool* tool{};
	if (ace_createTool(Module.GetContext(), &tool) != ACEResultOk)
	{
		return nullptr;
	}

	// Then make UE wrapper
	UObject* Outer = WorldContextObject;
	ULLMTool* Tool = NewObject<ULLMTool>(Outer);
	Tool->Name = InName;
	Tool->Description = InDescription;
	Tool->ParametersSchemaJson = InParametersSchemaJson;

	// Do String conversions
	FStringToNullTerminatedUtf8(Tool->Name, Tool->NameUtf8);
	FStringToNullTerminatedUtf8(Tool->Description, Tool->DescriptionUtf8);
	FStringToNullTerminatedUtf8(Tool->ParametersSchemaJson, Tool->ParametersSchemaUtf8);

	// Set underlying native object params
	if (ace_toolSetName(tool, Tool->NameUtf8.GetData()) != ACEResultOk)
	{
		Tool = nullptr;
		ace_destroyTool(tool);
		return nullptr;
	}

	if (ace_toolSetDescription(tool, Tool->DescriptionUtf8.GetData()) != ACEResultOk)
	{
		Tool = nullptr;
		ace_destroyTool(tool);
		return nullptr;
	}

	if (ace_toolSetParameters(tool, Tool->ParametersSchemaUtf8.GetData()) != ACEResultOk)
	{
		Tool = nullptr;
		ace_destroyTool(tool);
		return nullptr;
	}

	// Map native obj to UE obj
	Tool->m_nativeObject = tool;

	return Tool;
}
