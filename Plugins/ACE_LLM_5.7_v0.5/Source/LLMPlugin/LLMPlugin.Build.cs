// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

using System;
using System.IO;
using UnrealBuildTool;

public class LLMPlugin : ModuleRules
{
	public LLMPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

        PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
                "UMG",
				"Engine"
            }
			);

		// The Settings module (ISettingsModule) is only available in editor builds and
		// is not provided as a precompiled dependency for monolithic Shipping/Game
		// targets. Depend on it for editor targets only; the Project Settings page
		// registration in LLMModule is guarded with WITH_EDITOR to match.
		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("Settings");
		}

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
                "RHI",
                "Json"
			}
			);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D12RHI");
            AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"DX12"
			});
        }

        DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

        // ============================================
        // Third-party static library configuration
        // ============================================

        string ThirdPartyPath = System.IO.Path.Combine(ModuleDirectory, "..","..", "ThirdParty");

        PublicIncludePaths.Add(System.IO.Path.Combine(ThirdPartyPath, "include"));
        PrivateIncludePaths.Add(System.IO.Path.Combine(ThirdPartyPath, "include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "lib", "Win64", "ACE.SDK.lib"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "lib", "Linux", "ACE.SDK.a"));
        }

        //
        // Package the 3rd party DLLs
        //
        string binFolder = Path.GetFullPath(Path.Combine(ThirdPartyPath, "bin", "Win64"));

        string[] thirdPartyDLLs =
		{
			"ACE.SDK.dll",
            "cig_scheduler_settings.dll",
			"cudart64_12.dll",
			"ggml-base.dll",
			"ggml-cpu.dll",
			"ggml-cuda.dll",
			"ggml.dll",
			"libopenblas.dll",
			"llama.dll",
			"onnxruntime.dll",
            "cublas64_12.dll",
            "cublasLt64_12.dll"
        };

        foreach (string dllName in thirdPartyDLLs)
        {
            string thirdPartyDLLPath = Path.Combine(binFolder, dllName);

            if (File.Exists(thirdPartyDLLPath))
            {
                RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", dllName), thirdPartyDLLPath);
            }
            else
            {
                System.Console.WriteLine("Warning: Third-party DLL not found at: " + thirdPartyDLLPath);
                System.Console.WriteLine("Please place your DLL in: " + Path.GetDirectoryName(thirdPartyDLLPath));
            }
        }

        // Copy Models
        string contentPath = System.IO.Path.Combine(ModuleDirectory, "..", "..", "Content");

        string[] modelFilenames =
		{
            "Qwen3.5-4B-Q4_K_S.gguf"
        };

		foreach (string modelFile in modelFilenames)
		{

			string modelPath = Path.Combine(contentPath, modelFile);

			if (File.Exists(modelPath))
			{
                System.Console.WriteLine("Model file found at: " + modelPath);
                RuntimeDependencies.Add(Path.Combine("$(PluginDir)", "..", "..", "Content", modelFile), modelPath);
			}
			else
			{
				System.Console.WriteLine("Warning: Model not found at: " + modelPath);
			}
		}
    }
}

