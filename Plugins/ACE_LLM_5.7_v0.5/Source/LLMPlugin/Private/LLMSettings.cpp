// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMSettings.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ULLMSettings::ULLMSettings(const FObjectInitializer& obj)
{
	ModelFile.FilePath = TEXT("Qwen3.5-4B-Q4_K_S.gguf");

	PreferredBackend = EACELLMBackend::RTX;

	Temperature       = 0.2f;
	TopP              = 0.8f;
	TopK              = 20;
	MinP              = 0.0f;
	RepeatPenalty     = 1.0f;
	EnableStreaming   = true;
	EnableThinkMode   = false;
	OutputThinkTokens = false;

	bAutoRegisterTools = true;
}

#if WITH_EDITOR
void ULLMSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName   = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (MemberName != GET_MEMBER_NAME_CHECKED(ULLMSettings, RegisteredTools))
	{
		return;
	}

	TSet<FString> SeenNames;
	for (int32 i = 0; i < RegisteredTools.Num(); ++i)
	{
		const FLLMToolDefinition& Def = RegisteredTools[i];

		if (Def.Name.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("LLMSettings: RegisteredTools[%d] has an empty Name; it will be skipped at runtime."), i);
		}
		else if (Def.Name.Contains(TEXT(" ")))
		{
			UE_LOG(LogTemp, Warning, TEXT("LLMSettings: RegisteredTools[%d] Name '%s' contains whitespace."), i, *Def.Name);
		}
		else
		{
			bool bAlreadyIn = false;
			SeenNames.Add(Def.Name, &bAlreadyIn);
			if (bAlreadyIn)
			{
				UE_LOG(LogTemp, Warning, TEXT("LLMSettings: RegisteredTools[%d] duplicates tool Name '%s'."), i, *Def.Name);
			}
		}

		if (!Def.ParametersSchemaJson.IsEmpty())
		{
			TSharedPtr<FJsonObject> Parsed;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Def.ParametersSchemaJson);
			if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("LLMSettings: RegisteredTools[%d] ('%s') ParametersSchemaJson failed to parse as JSON."), i, *Def.Name);
			}
		}
	}
}
#endif
