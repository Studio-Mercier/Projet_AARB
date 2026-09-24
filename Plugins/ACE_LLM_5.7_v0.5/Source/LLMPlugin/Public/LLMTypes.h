// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LLMTypes.generated.h"

UENUM(BlueprintType)
enum class ELLMChatRole : uint8
{
	User      UMETA(DisplayName = "User"),
	Assistant UMETA(DisplayName = "Assistant"),
	Tool      UMETA(DisplayName = "Tool"),
	System    UMETA(DisplayName = "System"),
	Invalid   UMETA(DisplayName = "Invalid")
};

// Hardware backend used to run inference. The plugin always attempts the
// selected backend first and then automatically fails over to the next one
// down the chain (RTX -> GPU -> CPU) if initialization fails, so a project
// configured for RTX still runs on machines without a CIG-capable GPU.
UENUM(BlueprintType)
enum class EACELLMBackend : uint8
{
	// RTX GPU acceleration via CUDA-in-Graphics (CIG). Hands the D3D12 device a
	// dedicated command queue so inference does not stall the renderer. Falls
	// back to GPU then CPU if unsupported or initialization fails.
	RTX UMETA(DisplayName = "RTX (GPU Accelerated - CIG)"),
	// Standard D3D12 GPU handoff on the engine's main graphics queue (no dedicated
	// CIG queue). Use this if the CIG path misbehaves on a given GPU. Falls back to CPU.
	GPU UMETA(DisplayName = "GPU (D3D12)"),
	// CPU-only inference. No GPU device is handed to the runtime.
	CPU UMETA(DisplayName = "CPU")
};

UENUM(BlueprintType)
enum class ELLMInferenceOptions : uint8
{
    Temperature       UMETA(DisplayName = "Temperature"),
    TopP              UMETA(DisplayName = "TopP"),
    TopK              UMETA(DisplayName = "TopK"),
    MinP              UMETA(DisplayName = "MinP"),
    RepeatPenalty     UMETA(DisplayName = "RepeatPenalty"),
    EnableStreaming   UMETA(DisplayName = "EnableStreaming"),
    EnableThink       UMETA(DisplayName = "EnableThink"),
    OutputThinkTokens UMETA(DisplayName = "OutputThinkTokens")
};

USTRUCT(BlueprintType)
struct FLLMToolDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool", meta = (MultiLine = true))
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool", meta = (MultiLine = true))
	FString ParametersSchemaJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool")
	bool bEnabled = true;
};