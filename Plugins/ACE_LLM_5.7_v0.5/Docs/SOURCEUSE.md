# Building & Using ACE LLM From Source

This document covers the **source-build** path for the NVIDIA ACE LLM plugin — the workflow used by C++ projects, AAA / studio teams, custom engine builds, or anyone modifying the plugin's source. If you only need a drop-in Blueprint plugin, follow the prebuilt path in the main [README](../README.md) instead.

The instructions here assume working knowledge of Unreal Engine project setup.

---

## When You Need This Document

You need a source build (and therefore Visual Studio + the steps below) if **any** of these are true:

- Your `.uproject` declares one or more `"Modules"` entries (i.e. it is a **C++ project**, not Blueprint-only).
- You want to **modify** the plugin's `Source/` files.
- You are packaging a **shipping build** for distribution outside your own machine.
- You are using a **source build of Unreal Engine** (cloned from `EpicGames/UnrealEngine` rather than installed via the Launcher).
- You are integrating ACE LLM into an existing project that **already** uses other source-built plugins.

If none of those apply, copy the compiled `ACE_LLM` folder into your `Plugins/` directory and follow the drop-in path in the main [README](../README.md#quick-start).

---

## Required Tooling

Install in this order. These are standard UE source-build prerequisites — nothing unique to this plugin.

### 1. Visual Studio 2022

[Download Visual Studio Community](https://visualstudio.microsoft.com/downloads/) (or Professional / Enterprise). During install, on the **Workloads** tab, tick **Desktop development with C++**. Under that workload's *Installation details* pane, ensure these individual components are checked:

- **MSVC v143 – VS 2022 C++ x64/x86 build tools** (latest)
- **Windows 11 SDK** (10.0.22621 or newer; 10.0.26100 recommended)
- **C++ CMake tools for Windows**
- **C++ AddressSanitizer**

Optional but recommended:

- **.NET SDK** (UE's tooling uses .NET 6+ for `UnrealBuildTool`)
- **Git for Windows** (for cloning and managing the repo)

Verify by opening *Visual Studio Installer* and confirming the workload checkbox.

### 2. Unreal Engine 5.5+

Either route works:

- **Launcher build (recommended for most users):** Install the Epic Games Launcher and download the engine. No engine compilation needed.
- **Source build:** Clone `https://github.com/EpicGames/UnrealEngine` (requires linking your GitHub account to Epic). Follow Epic's [How to build the Unreal Editor](https://dev.epicgames.com/community/learning/tutorials/k8Ve/unreal-engine-how-to-build-the-unreal-editor) tutorial.

### 3. NVIDIA Driver

Install the latest **GeForce Game Ready** or **Studio** driver for GPU-backed inference. Verify with `nvidia-smi` from PowerShell. An RTX (Turing or newer) card with at least 2 – 3 GB free VRAM is recommended.

---

## Plugin Installation Path (Project vs. Engine)

The ACE LLM plugin installs **at the project level**, not under the engine directory.

| Where to place the plugin folder | Use when |
|---|---|
| `YourProject/Plugins/ACE_LLM/` | **Default. Recommended for almost everyone.** Plugin is versioned with the project, builds with the project, and ships with packaged builds. |
| `Engine/Plugins/ACE_LLM/` | You maintain a custom engine build and want every project sharing that engine to inherit the plugin. Requires rebuilding the engine. |

---

## Source Build — Step by Step

### Scenario A — Add the plugin to a C++ project you already build

1. **Close the editor** (and Visual Studio if it has the project open).
2. From your project root, ensure a `Plugins/` directory exists. Create it if not.
3. Clone or copy the `ACE_LLM` repo into `Plugins/`:
   ```powershell
   cd C:\Path\To\YourProject\Plugins
   git clone <your-fork-or-mirror-url> ACE_LLM
   ```
   The resulting layout must be:
   ```
   YourProject/
     YourProject.uproject
     Source/
     Plugins/
       ACE_LLM/
         LLMPlugin.uplugin
         Source/
         ThirdParty/
   ```
4. **Regenerate Visual Studio project files** — right-click `YourProject.uproject` in Explorer -> *Generate Visual Studio project files*. (If the entry is missing, run *Epic Games Launcher -> Engine version dropdown -> Verify*, or use `UnrealVersionSelector.exe`.)
5. Open `YourProject.sln` in Visual Studio.
6. Set the configuration to **Development Editor | Win64** and the startup project to your project's `Editor` target.
7. **Build** (`Ctrl+Shift+B`). On a fresh checkout this compiles `LLMPlugin` alongside your game module.
8. Launch the editor from VS (F5) or by reopening the `.uproject` from the Launcher.

### Scenario B — Drop the plugin into a Blueprint-only project

If your project is currently Blueprint-only and you add the plugin's `Source/` folder, Unreal will detect the unresolved C++ modules on first launch and prompt you to compile them.

1. Install Visual Studio 2022 with the **Desktop development with C++** workload (above).
2. Place the plugin under `Plugins/ACE_LLM/` as in Scenario A.
3. Open the `.uproject`. Click **Yes** when prompted to rebuild.
4. After compilation succeeds, the project remains Blueprint-only at the asset level but now has compiled binaries under `Binaries/Win64/` and `Plugins/ACE_LLM/Binaries/Win64/`.

If the rebuild prompt fails (common on machines without VS), Unreal will warn:
```
Plugin 'LLMPlugin' failed to load because module 'LLMPlugin' could not be found.
```
Install Visual Studio, then run *Right-click `.uproject` -> Generate Visual Studio project files* and build manually as in Scenario A.

---

## Linking ACE LLM Into Your C++ Module

Once the plugin compiles, expose its API to your own module by adding `"LLMPlugin"` to `PublicDependencyModuleNames` (or `PrivateDependencyModuleNames` if you only consume it from `.cpp` files):

```csharp
// YourGame/Source/YourGame/YourGame.Build.cs
public class YourGame : ModuleRules
{
    public YourGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "LLMPlugin",   // <-- add this
        });
    }
}
```

Then include the relevant headers from any of your translation units:

```cpp
#include "LLMEngine.h"
#include "LLMChat.h"
#include "LLMInferenceOptions.h"
#include "LLMInferenceTask.h"
#include "LLMTypes.h"
```

See [APIGUIDE.md](APIGUIDE.md) for the full type-by-type C++ reference.

---

## Third-Party Dependency Overview

`LLMPlugin.Build.cs` links and stages the following third-party binaries automatically. You do not need to configure these manually.

### Static library (linked at compile time)

| File | Location | Notes |
|---|---|---|
| `ACE.SDK.lib` | `ThirdParty/lib/Win64/` | ACE SDK import library. Linked via `PublicAdditionalLibraries`. |

### DLLs (staged to output directory at package time)

| DLL | Purpose |
|---|---|
| `ACE.SDK.dll` | NVIDIA ACE SDK runtime — chat, inference, tool-call parsing |
| `llama.dll` | llama.cpp inference backend |
| `ggml.dll` | GGML tensor compute library |
| `ggml-base.dll` | GGML base operations |
| `ggml-cpu.dll` | GGML CPU backend |
| `ggml-cuda.dll` | GGML CUDA backend |
| `cublas64_12.dll` | NVIDIA cuBLAS (CUDA linear algebra) |
| `cublasLt64_12.dll` | NVIDIA cuBLAS-Lt (lightweight CUDA linear algebra) |
| `cudart64_12.dll` | NVIDIA CUDA runtime |
| `libopenblas.dll` | OpenBLAS CPU linear algebra |
| `onnxruntime.dll` | ONNX Runtime |

All DLLs are declared as `RuntimeDependencies` with destination `$(TargetOutputDir)`, which causes them to be copied alongside the game executable in packaged builds.

---

## Packaging a Shipping Build

The `LLMPlugin` module is `"Type": "Runtime"` and `"PlatformAllowList": ["Win64"]`, so it is included in Win64 Shipping and Development packaged builds. There is no editor-only module to exclude.

To package from the command line:

```powershell
cd "C:\Program Files\Epic Games\UE_5.7"
.\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun `
    -project="C:\Path\To\YourProject\YourProject.uproject" `
    -noP4 -platform=Win64 -clientconfig=Shipping `
    -cook -allmaps -build -stage -pak -archive `
    -archivedirectory="C:\Path\To\Output"
```

### Model files in packaged builds

By default, `LLMPlugin.Build.cs` declares `Qwen3.5-4B-Q4_K_S.gguf` (if present in `Content/`) as a `RuntimeDependency` staged to `Content/` beside the executable. To include a different model file:

1. Add it to the plugin's `Content/` folder.
2. Add its filename to the `modelFilenames` array in `LLMPlugin.Build.cs`:
   ```csharp
   string[] modelFilenames =
   {
       "Qwen3.5-4B-Q4_K_S.gguf",
       "YourModel.gguf",   // <-- add here
   };
   ```
3. Rebuild and repackage.

Alternatively, set **Path To Model** in Project Settings to an absolute path that will be present on the target machine (for internal / kiosk deployments where the model lives outside the packaged directory).

Verify the packaged build by launching it on a clean machine (or a VM) without Visual Studio installed.

---

## Source Layout

```
Plugins/ACE_LLM/
  LLMPlugin.uplugin                        <- module manifest (Runtime, Win64)
  Source/LLMPlugin/
    LLMPlugin.Build.cs                     <- declares ThirdParty deps + RuntimeDependencies
    Public/                                <- API headers consumed by other modules
      LLMEngine.h                          <- CreateLLMEngine / DestroyLLMEngine
      LLMChat.h                            <- chat session wrapper
      LLMChatMessage.h                     <- single message wrapper
      LLMInferenceOptions.h                <- sampling + streaming config
      LLMInferenceTask.h                   <- async inference task (RunLLMChat)
      LLMTool.h                            <- tool definition wrapper
      LLMToolCall.h                        <- single tool call wrapper
      LLMToolCallList.h                    <- list of tool calls on a message
      LLMToolRunnerTask.h                  <- async tool dispatch task (RunTools)
      LLMModule.h                          <- module interface (global ACEContext/Model)
      LLMSettings.h                        <- UDeveloperSettings subclass
      LLMTypes.h                           <- ELLMChatRole, ELLMInferenceOptions enums
      LLMUtilities.h                       <- SelectModelToLoad file picker
    Private/                               <- ACE SDK integration + UObject implementations
  ThirdParty/
    include/                               <- ACE SDK headers (ace.h, ace_types.h, ace_result.h)
    lib/Win64/                             <- ACE.SDK.lib import library
    bin/Win64/                             <- runtime DLLs (ACE SDK, llama, ggml, CUDA, ORT)
    symbols/                               <- ACE.SDK.pdb (debug symbols)
  Content/
    PLACE_MODEL_FILES_HERE.txt             <- reminder to place .gguf files here
```

---

## Common Build Issues

| Symptom | Cause | Fix |
|---|---|---|
| `MSB3073: ... UnrealBuildTool ... exited with code 6` | Stale `Intermediate/` after engine version switch | Delete `Binaries/`, `Intermediate/`, and `Plugins/ACE_LLM/Binaries/`, `Plugins/ACE_LLM/Intermediate/`, regenerate project files and rebuild. |
| `error LNK2019: unresolved external symbol` referencing ACE SDK symbols | `ThirdParty/lib/Win64/ACE.SDK.lib` not found or mismatched | Confirm the file exists. If using Git LFS, run `git lfs pull` to download binary assets. |
| `Plugin 'LLMPlugin' failed to load because module 'LLMPlugin' could not be found.` | Module not compiled for the active editor configuration | Rebuild from VS in **Development Editor | Win64**; ensure the project `.target.cs` files exist under `Source/`. |
| `Cannot open include file: 'LLMEngine.h'` from your game module | Missing module dependency | Add `"LLMPlugin"` to your module's `Build.cs` `PublicDependencyModuleNames`. |
| Warnings about missing DLLs at runtime | DLLs not staged or CUDA not installed on target | Confirm `ThirdParty/bin/Win64/` contains all DLLs listed above. On packaged builds, confirm DLLs were copied to the output directory. |
| `Create LLM Engine` returns `false` after a successful build | Model path wrong or ACE context failed to init | Check the Output Log for `LogTemp` errors. Confirm the `.gguf` path is valid. Try an absolute path. |

---

## Reference Links

- Epic — [How to build the Unreal Editor (source build)](https://dev.epicgames.com/community/learning/tutorials/k8Ve/unreal-engine-how-to-build-the-unreal-editor)
- Epic — [Building and packaging Unreal Engine plugins](https://dev.epicgames.com/community/learning/tutorials/Y4yO/epic-games-store-fab-guide-to-building-and-packaging-unreal-engine-plugins)
- Epic — [Plugins overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)
- Epic — [Module Build.cs reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/module-properties-in-unreal-engine)
- NVIDIA — [ACE for Games developer page](https://developer.nvidia.com/ace-for-games)
- This plugin — [APIGUIDE.md](APIGUIDE.md) for the full Blueprint and C++ API reference.

---

## Getting Help

If a build fails after following the steps above:

1. Capture the full Visual Studio *Output* pane (set verbosity to *Detailed* under *Tools -> Options -> Projects and Solutions -> Build And Run*).
2. Capture the editor's `Saved/Logs/<Project>.log` from the most recent launch.
3. Include your Windows version, Visual Studio version, GPU model, driver version, and which scenario (A or B above) you were following.
