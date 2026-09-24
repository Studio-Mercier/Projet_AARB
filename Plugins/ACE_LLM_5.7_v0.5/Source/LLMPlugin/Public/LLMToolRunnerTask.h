// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/NoExportTypes.h"
#include "ace.h"

#include "LLMToolRunnerTask.generated.h"

class ULLMChat;
class ULLMToolCallList;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FToolCallOutput, FString, ToolName, FString, ToolId, FString, ToolArguments);

UCLASS(BlueprintType)
class ULLMToolRunnerTask : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

    UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Run LLM Tools", WorldContext = "WorldContextObject"))
    static ULLMToolRunnerTask* RunTools(UObject* WorldContextObject, ULLMChat* ChatObject);

    UPROPERTY(BlueprintAssignable)
    FToolCallOutput OnToolCall;

    virtual void Activate() override;

private:

    UPROPERTY()
    UObject*          m_worldContextObject;
    ULLMToolCallList* m_toolCallList{};
    bool              m_isActivated = false;
};
