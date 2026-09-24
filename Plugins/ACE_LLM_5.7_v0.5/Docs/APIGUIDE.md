# ACE LLM — API Reference Guide

Full reference for the **NVIDIA ACE LLM** plugin's Blueprint and C++ API. Pair this guide with the integration recipes in the main [README](../README.md#blueprint-quick-recipes); this document focuses on **what each type / function / parameter does**, not how to wire them together.

All Blueprint nodes live under the **LLM|Functions** category. All C++ types live in the `LLMPlugin` module — add `"LLMPlugin"` to your `Build.cs` `PublicDependencyModuleNames` to consume them. See [SOURCEUSE.md](SOURCEUSE.md).

---

## Table of Contents

- [High-Level Component (recommended)](#high-level-component-recommended)
  - [`UACELLMComponent`](#uacellmcomponent)
  - [`UACELLMSubsystem`](#uacellmsubsystem)
  - [`FACELLMInferenceConfig`](#facellminferenceconfig)
  - [`FACELLMRuntimeInfo`](#facellmruntimeinfo)
  - [Component Events](#component-events)
- [Low-Level Objects](#low-level-objects)
- [Entry Points](#entry-points)
- [`ULLMEngine`](#ullmengine)
- [`ULLMChat`](#ullmchat)
- [`ULLMChatMessage`](#ullmchatmessage)
- [`ULLMInferenceOptions`](#ullminferenceoptions)
- [`ULLMInferenceTask`](#ullminferencetask)
- [`ULLMTool`](#ullmtool)
- [`ULLMToolCallList`](#ullmtoolcalllist)
- [`ULLMToolCall`](#ullmtoolcall)
- [`ULLMToolRunnerTask`](#ullmtoolrunnertask)
- [`ULLMSettings`](#ullmsettings)
- [`ULLMUtilities`](#ullmutilities)
- [Enums](#enums)
- [Delegates](#delegates)
- [Threading & Lifetime Notes](#threading--lifetime-notes)

---

## High-Level Component (recommended)

For most projects the **`UACELLMComponent`** ("ACE LLM" in the component list) is the entry point. Drop it onto an Actor and it composes the low-level objects below for you — the engine handshake, the chat object, inference options, token streaming, and the agentic tool loop. A `UACELLMSubsystem` arbitrates the single shared engine so many components (NPCs) can coexist, each with its own chat history.

| Type | Header | Role |
|---|---|---|
| `UACELLMComponent` | `ACELLMComponent.h` | Drop-in `UActorComponent`. Owns one chat + options; exposes `SendMessage`, `SubmitToolResult`, and Blueprint events. |
| `UACELLMSubsystem` | `ACELLMSubsystem.h` | `UGameInstanceSubsystem` that owns/ref-counts the single global engine (PIE-safe). Created automatically; rarely accessed directly. |
| `FACELLMInferenceConfig` | `ACELLMComponentTypes.h` | Sampling / streaming settings struct surfaced on the component. |
| `FACELLMRuntimeInfo` | `ACELLMComponentTypes.h` | Read-only runtime/engine state snapshot for diagnostics UI. |

### `UACELLMComponent`

`UActorComponent` (ClassGroup **ACE**, BlueprintSpawnable, display name **ACE LLM**) — a self-contained conversational/agentic LLM surface for one Actor. Add it, set a **System Prompt**, bind events, and call **Send Message**.

#### Configuration properties

All properties live under the **NVIDIA ACE | LLM** category in the Details panel.

| Property | Type | Default | Description |
|---|---|---|---|
| `bAutoInitialize` | `bool` | `false` | If `true`, creates/joins the engine during `BeginPlay`. Leave `false` to gate engine load behind a loading screen and call `InitializeLLMAsync` yourself. |
| `ModelFileOverride` | `FFilePath` | *(empty)* | Optional per-component `.gguf` path. Empty uses `ULLMSettings::ModelFile`. The first component to initialize in the process wins (one global engine). |
| `MaxContextSize` | `int32` | `4096` | Token context window passed to `CreateLLMEngine`. Larger uses more VRAM. |
| `bAutoCreateChat` | `bool` | `true` | Auto-builds this component's `ULLMChat` when the engine becomes ready, so `SendMessage` works immediately. |
| `bSaveChatHistory` | `bool` | `true` | Passed to `CreateChatObject`. When `true`, each assistant reply is appended to history for multi-turn memory. |
| `MaxHistoryMessages` | `int32` | `0` | `MaxSize` for `CreateChatObject`. `0` = unlimited. |
| `SystemPrompt` | `FString` | *(empty)* | Seeded as the `System` message when the chat is created and re-applied by `ResetConversation`. |
| `InferenceDefaults` | `FACELLMInferenceConfig` | *(see struct)* | Sampling / streaming options converted once into a cached `ULLMInferenceOptions` and used for every `SendMessage`. |
| `bAutoRunTools` | `bool` | `true` | When `true`, tool calls in a response are auto-dispatched via `OnToolCalled` and inference re-runs after `SubmitToolResult`. |
| `bDestroyEngineOnEndPlay` | `bool` | `false` | `false` keeps the engine warm across PIE for instant reuse; `true` requests teardown on `EndPlay` (a PIE-safe no-op in editor; a real free on session end in a packaged game). |

#### Control functions

| Function | Signature | Notes |
|---|---|---|
| `InitializeLLMAsync` | `void InitializeLLMAsync()` | Idempotent. Registers interest in the shared engine and ensures it is (being) created. Fires `OnLLMReady` when up (immediately if already ready). |
| `IsLLMReady` | `bool IsLLMReady() const` | `true` once the engine is up and (if `bAutoCreateChat`) the chat is built. |
| `IsLLMInitializing` | `bool IsLLMInitializing() const` | `true` while the shared engine is loading. |
| `IsBusy` | `bool IsBusy() const` | `true` while an inference / tool loop is in flight. Guards against overlapping `SendMessage`. |
| `SendMessage` | `void SendMessage(const FString& UserText)` | Appends a `User` message, runs inference with the cached options, streams tokens, and completes via `OnResponseComplete`. |
| `SendMessageWithOptions` | `void SendMessageWithOptions(const FString& UserText, ULLMInferenceOptions* OptionsOverride, const FString& SystemPromptOverride)` | `SendMessage` variant with a one-off options and/or system-prompt override. `OptionsOverride` may be null and `SystemPromptOverride` empty to use the component defaults. |
| `SubmitToolResult` | `void SubmitToolResult(const FString& ToolId, const FString& ResultContent)` | Report a tool's outcome (see below), then auto-re-run inference once all pending tools are answered. |
| `ResetConversation` | `void ResetConversation()` | Clears history and re-seeds `SystemPrompt`. Broadcasts `OnConversationReset`. |
| `RefreshInferenceOptions` | `void RefreshInferenceOptions()` | Rebuilds the cached `ULLMInferenceOptions` from `InferenceDefaults` after changing fields at runtime. |
| `GetChat` | `ULLMChat* GetChat() const` | The component's underlying chat object (for advanced inspection / low-level interop). |
| `GetInferenceOptions` | `ULLMInferenceOptions* GetInferenceOptions() const` | The cached options object built from `InferenceDefaults`. |
| `GetRuntimeInfo` | `FACELLMRuntimeInfo GetRuntimeInfo() const` | A snapshot of engine/component state for diagnostics UI. |
| `ShutdownLLM` | `void ShutdownLLM()` | Releases this component's interest in the shared engine (teardown obeys the PIE-safe rules and `bDestroyEngineOnEndPlay`). Called automatically on `EndPlay`. |

##### `SubmitToolResult` parameters

| Parameter | Description |
|---|---|
| `ToolId` | The Id handed to you by `OnToolCalled`. **Leave empty** to answer the next pending tool call — all a single-tool NPC needs, so you do not have to stash the Id. Pass the explicit Id when multiple tools fire in one turn so each result matches its call. |
| `ResultContent` | What happened when **you** executed the tool. This is your game's own outcome report, authored by you — **not** produced by the LLM and **not** carried from `OnResponseComplete`. A plain sentence (`"The lock clicked open"`) or JSON (`{"success":true}`) both work; the model reads it to compose its reply. Internally this becomes an OpenAI-style tool message `{ "role":"tool", "tool_call_id":<ToolId>, "content":<ResultContent> }`. |

### `UACELLMSubsystem`

`UGameInstanceSubsystem` — the process-wide owner/arbiter of the single global ACE engine (`ACEContext` + `ACEModel`). The ACE/llama/ggml-cuda backend is a process-global that cannot be safely destroyed and re-created within one process, so components must not own the engine lifetime. Components register interest via the subsystem (ref-counted); it creates the engine exactly once and multicasts `OnEngineReady`. The subsystem is created automatically — you rarely call it directly (the component does).

| Member | Signature | Notes |
|---|---|---|
| `IsEngineReady` | `bool IsEngineReady() const` | `true` once the shared engine is loaded. |
| `IsEngineInitializing` | `bool IsEngineInitializing() const` | `true` while the engine is loading. |
| `OnEngineReady` | `FACELLMReadySignature` | Broadcast to all interested components when the engine becomes ready. |
| `OnEngineError` | `FACELLMErrorSignature` | Broadcast when engine initialization fails. |

> The subsystem is per-`GameInstance`, so its ready/ref-count state resets each PIE session, while the underlying engine persists at the module level across PIE — on a fresh PIE session the engine is simply reused and the component becomes ready almost instantly.

### `FACELLMInferenceConfig`

`USTRUCT(BlueprintType)` in `ACELLMComponentTypes.h` — the sampling/streaming settings surfaced as `UACELLMComponent::InferenceDefaults`. Converted once into a cached `ULLMInferenceOptions`. Field semantics match [`ULLMInferenceOptions`](#ullminferenceoptions).

| Field | Type | Default |
|---|---|---|
| `Temperature` | `float` | `0.2` |
| `TopP` | `float` | `0.8` |
| `TopK` | `int32` | `20` |
| `MinP` | `float` | `0.0` |
| `RepeatPenalty` | `float` | `1.0` |
| `EnableStreaming` | `bool` | `true` |
| `EnableThinkMode` | `bool` | `false` |
| `OutputThinkTokens` | `bool` | `false` |

### `FACELLMRuntimeInfo`

`USTRUCT(BlueprintType)` in `ACELLMComponentTypes.h` — a read-only snapshot returned by `GetRuntimeInfo()` for diagnostics UI.

| Field | Type | Description |
|---|---|---|
| `ModelPath` | `FString` | Resolved path of the loaded model. |
| `MaxContextSize` | `int32` | Context window the engine was created with. |
| `bIsReady` | `bool` | Engine is up. |
| `bIsInitializing` | `bool` | Engine is loading. |
| `bIsBusy` | `bool` | An inference / tool loop is in flight. |
| `MessageCount` | `int32` | Number of messages in this component's chat. |

### Component Events

All are `BlueprintAssignable` under **NVIDIA ACE | LLM | Events** and fire on the **game thread**. Delegate types are declared in `ACELLMComponentTypes.h`.

| Event | Parameters | Fires When |
|---|---|---|
| `OnLLMReady` | *(none)* | The engine is up and (if `bAutoCreateChat`) the chat is built — safe to `SendMessage`. |
| `OnLLMError` | `const FString& ErrorMessage` | Engine init failed or an inference could not be started. |
| `OnResponseStarted` | *(none)* | A `SendMessage` inference has begun (good for a "typing…" indicator). |
| `OnTokenStreamed` | `const FString& Token, const FString& PartialText` | Per generated token while streaming is enabled. `PartialText` is the accumulated response. |
| `OnResponseComplete` | `const FString& FullResponse, ULLMChatMessage* ResponseMessage` | Once when a reply is complete — the primary "new reply arrived" hook. |
| `OnToolCalled` | `const FString& ToolName, const FString& ToolId, const FString& ToolArguments` | Once per tool call in the latest response. Run the action, then call `SubmitToolResult`. `ToolArguments` is model-generated JSON — treat it as untrusted input. |
| `OnConversationReset` | *(none)* | After `ResetConversation` clears history. |

---

## Low-Level Objects

The objects below are the manual building blocks the component is built on. Use them directly only when you need full control over lifetime, threading, or the tool loop — otherwise prefer the [component](#high-level-component-recommended) above.


The plugin exposes three cooperative object types that must be used together. There is no single monolithic subsystem — the engine, chat, options, and task objects are created and composed by the caller.

| Object | Header | Role |
|---|---|---|
| `ULLMEngine` | `LLMEngine.h` | Loads the GGUF model and owns the global `ACEContext` + `ACEModel`. Create once; destroy when done. |
| `ULLMChat` | `LLMChat.h` | Holds the message history, tool definitions, and the latest generated response. One per NPC or conversation thread. |
| `ULLMInferenceOptions` | `LLMInferenceOptions.h` | Sampling and streaming configuration. One per inference style; reusable across calls. |
| `ULLMInferenceTask` | `LLMInferenceTask.h` | Async Blueprint task node. Created by `RunLLMChat`; fires `OnTokenOutput` and `OnGeneratedResponse`. |
| `ULLMToolRunnerTask` | `LLMToolRunnerTask.h` | Async Blueprint task node. Created by `RunTools`; fires `OnToolCall` once per tool call in the latest response. |

---

## `ULLMEngine`

`UObject` — owns the single global Game Agent SDK context and loaded model for the module. Only one engine instance can be active at a time across the entire plugin.

### Functions

| Function | Signature | Notes |
|---|---|---|
| `CreateLLMEngine` | `static bool CreateLLMEngine(UObject* WorldContextObject, FString ModelFilename, int MaxContextSize)` | Initialises the Game Agent SDK context (acquiring D3D12 device/queue if available) and loads the specified `.gguf` model. `ModelFilename` may be a relative or absolute path; if empty, falls back to **Path To Model** in Project Settings. `MaxContextSize` sets the token context window — typical values are `2048`, `4096`, or `8192`. Returns `true` on success. **Synchronous** — call during a loading phase, not mid-frame. |
| `DestroyLLMEngine` | `static bool DestroyLLMEngine()` | Releases the ACE model and context. Call during cleanup (`EndPlay`, actor destroy, or game shutdown). Returns `true` if a context was live and was successfully released. Calling when no engine is loaded is a safe no-op. |

---

## `ULLMChat`

`UObject` — the chat session wrapper. Holds a typed array of `ULLMChatMessage` wrappers, a typed array of `ULLMTool` definitions, and caches the most recent assistant response. One chat object maps to one `ACEChat` native object.

### Creation

| Function | Signature | Notes |
|---|---|---|
| `CreateChatObject` | `static ULLMChat* CreateChatObject(UObject* WorldContextObject, int MaxSize, bool SaveChatHistory)` | Creates a new chat session. `MaxSize` caps the internal message array; `0` or negative means unlimited. `SaveChatHistory` controls whether `Run LLM Chat` automatically appends the generated assistant response back into the message array after each call. |

### Message Management

| Function | Signature | Notes |
|---|---|---|
| `MakeNewMessage` | `ULLMChatMessage* MakeNewMessage(UObject* WorldContextObject, ELLMChatRole Role, FString Content)` | Creates a `ULLMChatMessage`, appends it to the chat, and returns the wrapper. The system message must be added **before** any user or assistant messages. Only one `System` role message is meaningful per chat object. |
| `AddMessage` | `bool AddMessage(ULLMChatMessage* message)` | Appends a pre-existing `ULLMChatMessage` wrapper (e.g. one you created manually with `CreateChatMessage`). Returns `false` if the message is null. |
| `GetMessage` | `ULLMChatMessage* GetMessage(int32 Index)` | Returns the message at `Index`, or null if out of range. |
| `DeleteMessage` | `void DeleteMessage(int32 Index)` | Removes the message at `Index` from both the wrapper array and the underlying `ACEChat`. |
| `DeleteAllMessages` | `void DeleteAllMessages()` | Clears the entire message history and resets the underlying `ACEChat`. Use between discrete conversations. |
| `GetMessageRole` | `ELLMChatRole GetMessageRole(int32 Index)` | Returns the role enum for the message at `Index`. |
| `GetMessageContent` | `FString GetMessageContent(int32 Index)` | Returns the text content for the message at `Index`. |
| `GetMessageCount` | `int32 GetMessageCount() const` | Returns the current number of messages in the wrapper array. |
| `GetLatestResponse` | `ULLMChatMessage* GetLatestResponse()` | Returns the most recently generated assistant response, or null if no inference has completed on this chat object. This is the same message that is added to history when `SaveChatHistory` is `true`. |
| `IsChatHistoryEnabled` | `bool IsChatHistoryEnabled() const` | Returns whether `SaveChatHistory` was set to `true` at creation. |

### Tool Management

| Function | Signature | Notes |
|---|---|---|
| `AddTool` | `bool AddTool(ULLMTool* Tool)` | Registers a tool definition with this chat session. Tools must be added before calling `Run LLM Chat` - the model will only be aware of tools present at inference time. Returns `false` if the tool is null. |
| `GetTool` | `ULLMTool* GetTool(int32 Index)` | Returns the registered tool at `Index`. |
| `DeleteTool` | `bool DeleteTool(int32 Index)` | Removes the registered tool at `Index`. Returns `false` if the index is out of range. |

### Debug Helpers

| Function | Notes |
|---|---|
| `DebugPrintChatObject` | Logs the full chat object — messages, tools, and optionally the underlying ACE native objects. |
| `DebugPrintMessageArray` | Logs only the message array. |
| `DebugPrintToolArray` | Logs only the tool definitions. |
| `DebugPrintLatestResponse` | Logs the latest assistant response and any attached tool calls. |

---

## `ULLMChatMessage`

`UObject` — a single message in the conversation. Role and content are immutable after creation.

### Creation

| Function | Signature | Notes |
|---|---|---|
| `CreateChatMessage` | `static ULLMChatMessage* CreateChatMessage(UObject* WorldContextObject, ELLMChatRole Role, FString Content)` | Creates a standalone message that is **not** automatically added to a chat. Use `ULLMChat::AddMessage` to attach it, or use `ULLMChat::MakeNewMessage` to create and add in one call. |

### Accessors

| Function | Returns | Notes |
|---|---|---|
| `GetRole()` | `ELLMChatRole` | The message role (User, Assistant, System, Tool, Invalid). |
| `GetContent()` | `FString` | The text content of the message. |
| `GetToolCallList` | `ULLMToolCallList*` | Returns the tool-call list attached to this message, if any (only present on assistant messages when the model requested tool calls). Triggers a build of the wrapper list from the native `ACEChatMessage` on first call. |

### Debug Helpers

| Function | Notes |
|---|---|
| `DebugPrintMessage` | Logs the role and content of this message. |
| `DebugPrintToolCalls` | Logs the tool calls attached to this message. |

---

## `ULLMInferenceOptions`

`UObject` — sampling and streaming configuration for a single inference call. Reusable across multiple calls; values can be modified between calls with `SetOption`.

### Creation

| Function | Signature |
|---|---|
| `CreateLLMInferenceOptions` | `static ULLMInferenceOptions* CreateLLMInferenceOptions(UObject* WorldContextObject, float Temperature, float TopP, int TopK, float MinP, float RepeatPenalty, bool EnableStreaming, bool EnableThinkMode, bool OutputThinkTokens)` |

### Parameters

| Parameter | Type | Recommended Default | Description |
|---|---|---|---|
| `Temperature` | `float` | `0.2` | Sampling temperature. Lower values (towards `0.0`) make token selection more deterministic (always picks the highest-probability token at `0.0`). Higher values increase randomness and creativity. Range: `0.0` – `2.0`. |
| `TopP` | `float` | `0.8` | Nucleus (top-p) sampling. The model samples from the smallest set of tokens whose cumulative probability exceeds this threshold. `1.0` disables nucleus filtering. |
| `TopK` | `int` | `20` | Top-k sampling. Limits candidate tokens to the `k` highest-probability options before applying top-p. `0` disables. |
| `MinP` | `float` | `0.0` | Minimum probability filter. Tokens with probability below `MinP * (max token probability)` are excluded. `0.0` disables. |
| `RepeatPenalty` | `float` | `1.0` | Repetition penalty applied to tokens already present in the context. `1.0` = no penalty. Values above `1.0` (e.g. `1.1` – `1.3`) reduce repetitive output. |
| `EnableStreaming` | `bool` | `true` | When `true`, `OnTokenOutput` fires for each generated token. When `false`, the full response is returned only via `OnGeneratedResponse`. |
| `EnableThinkMode` | `bool` | `false` | Enables reasoning-token mode on models that support it (e.g. Qwen3 with thinking). The model generates internal reasoning before the final answer. |
| `OutputThinkTokens` | `bool` | `false` | When `true` (and `EnableThinkMode` is `true`), reasoning tokens appear in `OnTokenOutput`. When `false`, only the final answer tokens are streamed. |

### Option Accessors

| Function | Signature | Notes |
|---|---|---|
| `GetOption` | `float GetOption(ELLMInferenceOptions OptionName)` | Reads any option by enum. Boolean options are stored as `0.0` / `1.0`. |
| `SetOption` | `void SetOption(ELLMInferenceOptions OptionName, float Value)` | Updates any option by enum. Call before the next `Run LLM Chat` invocation. |

---

## `ULLMInferenceTask`

`UBlueprintAsyncActionBase` — the async task node returned by `Run LLM Chat`. Internally spawns a producer thread (Game Agent SDK inference) and a consumer thread (token polling), then marshals delegate broadcasts back to the game thread.

### Creation

| Function | Signature | Notes |
|---|---|---|
| `RunLLMChat` | `static ULLMInferenceTask* RunLLMChat(UObject* WorldContextObject, ULLMChat* ChatObj, ULLMInferenceOptions* Options, FString SystemPromptOverride)` | Creates and returns the task. The task does **not** start until `Activate()` is called, which Blueprint's async node wiring handles automatically. `SystemPromptOverride`: if non-empty, replaces the system message for this call only without modifying the chat object's stored messages. Pass `TEXT("")` to use the chat's existing system message. |

### Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnTokenOutput` | `(FString CurrentToken, FString CurrentString)` | Once per generated token when `EnableStreaming` is `true`. `CurrentToken` is the new token; `CurrentString` is the full accumulated response up to and including this token. |
| `OnGeneratedResponse` | `()` | Once when inference completes (all tokens generated). After this fires, call `GetLatestResponse()` on the chat object to retrieve the full assistant message. |

---

## `ULLMTool`

`UObject` — a tool definition describing a game action the model may request. Registered with `ULLMChat::AddTool` before inference.

### Creation

| Function | Signature | Notes |
|---|---|---|
| `CreateTool` | `static ULLMTool* CreateTool(UObject* WorldContextObject, const FString& InName, const FString& InDescription, const FString& InParametersSchemaJson)` | Creates a tool definition. All three fields are required by the Game Agent SDK. |

### Fields

| Field | Type | Description |
|---|---|---|
| `Name` | `FString` | The function name the model will emit in its tool-call JSON (e.g. `"open_door"`, `"query_inventory"`). Must be a valid identifier string — no spaces. |
| `Description` | `FString` | A natural-language description of when and why the model should call this tool. The quality of this description directly affects tool-call accuracy. |
| `ParametersSchemaJson` | `FString` | A JSON Schema object describing the tool's input parameters. Example: `{"type":"object","properties":{"door_id":{"type":"string","description":"The unique ID of the door to open"}},"required":["door_id"]}` |

---

## `ULLMToolCallList`

`UObject` — the list of tool calls parsed from a single assistant message after inference.

### Functions

| Function | Signature | Notes |
|---|---|---|
| `CreateToolCallList` | `static ULLMToolCallList* CreateToolCallList(UObject* WorldContextObject, ULLMChatMessage* Parent)` | Manually creates a list wrapper attached to a parent message. Typically you do not call this directly — it is created by `ULLMChatMessage::GetToolCallList`. |
| `AddToolCall` | `ULLMToolCall* AddToolCall(UObject* WorldContextObject, FString Id, FString Name, FString Arguments)` | Appends a tool call entry. Used internally during parsing. |
| `GetCount` | `int32 GetCount() const` | Number of tool calls in this list. |
| `GetToolCall` | `ULLMToolCall* GetToolCall(int32 Index)` | Returns the tool call at `Index`. |
| `DeleteToolCallAt` | `bool DeleteToolCallAt(int32 Index)` | Removes the tool call at `Index`. |
| `ClearToolCalls` | `bool ClearToolCalls()` | Removes all tool calls. |

---

## `ULLMToolCall`

`UObject` — a single tool call emitted by the model inside an assistant message.

### Fields

| Field | Type | Description |
|---|---|---|
| `Id` | `FString` | The unique call ID string generated by the model. Pass this back in the `Tool` role message if continuing the agentic loop so the model can correlate the result to the request. |
| `Name` | `FString` | The tool name the model requested (matches the `Name` field on the registered `ULLMTool`). |
| `Arguments` | `FString` | A JSON string containing the arguments the model supplied. Always validate and parse this before executing gameplay logic. |

---

## `ULLMToolRunnerTask`

`UBlueprintAsyncActionBase` — parses tool calls from the chat object's latest response and broadcasts them one at a time.

### Creation

| Function | Signature | Notes |
|---|---|---|
| `RunTools` | `static ULLMToolRunnerTask* RunTools(UObject* WorldContextObject, ULLMChat* ChatObject)` | Creates the task. Reads the latest response from `ChatObject`, builds the tool-call list, and broadcasts `OnToolCall` once per entry when activated. |

### Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnToolCall` | `(FString ToolName, FString ToolId, FString ToolArguments)` | Once per tool call in the latest assistant response. `ToolArguments` is a raw JSON string — parse it before using the values in gameplay code. |

---

## `ULLMSettings`

`UObject` subclass of `UDeveloperSettings`, saved to `Config/DefaultGame.ini` under section `[/Script/LLMPlugin.LLMSettings]`. Exposed in the editor at *Edit -> Project Settings -> Plugins -> NVIDIA ACE LLM*.

| Category | Property | C++ Name | Type | Default | Description |
|---|---|---|---|---|---|
| LLM Model | Path To Model | `PathToModel` | `FString` | *(empty)* | Relative (to project root) or absolute path to the `.gguf` model file. Used by `CreateLLMEngine` when `ModelFilename` is empty. Relative paths use the game's base directory as the root. Accepts both forward slashes and backslashes. |

> **Note:** Sampling parameters (`Temperature`, `TopP`, `TopK`, `MinP`, `RepeatPenalty`) and streaming/think-mode flags are not stored in project settings — they are configured per-inference-call via `ULLMInferenceOptions`. This keeps each NPC or dialogue flow independently configurable at runtime without restarting the editor.

---

## `ULLMUtilities`

`UObject` — static Blueprint helpers. No state.

| Function | Signature | Notes |
|---|---|---|
| `SelectModelToLoad` | `static bool SelectModelToLoad(FString& OutModelFile)` | Opens a native Windows file picker filtered to `.gguf` files. On confirmation, writes the selected path to `OutModelFile` and returns `true`. Returns `false` if the user cancels. Pass `OutModelFile` directly to `CreateLLMEngine` for a file-picker workflow. |

---

## Enums

### `ELLMChatRole`

Defined in `LLMTypes.h`. Maps to the Game Agent SDK message role enum.

| Value | Display Name | Use |
|---|---|---|
| `User` | User | Player speech, ASR transcripts, gameplay event text. |
| `Assistant` | Assistant | Model-generated responses. Normally added automatically when `SaveChatHistory` is `true`. |
| `Tool` | Tool | Result of a tool-call action sent back to the model to continue an agentic loop. Content should be a plain string or JSON describing the outcome. |
| `System` | System | NPC personality, context, constraints. Only one system message per chat object. Must be the first message added. |
| `Invalid` | Invalid | Sentinel value — not used for message creation. |

### `ELLMInferenceOptions`

Defined in `LLMTypes.h`. Used with `GetOption` and `SetOption` as a typed accessor for the options object.

| Value | Type | Maps To |
|---|---|---|
| `Temperature` | float | `Temperature` parameter |
| `TopP` | float | `TopP` parameter |
| `TopK` | float (cast to int internally) | `TopK` parameter |
| `MinP` | float | `MinP` parameter |
| `RepeatPenalty` | float | `RepeatPenalty` parameter |
| `EnableStreaming` | float (`0.0` / `1.0`) | `EnableStreaming` flag |
| `EnableThink` | float (`0.0` / `1.0`) | `EnableThinkMode` flag |
| `OutputThinkTokens` | float (`0.0` / `1.0`) | `OutputThinkTokens` flag |

> Boolean options are stored and returned as `float` (`0.0` = false, `1.0` = true) to allow a single generic accessor pair.

---

## Delegates

All delegates defined in this plugin fire on the **game thread**.

| Delegate Type | Declared In | Parameters | Used By |
|---|---|---|---|
| `FTokenOutput` | `LLMInferenceTask.h` | `FString CurrentToken, FString CurrentString` | `ULLMInferenceTask::OnTokenOutput` |
| `FGeneratedResponse` | `LLMInferenceTask.h` | *(none)* | `ULLMInferenceTask::OnGeneratedResponse` |
| `FTokensCompleted` | `LLMInferenceTask.h` | *(none)* | Internal completion signal |
| `FToolCallOutput` | `LLMToolRunnerTask.h` | `FString ToolName, FString ToolId, FString ToolArguments` | `ULLMToolRunnerTask::OnToolCall` |
| `FACELLMReadySignature` | `ACELLMComponentTypes.h` | *(none)* | `UACELLMComponent::OnLLMReady`, `UACELLMSubsystem::OnEngineReady` |
| `FACELLMErrorSignature` | `ACELLMComponentTypes.h` | `FString ErrorMessage` | `UACELLMComponent::OnLLMError`, `UACELLMSubsystem::OnEngineError` |
| `FACELLMResponseStartedSignature` | `ACELLMComponentTypes.h` | *(none)* | `UACELLMComponent::OnResponseStarted` |
| `FACELLMTokenSignature` | `ACELLMComponentTypes.h` | `FString Token, FString PartialText` | `UACELLMComponent::OnTokenStreamed` |
| `FACELLMResponseCompleteSignature` | `ACELLMComponentTypes.h` | `FString FullResponse, ULLMChatMessage* ResponseMessage` | `UACELLMComponent::OnResponseComplete` |
| `FACELLMToolCalledSignature` | `ACELLMComponentTypes.h` | `FString ToolName, FString ToolId, FString ToolArguments` | `UACELLMComponent::OnToolCalled` |
| `FACELLMConversationResetSignature` | `ACELLMComponentTypes.h` | *(none)* | `UACELLMComponent::OnConversationReset` |

---

## Threading & Lifetime Notes

### Inference threads

`ULLMInferenceTask::Activate` launches two background threads:

- **Producer thread** (`RunChatInference`) — drives the Game Agent SDK token generation loop.
- **Consumer thread** (`GetChatInferenceOutputs`) — polls for new tokens and accumulates the response string.

Both threads run to completion. `OnBothThreadsComplete` is called on the game thread once both are done, which triggers `OnGeneratedResponse`.

All delegate broadcasts (`OnTokenOutput`, `OnGeneratedResponse`) occur on the **game thread** — it is safe to update UMG widgets and Unreal objects directly from these delegates without marshalling.

### Object lifetime

- `ULLMEngine`, `ULLMChat`, and `ULLMInferenceOptions` are standard `UObject` instances managed by Unreal's GC. Hold a `UPROPERTY` reference to prevent premature collection.
- `ULLMInferenceTask` is a `UBlueprintAsyncActionBase`. Blueprint wiring handles its lifetime automatically. From C++, keep a `UPROPERTY` reference until `OnGeneratedResponse` fires.
- The underlying `ACEChat`, `ACEModel`, and `ACEContext` native objects are owned by their respective `UObject` wrappers and are released in `BeginDestroy` or explicit destroy calls.
- Destroying `ULLMChat` while an inference task is in flight produces undefined behavior. Always wait for `OnGeneratedResponse` before releasing the chat object.

### One global engine

The plugin module (`LLMModule`) stores one `ACEContext*` and one `ACEModel*`. `CreateLLMEngine` / `DestroyLLMEngine` operate on these module-level singletons. Calling `CreateLLMEngine` while an engine is already loaded is a no-op — call `DestroyLLMEngine` first if you want to switch models.
