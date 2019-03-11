#pragma once

#include "ConstantsAndGlobals.hpp"
#include "StructsAndEnums.hpp"

class Project;

class GPUStage
{
public:
	GPUStage() {}
	virtual ~GPUStage() {}

	virtual void Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig) = 0;

	virtual void Run(int swapBufferIndex, int threadIndex, Project* p) = 0;
protected:
	ID3D12PipelineState* mPipelineState = nullptr;
};