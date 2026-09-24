// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMInferenceOptions.h"

#include "LLMModule.h"
#include "LLMSettings.h"
#include "Modules/ModuleManager.h"

ULLMInferenceOptions* ULLMInferenceOptions::CreateLLMInferenceOptions(UObject* WorldContextObject,
                                                                      float Temperature,
                                                                      float TopP,
                                                                      int   TopK,
                                                                      float MinP,
                                                                      float RepeatPenalty,
                                                                      bool  EnableStreaming,
                                                                      bool  EnableThinkMode,
                                                                      bool  OutputThinkTokens)
{
    LLMModule& Module = FModuleManager::GetModuleChecked<LLMModule>(LLMModule::GetModuleName());
    if (Module.GetContext() == nullptr)
    {
        UE_LOG(LogTemp, Display, TEXT("ULLMInferenceOptions::CreateLLMInferenceOptions LLM Engine not initialized"));
        return nullptr;
    }

    const ULLMSettings* Settings = GetDefault<ULLMSettings>();
    if (Settings)
    {
        if (Temperature   < 0.0f) Temperature   = Settings->Temperature;
        if (TopP          < 0.0f) TopP          = Settings->TopP;
        if (TopK          < 0)    TopK          = Settings->TopK;
        if (MinP          < 0.0f) MinP          = Settings->MinP;
        if (RepeatPenalty < 0.0f) RepeatPenalty = Settings->RepeatPenalty;
    }

    ACEInferenceOptions* nativeOpts{};
    const ACEResult Result = ace_createInferenceOptions(Module.GetContext(), &nativeOpts);
    if (Result != ACEResultOk || nativeOpts == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("ULLMInferenceOptions::CreateLLMInferenceOptions failed to create native options (result=%u)"), Result);
        return nullptr;
    }

    UObject* Outer = WorldContextObject;
    ULLMInferenceOptions* Obj = NewObject<ULLMInferenceOptions>(Outer);
    Obj->m_nativeOptions = nativeOpts;

    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_Temperature, Temperature);
    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_TopP, TopP);
    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_TopK, TopK);
    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_MinP, MinP);
    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_RepeatPenalty, RepeatPenalty);
    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_EnableStreaming, EnableStreaming);
    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_EnableThink, EnableThinkMode);
    ace_setInferenceOptionFloat(nativeOpts, ACEInferenceOption_OutputThinkTokens, OutputThinkTokens);

    return Obj;
}


float ULLMInferenceOptions::GetOption(ELLMInferenceOptions OptionName)
{
    float returnVal{};
    ace_getInferenceOptionFloat(m_nativeOptions, (ACEInferenceOption)OptionName, &returnVal);
    return returnVal;
}

void ULLMInferenceOptions::SetOption(ELLMInferenceOptions OptionName, float Value)
{
    ace_setInferenceOptionFloat(m_nativeOptions, (ACEInferenceOption)OptionName, Value);
}
