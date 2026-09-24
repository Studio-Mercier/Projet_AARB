// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ace.h"
#include "LLMTypes.h"
#include "LLMInferenceOptions.generated.h"

UCLASS(BlueprintType)
class ULLMInferenceOptions : public UObject
{
	GENERATED_BODY()

public:

    /*
     * ===Recommended Default Values for Inference Options===
     * Temperature: 0.2
     * TopP: 0.8
     * TopK: 20
     * MinP: 0.0
     * RepeatPenalty: 1.0f
     * EnableStreaming: true
     * EnableThinkMode: false
     * OutputThinkTokens: false
     */
    UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Create LLM Inference Options", WorldContext = "WorldContextObject", AdvancedDisplay = "Temperature,TopP,TopK,MinP,RepeatPenalty,EnableStreaming,EnableThinkMode,OutputThinkTokens"))
    static ULLMInferenceOptions* CreateLLMInferenceOptions(UObject* WorldContextObject,
                                                           float Temperature       = -1.0f,
                                                           float TopP              = -1.0f,
                                                           int   TopK              = -1,
                                                           float MinP              = -1.0f,
                                                           float RepeatPenalty     = -1.0f,
                                                           bool  EnableStreaming   = true,
                                                           bool  EnableThinkMode   = false,
                                                           bool  OutputThinkTokens = false);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ACE LLM|Functions")
	float GetOption(ELLMInferenceOptions OptionName);

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions")
	void SetOption(ELLMInferenceOptions OptionName, float Value);

	ACEInferenceOptions* GetNativeObject() { return m_nativeOptions; }

	ACEInferenceOptions* m_nativeOptions{};

};
