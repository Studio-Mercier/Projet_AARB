// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "CoreMinimal.h"
#include "ace.h"
#include "LLMInferenceOptions.h"
#include "LLMEngine.generated.h"

struct ID3D12Device;
struct ID3D12CommandQueue;

UCLASS(BlueprintType)
class ULLMEngine : public UObject
{
	GENERATED_BODY()

public:
	ULLMEngine();


	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Create LLM Engine", WorldContext = "WorldContextObject"))
	static bool CreateLLMEngine(UObject* WorldContextObject,
                                FString ModelFilename,
                                int     MaxContextSize);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Destroy LLM Engine"))
	static bool DestroyLLMEngine();

private:
#if PLATFORM_WINDOWS
	static ID3D12Device*       GetD3D12Device(int devIndex);
	static ID3D12CommandQueue* GetD3D12Queue();
#endif
};