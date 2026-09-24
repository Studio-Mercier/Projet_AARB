// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

#ifndef ACE_RESULT
typedef uint32_t ACEResult;
#    define ACE_RESULT const ACEResult
#endif

// Section SDK : Base Result Codes
ACE_RESULT RES_BASE_OFFSET               = 0;
ACE_RESULT ACEResultOk                   = RES_BASE_OFFSET + 0;
ACE_RESULT ACEResultNullContext          = RES_BASE_OFFSET + 1;
ACE_RESULT ACEResultWaitTimeout          = RES_BASE_OFFSET + 2;
ACE_RESULT ACEResultWaitFailed           = RES_BASE_OFFSET + 3;
ACE_RESULT ACEResultInvalidParameter     = RES_BASE_OFFSET + 4;
ACE_RESULT ACEResultOutOfMemory          = RES_BASE_OFFSET + 5;
ACE_RESULT ACEResultInvalidDriverVersion = RES_BASE_OFFSET + 6;
ACE_RESULT ACEResultException            = RES_BASE_OFFSET + 7;
ACE_RESULT ACEResultContextMismatch      = RES_BASE_OFFSET + 8;

// Section SDK : NvIGI
ACE_RESULT RES_NVIGI_OFFSET              = 100;
ACE_RESULT ACEResultNvIgiFailedToLoadSDK = RES_NVIGI_OFFSET + 1;
ACE_RESULT ACEResultNvIgiFailedToInitSLM = RES_NVIGI_OFFSET + 2;

// Section SDK : RAG
ACE_RESULT RES_RAG_OFFSET                       = 200;
ACE_RESULT ACEResultRagDBNotFound               = RES_RAG_OFFSET + 1;
ACE_RESULT ACEResultFailedToInitRag             = RES_RAG_OFFSET + 2;
ACE_RESULT ACEResultRagQueryFailed              = RES_RAG_OFFSET + 3;
ACE_RESULT ACEResultRagInvalidSearchType        = RES_RAG_OFFSET + 4;
ACE_RESULT ACEResultRagLexicalDBNotLoaded       = RES_RAG_OFFSET + 5;
ACE_RESULT ACEResultRagSemanticDBNotLoaded      = RES_RAG_OFFSET + 6;
ACE_RESULT ACEResultRagTooManySemanticDBsLoaded = RES_RAG_OFFSET + 7;
ACE_RESULT ACEResultRagTooManyLexicalDBsLoaded  = RES_RAG_OFFSET + 8;

// Section SDK : Embedding
ACE_RESULT RES_EMBEDDING_OFFSET                    = 300;
ACE_RESULT ACEResultFailedToInitEmbedding          = RES_EMBEDDING_OFFSET + 1;
ACE_RESULT ACEResultEmbeddingModelNotFound         = RES_EMBEDDING_OFFSET + 2;
ACE_RESULT ACEResultEmbeddingNullTokenizer         = RES_EMBEDDING_OFFSET + 3;
ACE_RESULT ACEResultEmbeddingCrossEncAlreadyLoaded = RES_EMBEDDING_OFFSET + 4;
ACE_RESULT ACEResultEmbeddingDimMismatch           = RES_EMBEDDING_OFFSET + 5;

// Section SDK : SLM
ACE_RESULT RES_SLM_OFFSET                 = 400;
ACE_RESULT ACEResultSLMTokenizeFailed     = RES_SLM_OFFSET + 1;
ACE_RESULT ACEResultSLMDecodeFailed       = RES_SLM_OFFSET + 2;
ACE_RESULT ACEResultSLMTokenToPieceFailed = RES_SLM_OFFSET + 3;

// Section SDK : Agent
ACE_RESULT RES_AGENT_OFFSET               = 500;
ACE_RESULT ACEResultAgentMaxToolExchanges = RES_AGENT_OFFSET + 1;
ACE_RESULT ACEResultCancelled             = RES_AGENT_OFFSET + 2;
ACE_RESULT ACEResultInvalidState          = RES_AGENT_OFFSET + 3;
ACE_RESULT ACEResultInvalidFormat         = RES_AGENT_OFFSET + 4;
ACE_RESULT ACEResultBufferTooSmall        = RES_AGENT_OFFSET + 5;
ACE_RESULT ACEResultUnsupportedVersion    = RES_AGENT_OFFSET + 6;