// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

/** Default struct member value helper when compiled as C++ */
#ifdef __cplusplus
#    define ACE_D(value) = value
#else
#    define ACE_D(value)
#endif

/** Inferencing data is produced in the order of begin, [0...n] data events, end */
typedef enum
{
    ACEEventType_Begin, ///< Indicates the beginning of inference data
    ACEEventType_Data,  ///< Indicates data produced by inferencing
    ACEEventType_End    ///< Indicates the end of inference data
} ACEEventType;

/** Vector database types */
typedef enum
{
    ACEIndexType_Semantic, ///< Semantic search (vector database)
    ACEIndexType_Lexical,  ///< Lexical search (text database)
} ACEIndexType;

/** Vector database entry for multiple VDB support */
typedef struct
{
    const char*  vecDbDirName; ///< Path to the vector data base to load
    ACEIndexType vecDbType;    ///< Type of vector database (semantic or lexical search)
} ACEIndexEntry;

/** Inference backend for ACEModelLoadParams */
typedef enum
{
    ACEInferenceBackend_Llama, ///< Llama.cpp backend (default)
} ACEInferenceBackend;

// Pre-context input-args structs (ACEInitArgs, ACENvIGIParams) cannot be opaque because they
// are passed to ace_initContext before any ACEContext exists to act as a factory. They carry
// a \c version field so the library can detect the layout the caller was built with.
//
// Maintainer contract when changing one of these structs' layout:
//   - Append new fields ONLY at the end. Never insert in the middle, remove an existing field,
//     change a field's type or width, or reorder.
//   - When you make an append-only change, bump the ACE_*_VERSION constant by 1.
//   - In the library, accept the full range of versions you intend to support (e.g.
//     `args->version < 1 || args->version > ACE_INIT_ARGS_VERSION` rejects unknown versions),
//     and guard reads of any newly-added field with `if (args->version >= N) ...` so an older
//     caller (whose struct doesn't have that field) isn't fed garbage from beyond its slot.
//
// Post-context input-args structs (ACEModelLoadParams, ACEDatabaseLoadParams, ACESearchOptions,
// ACEInferenceOptions, ACEAgentParams) are opaque handles created via ctx->Create...() and
// configured through typed setters, so their layout is invisible to callers and version-free.
//
// C++ wrappers in ace_cpp.h fill in \c version automatically. Pure C callers must set it.

#define ACE_NVIGI_PARAMS_VERSION 1

/** Used to load/configure NvIGI */
typedef struct ACENvIGIParams
{
    uint32_t version       ACE_D(ACE_NVIGI_PARAMS_VERSION); ///< Set to ACE_NVIGI_PARAMS_VERSION; bumped on incompatible layout changes. C++ wrapper fills this in.
    const char* pluginPath ACE_D(nullptr);                  ///< Path to the NvIGI plugins (NvIGI dlls). Not currently supported; reserved for future use.
} ACENvIGIParams;

/** Log levels for ACE logging system */
typedef enum
{
    ACELogLevel_Error,          ///< Error messages
    ACELogLevel_Warn,           ///< Warning messages
    ACELogLevel_Info,           ///< Informational messages
    ACELogLevel_InferenceDebug, ///< Debug messages for inference operations
    ACELogLevel_Count           ///< Total count of log levels
} ACELogLevel;

typedef void (*ACELogFuncCallbackType)(ACELogLevel level, const char* msg, void* userData);

/** Output event for SLM Inferencing.
 *
 * Layout is locked: this struct is returned by value at every streamed token, so an inline
 * buffer keeps the hot path heap-free. Any incompatible field change must go through a
 * parallel struct (e.g. ACETokenEvent2) + parallel API rather than modifying this one.
 */
typedef struct
{
    ACEEventType  eventType;     ///< Indicates the type of event (begin/data/end)
    unsigned long tokenSize;     ///< Size of the token data string including the null term character
    char          tokenData[16]; ///< The token as UTF8 string including null term (Note: tokens larger than this buffer will be split across multiple ACETokenEvents)
} ACETokenEvent;

typedef struct ACEContext            ACEContext;
typedef struct ACEToolCall           ACEToolCall;
typedef struct ACEToolCalls          ACEToolCalls;
typedef struct ACETool               ACETool;
typedef struct ACEChatMessage        ACEChatMessage;
typedef struct ACEChat               ACEChat;
typedef struct ACEModel              ACEModel;
typedef struct ACEModelLoadParams    ACEModelLoadParams;
typedef struct ACEDatabase           ACEDatabase;
typedef struct ACEDatabaseLoadParams ACEDatabaseLoadParams;
typedef struct ACESearchResults      ACESearchResults;
typedef struct ACESearchResult       ACESearchResult;
typedef struct ACEInferenceOptions   ACEInferenceOptions;
typedef struct ACESearchOptions      ACESearchOptions;
typedef struct ACEAgentParams        ACEAgentParams;
typedef struct ACEAgent              ACEAgent;

struct ID3D12Device;
struct ID3D12CommandQueue;

typedef enum
{
    ACEDeviceType_None,
    ACEDeviceType_D3D12,
    ACEDeviceType_Vulkan,
} ACEDeviceType;

/** Graphics API Parameters for CiG Support in NvIGI */
typedef struct
{
    ACEDeviceType deviceType;
    union
    {
        struct
        {
            ID3D12Device*       device;
            ID3D12CommandQueue* commandQueue;
        } d3d12;
        struct
        {
            void* physicalDevice;
            void* device;
            void* instance;
            void* queue;
        } vulkan;
    } deviceInfo;
} ACEDeviceArgs;

typedef struct
{
    void* (*allocate)(void* userData, size_t size);
    void (*release)(void* userData, void* ptr);
    void* userData;
} ACEAllocator;

#define ACE_INIT_ARGS_VERSION 1

/** Initialization arguments for ACE framework */
typedef struct ACEInitArgs
{
    uint32_t version       ACE_D(ACE_INIT_ARGS_VERSION); ///< Set to ACE_INIT_ARGS_VERSION; see maintainer contract above ACENvIGIParams. C++ wrapper fills this in.
    const ACENvIGIParams*  nvIgiParams;                  ///< Parameters used when loading NvIGI SDK
    const char**           embeddingModelPaths;          ///< Embedding model file paths to load (count = embeddingModelCount)
    unsigned long          embeddingModelCount;          ///< Number of entries in embeddingModelPaths
    const char*            crossEncoderModelPath;        ///< Cross-encoder ONNX model path (required when an embedding model is MiniLM-based)
    ACELogFuncCallbackType logFuncCallback;              ///< Callback function used for logging messages
    void*                  logUserData;                  ///< User data passed back to the callback function
    ACELogLevel            level;                        ///< Log level of logs to capture
    ACEDeviceArgs          deviceArgs;                   ///< Info about which device, if any, to use for compute
    ACEAllocator           allocator;                    ///< Custom allocator callbacks - set all to zero to use default allocator
} ACEInitArgs;

/** Model inference options */
typedef enum
{
    ACEInferenceOption_Temperature,       ///< default: 0.2f
    ACEInferenceOption_TopP,              ///< default: 0.8f
    ACEInferenceOption_TopK,              ///< default: 20.0f
    ACEInferenceOption_MinP,              ///< default: 0.0f
    ACEInferenceOption_RepeatPenalty,     ///< default: 1.0f
    ACEInferenceOption_EnableStreaming,   ///< default: 1.0f (on); nonzero = enable token streaming to the model output buffer
    ACEInferenceOption_EnableThink,       ///< default: 0.0f (off); nonzero = enable extended thinking / reasoning (model-dependent)
    ACEInferenceOption_OutputThinkTokens, ///< default: 0.0f (off); nonzero = include thinking/reasoning tokens in streamed output
} ACEInferenceOption;

/** Search options for \c ace_databaseSearch and \c ace_hybridSearch */
typedef enum
{
    ACESearchOption_MaxNumResults, ///< default: 5; max count of results (per database) returned by search
} ACESearchOption;

/** Parameter keys for ACEDatabaseLoadParams */
typedef enum
{
    ACEDatabaseLoadParam_IndexType,          ///< (int)    ACEIndexType_Semantic (default) or ACEIndexType_Lexical
    ACEDatabaseLoadParam_EmbeddingModelPath, ///< (string) Semantic only; must match one of the paths in ACEInitArgs::embeddingModelPaths at context init
} ACEDatabaseLoadParam;

/** Parameter keys for ACEModelLoadParams */
typedef enum
{
    ACEModelLoadParam_MaxContextSize,            ///< (int) Max context length in tokens; 0 = use model default
    ACEModelLoadParam_InferenceOutputBufferSize, ///< (int) Max events in the inference output buffer; 0 = no recommendation
    ACEModelLoadParam_Backend,                   ///< (int) ACEInferenceBackend value
} ACEModelLoadParam;

/** Chat template role for ACEChatMessage (system, user, assistant) */
typedef enum
{
    ACEChatRole_User,
    ACEChatRole_Assistant,
    ACEChatRole_Tool,
    ACEChatRole_System,
} ACEChatRole;

/** Parameter keys for ACEAgentParams */
typedef enum
{
    ACEAgentParam_Id,            ///< (string) Caller-chosen identifier for this agent
    ACEAgentParam_Instructions,  ///< (string) System instructions defining agent behavior
    ACEAgentParam_MaxSteps,      ///< (int)    Cap on consecutive inferences in an autonomous tool-call chain. Tool result delivery does not reset the counter; other client input does (default: 10; <= 0 = unbounded)
    ACEAgentParam_HistoryWindow, ///< (int)    Rolling window in messages; oldest messages beyond this are dropped from the prompt (default: 50; <= 0 = unbounded)
} ACEAgentParam;

/** Status codes returned by agent */
typedef enum
{
    ACEAgentStatus_Shutdown,     ///< Session ended
    ACEAgentStatus_Idle,         ///< Agent is not processing
    ACEAgentStatus_ResponseText, ///< Response text available via getResponseText()
    ACEAgentStatus_ToolCalls,    ///< Tool calls available via getToolCalls()
    ACEAgentStatus_Error,        ///< Error occurred, error message available via getError()
    ACEAgentStatus_Cancelled,    ///< Agent was cancelled via cancel()
} ACEAgentStatus;
