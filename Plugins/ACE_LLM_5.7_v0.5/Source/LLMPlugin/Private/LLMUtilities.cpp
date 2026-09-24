// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "LLMUtilities.h"
#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#endif
#include "Misc/Paths.h"

bool ULLMUtilities::SelectModelToLoad(FString& OutModelFile)
{
    OutModelFile.Reset();

#if WITH_EDITOR
    IDesktopPlatform* const DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        return false;
    }

    TArray<FString> OutFilenames;
    const bool bSelected = DesktopPlatform->OpenFileDialog(
        nullptr,
        TEXT("Open Model File"),
        FPaths::ProjectContentDir(),
        TEXT(""),
        TEXT("All Files (*.*)|*.*"),
        0u,
        OutFilenames
    );

    if (!bSelected || OutFilenames.Num() == 0)
    {
        return false;
    }

    const FString& SelectedPath = OutFilenames[0];
    if (!FPaths::FileExists(SelectedPath))
    {
        return false;
    }

    OutModelFile = SelectedPath;
    return true;
#else
    // The native file-open dialog is provided by the editor-only DesktopPlatform
    // module, which is not available in monolithic Shipping/Game builds. Interactive
    // model selection is an editor authoring convenience only.
    return false;
#endif
}