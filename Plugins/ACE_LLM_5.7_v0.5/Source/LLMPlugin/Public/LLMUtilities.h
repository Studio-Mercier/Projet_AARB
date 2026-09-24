// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "CoreMinimal.h"
#include "ace.h"
#include "LLMUtilities.generated.h"

namespace
{
	void FStringToNullTerminatedUtf8(const FString& Src, TArray<char>& OutUtf8)
	{
		if (Src.IsEmpty())
		{
			OutUtf8.Empty();
			return;
		}

		FTCHARToUTF8 Utf8(*Src);
		const int32 Len = Utf8.Length();
		OutUtf8.SetNumUninitialized(Len + 1);
		if (Len > 0)
		{
			FMemory::Memcpy(OutUtf8.GetData(), Utf8.Get(), Len);
		}
		OutUtf8[Len] = '\0';
	}

	const char* Utf8OrEmpty(const FString& Src, const TArray<char>& Utf8Buf)
	{
		return Src.IsEmpty() ? "" : Utf8Buf.GetData();
	}
}

UCLASS(BlueprintType)
class ULLMUtilities : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "ACE LLM|Functions", meta = (DisplayName = "Select Model To Load"))
	static bool SelectModelToLoad(FString& OutModelFile);
};
