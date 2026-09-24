// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

#include "ace.h"

struct ID3D12Device;
struct ID3D12CommandQueue;

// Dedicated log category for the ACE LLM plugin. Filter the Output Log with
// "LogACELLM" to see only plugin diagnostics. Default verbosity is Log; all
// levels are compiled in so Verbose/VeryVerbose can be enabled at runtime via
// `Log LogACELLM Verbose`.
DECLARE_LOG_CATEGORY_EXTERN(LogACELLM, Log, All);

// Convert an ACEResult status code to a short, human readable string.
const TCHAR* ACEResultToString(uint32 Result);

class LLMModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// single global context
	ACEContext* GetContext();
	void        SetContext(ACEContext* ctx);

	// D3D12 Device used to create ACE ctx
	ID3D12Device*       GetD3D12Device();
	void                SetD3D12Device(ID3D12Device* dev);

	// D3D12 Queue used to create ACE ctx
	ID3D12CommandQueue* GetD3D12CommandQueue();
	void                SetD3D12CommandQueue(ID3D12CommandQueue* queue);

	// When true, the plugin created m_d3d12CommandQueue itself (a dedicated CIG
	// queue) and is responsible for releasing it at shutdown. When false, the
	// queue is the RHI-owned primary graphics queue and must not be released.
	void                SetOwnsCommandQueue(bool bOwns);

	// currently loaded model
	ACEModel* GetModel();
	void      SetModel(ACEModel* model);

	static FName GetModuleName() { return TEXT("LLMPlugin"); }

private:

	ACEContext*               m_context{};
	ID3D12Device*             m_d3d12Device{};
	ID3D12CommandQueue*       m_d3d12CommandQueue{};
	bool                      m_bOwnsCommandQueue{false};

	// New Low Level API
	ACEModel*            m_model{};
};

