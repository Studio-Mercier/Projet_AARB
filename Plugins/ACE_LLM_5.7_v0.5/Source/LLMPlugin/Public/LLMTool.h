// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "ace.h"
#include "LLMTool.generated.h"

struct ACETool;

UCLASS(BlueprintType)
class ULLMTool : public UObject
{
	GENERATED_BODY()

public:
	ULLMTool();

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Create LLM Tool", WorldContext = "WorldContextObject"))
	static ULLMTool* CreateTool(UObject* WorldContextObject,
		                        const FString& InName,
		                        const FString& InDescription,
	                            const FString& InParametersSchemaJson);

	ACETool* GetNativeObject() const { return m_nativeObject; }

	FString Name;
	FString Description;
	FString ParametersSchemaJson;

private:

	mutable ACETool* m_nativeObject{};

	mutable TArray<char> NameUtf8;
	mutable TArray<char> DescriptionUtf8;
	mutable TArray<char> ParametersSchemaUtf8;
};
