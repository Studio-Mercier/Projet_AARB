// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once


#ifdef ACE_EXPORT
#    ifdef __cplusplus
#        define ACE_API extern "C" __declspec(dllexport)
#    else
#        define ACE_API __declspec(dllexport)
#    endif
#else
#    ifdef __cplusplus
#        define ACE_API extern "C" __declspec(dllimport)
#    else
#        define ACE_API __declspec(dllimport)
#    endif
#endif


#include "ace_result.h"
#include "ace_types.h"

//! Initializes the ACE framework
//!
//! This function will create a ACE handle placing it in *pCtx and
//! perform the loading of the underlying NvIGI SDK
//!
//! @param pCtx The context of the ACE session to create
//! @param pInitArgs The initialization arguments containing NvIGI parameters, logging configuration, and log level
//!
//! @returns Status Code
//!
//! Lifetime: On success, caller owns *pCtx until ace_destroyContext.
//!
ACE_API ACEResult ace_initContext(ACEContext** pCtx, const ACEInitArgs* pInitArgs);

//! Destroy the ACE Context Object
//!
//! This method will destroy the ACE Context Object and also unload the NvIGI SDK
//!
//! @param ctx The context of the ACE session to destroy
//!
//! @returns Status Code
//!
//! Lifetime: Destroy every object created from this context (via ace_create* / ace_loadModel) before calling this.
//!
ACE_API ACEResult ace_destroyContext(ACEContext* ctx);

//! Validate the NVIDIA GPU Driver Version
//!
//! This function will check the driver version reported by the OS and return a
//! warning if the driver version is too old
//!
//! @returns Status Code (ACEResultInvalidDriverVersion if driver is too old)
//!
ACE_API ACEResult ace_validateDriverVersion();

//! @anchor ace_ownership
//! Lifetimes: Objects created below are tied to their ACEContext until destroyed; destroy them before ace_destroyContext.

//! Create inference options for use with ace_Model_Chat
//!
//! @param ctx Context session
//! @param out Receives the new handle
//! @returns Status Code
//!
//! Lifetime: On success, caller owns *out until ace_destroyInferenceOptions.
//!
ACE_API ACEResult ace_createInferenceOptions(ACEContext* ctx, ACEInferenceOptions** out);

//! Destroy inference options
//!
//! @param options The inference options object to destroy
//!
//! Lifetime: Frees \p options; must not be used after this returns.
//!
ACE_API void ace_destroyInferenceOptions(ACEInferenceOptions* options);

//! Set a single inference option (e.g. temperature, top_p)
//!
//! @param options The inference options object
//! @param option The option to set (ACEInferenceOption_*)
//! @param value The value (temperature/top_p/etc. as float; boolean options use nonzero = true, zero = false)
//! @returns Status Code
//!
ACE_API ACEResult ace_setInferenceOptionFloat(ACEInferenceOptions* options, ACEInferenceOption option, float value);

//! Get a single inference option value
//!
//! Writes the current value of \p option into \p *out. Boolean options are written as 0.0f or 1.0f.
//! The return value is an ACEResult status code indicating success or error; it is not the option
//! value (read \p *out after a successful return).
//!
//! @param options The inference options object (const ACEInferenceOptions*)
//! @param option Which option to read (ACEInferenceOption)
//! @param out Output pointer; receives the option value on success (float*)
//! @returns Status code (ACEResult), not the option value
//!
ACE_API ACEResult ace_getInferenceOptionFloat(const ACEInferenceOptions* options, ACEInferenceOption option, float* out);

//! Create search options for \c ace_databaseSearch and \c ace_hybridSearch
//!
//! @param ctx Context session
//! @param out Receives the new handle
//! @returns Status Code
//!
//! Lifetime: On success, caller owns *out until ace_destroySearchOptions.
//!
ACE_API ACEResult ace_createSearchOptions(ACEContext* ctx, ACESearchOptions** out);

//! Destroy search options
//!
//! @param options The search options object to destroy
//!
//! Lifetime: Frees \p options; must not be used after this returns.
//!
ACE_API void ace_destroySearchOptions(ACESearchOptions* options);

//! Set a single search option (e.g. max result count)
//!
//! @param options The search options object
//! @param option The option to set (\c ACESearchOption_*)
//! @param value The value (\c ACESearchOption_MaxNumResults: max hits per database, must be >= 1)
//! @returns Status Code
//!
ACE_API ACEResult ace_setSearchOptionInt(ACESearchOptions* options, ACESearchOption option, int value);

//! Get a single search option value
//!
//! @param options The search options object (const ACESearchOptions*)
//! @param option Which option to read (ACESearchOption)
//! @param out Output pointer; receives the option value on success (int*)
//! @returns Status code (ACEResult), not the option value
//!
ACE_API ACEResult ace_getSearchOptionInt(const ACESearchOptions* options, ACESearchOption option, int* out);

//! Create a chat object for ace_Model_Chat (and related chat APIs).
//!
//! @param ctx Context session
//! @param out Receives the new handle
//! @returns Status Code
//!
//! Lifetime: On success, caller owns *out until ace_destroyChat.
//!
ACE_API ACEResult ace_createChat(ACEContext* ctx, ACEChat** out);

//! Destroy a chat object, all messages it owns, and all tools added with ace_chatAddTool.
//!
//! @param chat The chat object to destroy
//!
//! Lifetime: Frees \p chat and nested messages and tools; must not be used after this returns.
//!
ACE_API void ace_destroyChat(ACEChat* chat);

//! Get the number of entries in a chat
//!
ACE_API ACEResult ace_chatGetCount(const ACEChat* chat, size_t* out);

//! Get a chat entry by index
//!
//! Lifetime: *out is valid until \p chat is modified or destroyed.
//!
ACE_API ACEResult ace_chatGet(const ACEChat* chat, size_t index, ACEChatMessage** out);

//! Add a new empty entry to the chat and return it
//!
//! Lifetime: On success, the chat owns *out until removed or ace_destroyChat.
//!
ACE_API ACEResult ace_chatAdd(ACEChat* chat, ACEChatMessage** out);

//! Add a copy of an entry to the chat
//!
//! Lifetime: On success, the chat owns the copy; \p entry remains owned by its caller.
//!
ACE_API ACEResult ace_chatAddEntry(ACEChat* chat, const ACEChatMessage* entry);

//! Remove the entry at the given index and return it
//!
//! Lifetime: On success, caller owns *out until ace_destroyChatMessage.
//!
ACE_API ACEResult ace_chatDelete(ACEChat* chat, size_t index, ACEChatMessage** out);

//! Add a tool to the chat.
//!
//! Lifetime: On success, the chat owns \p tool; caller must not destroy it.
//!
ACE_API ACEResult ace_chatAddTool(ACEChat* chat, ACETool* tool);

//! Get the number of tools in the chat
//!
ACE_API ACEResult ace_chatGetToolCount(const ACEChat* chat, size_t* out);

//! Get a tool by index
//!
//! Lifetime: *out is valid until that tool is removed or \p chat is destroyed.
//!
ACE_API ACEResult ace_chatGetTool(const ACEChat* chat, size_t index, ACETool** out);

//! Remove and destroy the tool at the given index
//!
ACE_API ACEResult ace_chatDeleteTool(ACEChat* chat, size_t index);

//! Remove and destroy all tools in the chat
//!
ACE_API ACEResult ace_chatClearTools(ACEChat* chat);

//! Destroy a chat message
//!
//! Lifetime: Frees \p entry. Do not call for messages still owned by a chat (use ace_chatDelete or ace_destroyChat).
//!
ACE_API void ace_destroyChatMessage(ACEChatMessage* entry);

//! @name Role
//! Role of a chat entry.
//! @{
ACE_API ACEResult ace_chatMessageGetRole(const ACEChatMessage* entry, ACEChatRole* out);
ACE_API ACEResult ace_chatMessageSetRole(ACEChatMessage* entry, ACEChatRole role);
//! @}

//! @name Content
//! Content of a chat entry.
//!
//! Lifetime (get): *out valid until \p entry is modified or destroyed.
//! @{
ACE_API ACEResult ace_chatMessageGetContent(const ACEChatMessage* entry, const char** out);
ACE_API ACEResult ace_chatMessageSetContent(ACEChatMessage* entry, const char* content);
//! @}

//! Tool-call list embedded in \p entry (use ace_toolCalls* on \p *out to read or modify).
//!
ACE_API ACEResult ace_chatMessageGetToolCalls(ACEChatMessage* entry, ACEToolCalls** out);

//! @name Tool-call ID
//! Tool-call ID for a tool-result message.
//!
//! Lifetime (get): *out valid until \p entry is modified or destroyed.
//! @{
ACE_API ACEResult ace_chatMessageGetToolCallId(const ACEChatMessage* entry, const char** out);
ACE_API ACEResult ace_chatMessageSetToolCallId(ACEChatMessage* entry, const char* id);
//! @}

//! @name Redacted flag
//!
//! When non-zero, the message content is shown as "[REDACTED]" in the prompt once an assistant
//! message follows it in the conversation. The original content is preserved (useful for large
//! tool results that should not consume context after the model has acted on them).
//!
//! Honored only by ACEAgent. Setting it on a message owned by an ACEChat has no effect.
//! @{
ACE_API ACEResult ace_chatMessageGetRedacted(const ACEChatMessage* entry, unsigned long* out);
ACE_API ACEResult ace_chatMessageSetRedacted(ACEChatMessage* entry, unsigned long redacted);
//! @}

//! @name Transient flag
//!
//! When non-zero, the message is included in the next prompt prepared for inference but is
//! skipped from subsequent prompts once an assistant message follows it in the conversation.
//! The original content is preserved. Useful for one-shot steering that should not pollute
//! the persistent prompt rendering.
//!
//! If both transient and redacted are set, transient takes precedence: the message is skipped
//! entirely after consumption rather than rendered as "[REDACTED]".
//!
//! Honored only by ACEAgent. Setting it on a message owned by an ACEChat has no effect.
//! @{
ACE_API ACEResult ace_chatMessageGetTransient(const ACEChatMessage* entry, unsigned long* out);
ACE_API ACEResult ace_chatMessageSetTransient(ACEChatMessage* entry, unsigned long transient);
//! @}

//! Tool-call list (embedded in a chat message, or from ace_agentGetToolCalls).
//!
ACE_API ACEResult ace_toolCallsGetCount(const ACEToolCalls* list, size_t* out);
ACE_API ACEResult ace_toolCallsGetCall(const ACEToolCalls* list, size_t index, ACEToolCall** out);
ACE_API ACEResult ace_toolCallsAddCall(ACEToolCalls* list, ACEToolCall** out);
ACE_API ACEResult ace_toolCallsDeleteCall(ACEToolCalls* list, size_t index);
ACE_API ACEResult ace_toolCallsClear(ACEToolCalls* list);

//! @name ACEToolCall accessors
//! Get and set the id, name, and arguments fields of an ACEToolCall.
//!
//! Lifetime (get): *out valid until the tool call or its owning tool-call list / ACEChatMessage is modified or destroyed. No separate destroy for ACEToolCall.
//! @{
ACE_API ACEResult ace_toolCallGetId(const ACEToolCall* toolCall, const char** out);
ACE_API ACEResult ace_toolCallSetId(ACEToolCall* toolCall, const char* id);
ACE_API ACEResult ace_toolCallGetName(const ACEToolCall* toolCall, const char** out);
ACE_API ACEResult ace_toolCallSetName(ACEToolCall* toolCall, const char* name);
ACE_API ACEResult ace_toolCallGetArguments(const ACEToolCall* toolCall, const char** out);
ACE_API ACEResult ace_toolCallSetArguments(ACEToolCall* toolCall, const char* arguments);
//! @}

//! Create a standalone tool
//!
//! Lifetime: On success, caller owns *out until ace_destroyTool or ace_chatAddTool (transfer to chat).
//!
ACE_API ACEResult ace_createTool(ACEContext* ctx, ACETool** out);

//! Lifetime: Frees \p tool. Call only while the caller still owns it (not after a successful ace_chatAddTool).
//!
ACE_API void ace_destroyTool(ACETool* tool);

//! @name ACETool accessors
//! Get and set the name, description, and parameters fields of an ACETool.
//!
//! Lifetime (get): *out valid until \p tool is modified or destroyed.
//! @{
ACE_API ACEResult ace_toolGetName(const ACETool* tool, const char** out);
ACE_API ACEResult ace_toolSetName(ACETool* tool, const char* name);
ACE_API ACEResult ace_toolGetDescription(const ACETool* tool, const char** out);
ACE_API ACEResult ace_toolSetDescription(ACETool* tool, const char* description);
ACE_API ACEResult ace_toolGetParameters(const ACETool* tool, const char** out);
ACE_API ACEResult ace_toolSetParameters(ACETool* tool, const char* parameters);
//! @}

//! Create a model load params object. Configure with \c ace_modelLoadParamsSetInt, then pass
//! to \c ace_loadModel (or pass NULL to ace_loadModel for defaults).
//!
//! Lifetime: On success, caller owns *out until ace_destroyModelLoadParams.
//!
ACE_API ACEResult ace_createModelLoadParams(ACEContext* ctx, ACEModelLoadParams** out);

//! Destroy model load params.
//!
ACE_API void ace_destroyModelLoadParams(ACEModelLoadParams* params);

//! Set an integer parameter on model load params (see \c ACEModelLoadParam_*).
//!
ACE_API ACEResult ace_modelLoadParamsSetInt(ACEModelLoadParams* params, ACEModelLoadParam param, int value);

//! Load a model. Optionally pass load params (max context length, inference output buffer size); NULL for defaults.
//!
//! @param ctx Context session
//! @param out Receives the new handle
//! @param modelPath Filesystem path to the model
//! @param params Optional load parameters created with \c ace_createModelLoadParams, or NULL for defaults
//!
//! Lifetime: On success, caller owns *out until ace_destroyModel.
//!
ACE_API ACEResult ace_loadModel(ACEContext* ctx, ACEModel** out, const char* modelPath, const ACEModelLoadParams* params);

//! Destroy a model
//!
//! Lifetime: Frees \p model; must not be used after this returns.
//!
ACE_API void ace_destroyModel(ACEModel* model);

//! Create a database load params object. Configure with \c ace_databaseLoadParamsSetInt /
//! \c ace_databaseLoadParamsSetString, then pass to \c ace_loadDatabase.
//!
//! Lifetime: On success, caller owns *out until ace_destroyDatabaseLoadParams.
//!
ACE_API ACEResult ace_createDatabaseLoadParams(ACEContext* ctx, ACEDatabaseLoadParams** out);

//! Destroy database load params.
//!
ACE_API void ace_destroyDatabaseLoadParams(ACEDatabaseLoadParams* params);

//! Set an integer parameter on database load params (e.g. \c ACEDatabaseLoadParam_IndexType).
//!
ACE_API ACEResult ace_databaseLoadParamsSetInt(ACEDatabaseLoadParams* params, ACEDatabaseLoadParam param, int value);

//! Set a string parameter on database load params (e.g. \c ACEDatabaseLoadParam_EmbeddingModelPath).
//! Pass NULL to clear.
//!
ACE_API ACEResult ace_databaseLoadParamsSetString(ACEDatabaseLoadParams* params, ACEDatabaseLoadParam param, const char* value);

//! Load a RAG index from disk and associate it with \p ctx.
//!
//! @param params Load options created with \c ace_createDatabaseLoadParams. For semantic indexes,
//!        \c ACEDatabaseLoadParam_EmbeddingModelPath must match one of the strings passed in
//!        \c ACEInitArgs::embeddingModelPaths at init.
//!
//! Lifetime: On success, caller owns *out until ace_destroyDatabase.
//!
ACE_API ACEResult ace_loadDatabase(ACEContext* ctx, ACEDatabase** out, const char* dbDirectoryPath, const ACEDatabaseLoadParams* params);

//! Destroy a database handle created with \c ace_loadDatabase (unloads loaded indexes).
//!
ACE_API void ace_destroyDatabase(ACEDatabase* database);

//! Run RAG retrieval for a single loaded database handle (semantic or lexical only that handle's index).
//!
//! @param searchOptions Search parameters (must not be null; borrowed for the call only).
//!
//! Lifetime: On success, caller owns *outResults until ace_destroySearchResults.
//!
ACE_API ACEResult ace_databaseSearch(const ACEDatabase* database, const char* query, const ACESearchOptions* searchOptions,
    ACESearchResults** outResults);

//! Combined semantic + lexical retrieval over multiple \c ACEDatabase handles. Pass at least one semantic and one lexical
//! loaded database; each must belong to \p ctx. \p databases is borrowed for the call only.
//!
//! @param searchOptions Search parameters (must not be null; borrowed for the call only).
//!
//! Lifetime: On success, caller owns *outResults until ace_destroySearchResults.
//!
ACE_API ACEResult ace_hybridSearch(ACEContext* ctx, ACEDatabase* const* databases, size_t databaseCount, const char* query,
    const ACESearchOptions* searchOptions, ACESearchResults** outResults);

ACE_API void ace_destroySearchResults(ACESearchResults* results);

ACE_API ACEResult ace_searchResultsGetCount(const ACESearchResults* results, size_t* outCount);
ACE_API ACEResult ace_searchResultsGet(ACESearchResults* results, size_t index, ACESearchResult** outHit);

ACE_API ACEResult ace_searchResultGetDocument(const ACESearchResult* hit, const char** out);
ACE_API ACEResult ace_searchResultGetMetadata(const ACESearchResult* hit, const char** out);
ACE_API ACEResult ace_searchResultGetId(const ACESearchResult* hit, const char** out);
ACE_API ACEResult ace_searchResultGetNumTokens(const ACESearchResult* hit, uint32_t* out);
ACE_API ACEResult ace_searchResultGetDistance(const ACESearchResult* hit, float* out);
ACE_API ACEResult ace_searchResultGetDocStoreIndex(const ACESearchResult* hit, int* out);

//! Get next token event from the model's internal inference buffer (after ace_Model_Chat has started inference).
//!
ACE_API ACEResult ace_Model_GetNextEvent(ACEModel* model, ACETokenEvent* outputEvent, uint32_t timeOut);

//! Run chat completion: take a chat (list of role+content entries), run inference, return the new assistant entry.
//! Inference options (temperature, etc.) may be applied when supported by the backend.
//!
//! Lifetime: \p chat and \p inferenceOptions are borrowed for the call; caller still owns both. On success, caller owns *outEntry until ace_destroyChatMessage.
//!
ACE_API ACEResult ace_Model_Chat(ACEModel* model, const ACEChat* chat, const ACEInferenceOptions* inferenceOptions, ACEChatMessage** outEntry);

//==============================================================================
// ACEAgentParams functions
//==============================================================================

//! Create agent parameters with sensible defaults.
//!
//! @param ctx The ACE context
//! @param outParams Pointer to store the created params handle
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_createAgentParams(ACEContext* ctx, ACEAgentParams** outParams);

//! Destroy agent parameters.
//!
//! @param params The params to destroy
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_destroyAgentParams(ACEAgentParams* params);

//! Set a string parameter on agent params.
//!
//! @param params The params object
//! @param param  The parameter to set (\c ACEAgentParam_*)
//! @param value  The string value, or NULL to clear.
//! @returns Status Code
//!
ACE_API ACEResult ace_agentParamsSetString(ACEAgentParams* params, ACEAgentParam param, const char* value);

//! Set an integer parameter on agent params.
//!
//! @param params The params object
//! @param param  The parameter to set (\c ACEAgentParam_*)
//! @param value  The integer value.
//! @returns Status Code
//!
ACE_API ACEResult ace_agentParamsSetInt(ACEAgentParams* params, ACEAgentParam param, int value);

//==============================================================================
// ACEAgent functions
//==============================================================================

//! Create an ACE Agent
//!
//! Creates a new agent instance bound to an ACEModel.
//! The agent uses the model for inference and its internal output buffer for streaming.
//!
//! @param model The loaded model to use for inference
//! @param params Parameters for agent initialization
//! @param inferenceOptions Inference options (temperature, streaming, thinking, etc.)
//! @param outAgent Pointer to store the created agent handle
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentCreate(ACEModel* model, const ACEAgentParams* params, const ACEInferenceOptions* inferenceOptions, ACEAgent** outAgent);

//! Add a tool to the agent (takes ownership of \p tool). Caller must not use or destroy \p tool after success.
//! Create standalone tools with ace_createTool; same ACEContext as the agent is required.
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentAddTool(ACEAgent* agent, ACETool* tool);

//! Set the agent's system instructions
//!
//! Replaces the agent's system instructions (the system message in the prompt).
//! Takes effect on the next run() call. Can be called at any time.
//! Pass NULL or an empty string to clear the instructions.
//!
//! @param agent The agent
//! @param instructions The new system instructions, or NULL to clear
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentSetInstructions(ACEAgent* agent, const char* instructions);

//! Get a pointer to the agent's inference options for in-place mutation.
//!
//! The returned pointer is owned by the agent and is valid until the agent is destroyed.
//!
//! @param agent The agent
//! @param out   Receives a pointer to the agent's inference options
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentGetInferenceOptions(ACEAgent* agent, ACEInferenceOptions** out);

//! Destroy an ACE Agent
//!
//! Destroys the agent and releases all associated resources.
//!
//! @param agent The agent to destroy
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentDestroy(ACEAgent* agent);

//! Add user input
//!
//! Appends a user message to the conversation.
//!
//! @param agent The agent
//! @param content The user's input message
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentAddUserInput(ACEAgent* agent, const char* content);

//! Add a tool result
//!
//! Appends a tool result to the conversation.
//!
//! @param agent The agent
//! @param toolCallId The tool call ID to provide the result for
//! @param content The tool result string
//! @param redacted Non-zero: this tool result shows redacted in the prompt once it is part of conversation history.
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentAddToolResult(ACEAgent* agent, const char* toolCallId, const char* content, unsigned long redacted);

//! Add a chat message to the conversation
//!
//! Creates a new empty message in the agent's conversation and returns a mutable pointer.
//! The caller populates the message using ace_chatMessageSet* and ace_toolCalls* functions.
//! This is the general-purpose alternative to the role-specific helpers (addUserInput, etc.).
//!
//! Lifetime: *out is valid until the message is removed (e.g. clearConversation) or the agent is destroyed.
//!
//! @param agent The agent
//! @param out   Pointer to receive the new message
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentAddChatMessage(ACEAgent* agent, ACEChatMessage** out);

//! Clear conversation
//!
//! Clears all conversation messages from the agent and resets transient state (step counter,
//! skip-inference flag, error message). Preamble entries are not affected.
//!
//! @param agent The agent
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentClearConversation(ACEAgent* agent);

//! Append a message to the agent's preamble (prefix messages emitted between instructions and
//! the windowed conversation; persistent across inferences and not affected by historyWindow).
//! Note: transient/redacted flags on the returned message are ignored -- those are conversation-
//! message lifecycle concerns.
//!
//! @param agent The agent
//! @param out   Pointer to receive the new message; caller fills in role/content via ace_chatMessageSet*.
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentAddPreambleMessage(ACEAgent* agent, ACEChatMessage** out);

//! Clear the agent's preamble (all entries added via ace_agentAddPreambleMessage).
//!
//! @param agent The agent
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentClearPreamble(ACEAgent* agent);

//! Get the number of messages currently in the agent's conversation.
//!
//! @param agent    The agent
//! @param outCount Receives the count
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentGetConversationCount(const ACEAgent* agent, size_t* outCount);

//! Get a read-only pointer to the conversation message at \p index. The pointer is valid
//! until the next agent mutation (AddUserInput / AddChatMessage / AddToolResult / Run /
//! ClearConversation / LoadConversation).
//!
//! @param agent      The agent
//! @param index      Index in [0, GetConversationCount)
//! @param outMessage Receives the message pointer
//!
//! @returns Status Code; ACEResultInvalidParameter if index is out of range
//!
ACE_API ACEResult ace_agentGetConversation(const ACEAgent* agent, size_t index, const ACEChatMessage** outMessage);

//! Get the index of the first conversation message that would be included in the next inference's
//! prompt under the current historyWindow setting. Messages at [0, windowStart) are about to be
//! excluded. Walks back from the end counting only messages that actually render (consumed
//! transients are skipped and don't eat into the budget).
//!
//! @param agent    The agent
//! @param outIndex Receives the index (0 if historyWindow <= 0 or conversation fits)
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentGetWindowStart(const ACEAgent* agent, size_t* outIndex);

//! Serialize the agent's conversation history to a byte buffer.
//!
//! Serializes the conversation into an opaque byte representation that can later
//! be restored with ace_agentLoadConversation. The contents of the buffer should be
//! treated as opaque by the caller. Can be called from any agent state.
//!
//! The returned buffer is owned by the agent and valid until the next ace_agent* call.
//!
//! @param agent   The agent
//! @param out     Receives a pointer to the serialized bytes (agent-owned)
//! @param outSize Receives the byte count of the serialized data
//!
//! @returns ACEResultOk on success, or another status code on error
//!
ACE_API ACEResult ace_agentSerializeConversation(ACEAgent* agent, const unsigned char** out, size_t* outSize);

//! Load (deserialize) a conversation into the agent, replacing the current conversation.
//!
//! Restores conversation history from a buffer previously produced by
//! ace_agentSerializeConversation. A trailing assistant message with unanswered tool calls
//! (i.e. no subsequent tool results) is stripped on load, since the next inference would
//! otherwise feed the model an orphaned tool_call. Only valid when the agent is Idle.
//!
//! @param agent  The agent
//! @param in     Pointer to the buffer containing the serialized data
//! @param inSize The byte count of the input buffer
//!
//! @returns ACEResultOk on success,
//!          ACEResultInvalidState if the agent is not Idle,
//!          ACEResultUnsupportedVersion if the data version is not recognized,
//!          ACEResultInvalidFormat if the buffer is corrupt or invalid,
//!          or another status code on error
//!
ACE_API ACEResult ace_agentLoadConversation(ACEAgent* agent, const unsigned char* in, size_t inSize);

//! Run the agent
//!
//! Drives the agent forward. Blocks when inference is in progress.
//! Returns a status indicating what to do next.
//!
//! @param agent The agent
//! @param out Pointer to store the current agent status
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentRun(ACEAgent* agent, ACEAgentStatus* out);

//! Skip the next inference
//!
//! Suppresses the next inference. The next run() returns Idle without
//! running the model. One-shot: the flag is cleared after.
//!
//! @param agent The agent
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentSkipInference(ACEAgent* agent);

//! Cancel the agent
//!
//! Interrupts the agent's current operation. Thread-safe.
//! The next run() returns ACEAgentStatus_Cancelled.
//!
//! @param agent    The agent
//! @param rollback If non-zero, roll back to the most recent user message (erasing everything
//!                 after it). If zero (default), preserve the conversation as-is.
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentCancel(ACEAgent* agent, unsigned long rollback);

//! Clear a pending cancel flag
//!
//! Clears a pending cancel flag without consuming it as a Cancelled status.
//! Use when cancel() was called but the agent had already completed its work
//! and returned Idle on its own.
//!
//! @param agent The agent
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentClearCancel(ACEAgent* agent);

//! Shutdown the agent
//!
//! Signals the agent to stop processing. Can be called from any thread.
//! The next run() returns ACEAgentStatus_Shutdown.
//!
//! @param agent The agent
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentShutdown(ACEAgent* agent);

//! Get response text
//!
//! Retrieves the agent's most recent response text (user-facing text; tool calls are separate).
//! Only valid when the last run() returned ACEAgentStatus_ResponseText; returns NULL otherwise.
//!
//! @param agent The agent
//! @param out Pointer to store the response text, or NULL if none available
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentGetResponseText(ACEAgent* agent, const char** out);

//! Get tool calls from the agent
//!
//! Retrieves tool calls requested by the model. Only valid when the last run() returned
//! ACEAgentStatus_ToolCalls; returns NULL otherwise.
//!
//! @param agent The agent
//! @param out Pointer to store the tool-call list (ACEToolCalls), or NULL if none. Iterate with
//!            ace_toolCallsGetCount / ace_toolCallsGetCall. Valid until the owning message is removed or
//!            tool calls on that message are modified (e.g. clear conversation).
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentGetToolCalls(ACEAgent* agent, const ACEToolCalls** out);

//! Get agent error message
//!
//! Retrieves the error message from the last run.
//!
//! @param agent The agent
//! @param out Pointer to store the error message string, or NULL if no error
//!
//! @returns Status Code
//!
ACE_API ACEResult ace_agentGetError(ACEAgent* agent, const char** out);
