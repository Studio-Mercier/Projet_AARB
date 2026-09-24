// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "LLMTypes.h"

#include "LLMSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "LLM Plugin Settings"))
class ULLMSettings : public UObject
{
	GENERATED_BODY()

public:
	ULLMSettings(const FObjectInitializer& obj);

	UPROPERTY(Config, EditAnywhere, Category = "Model", meta = (FilePathFilter = "gguf", DisplayName = "Model File"))
	FFilePath ModelFile;

	// Hardware backend used to run inference. Defaults to RTX (GPU accelerated via
	// CIG). The plugin automatically fails over to the next backend down the chain
	// (RTX -> GPU -> CPU) if the selected one is unsupported or fails to initialize.
	UPROPERTY(Config, EditAnywhere, Category = "Hardware", meta = (DisplayName = "Preferred Backend"))
	EACELLMBackend PreferredBackend = EACELLMBackend::RTX;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TopP;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options", meta = (ClampMin = "0"))
	int32 TopK;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinP;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options", meta = (ClampMin = "0.0"))
	float RepeatPenalty;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options")
	bool EnableStreaming;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options")
	bool EnableThinkMode;

	UPROPERTY(Config, EditAnywhere, Category = "Default Inference Options")
	bool OutputThinkTokens;

	UPROPERTY(Config, EditAnywhere, Category = "Tools", meta = (DisplayName = "Auto-register Tools On Chat Creation"))
	bool bAutoRegisterTools;

	UPROPERTY(Config, EditAnywhere, Category = "Tools", meta = (TitleProperty = "Name"))
	TArray<FLLMToolDefinition> RegisteredTools;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};