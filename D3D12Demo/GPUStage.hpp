#pragma once

#include "ConstantsAndGlobals.hpp"
#include "StructsAndEnums.hpp"

struct IDXGISwapChain4;
class Project;

class GPUStage
{
public:
	GPUStage() {}
	virtual ~GPUStage() {}

	void Stop() { isRunning = false; }

	virtual void Run(int index, Project* p) = 0;
protected:
	ID3D12PipelineState* mPipelineState = nullptr;
	bool isRunning = true;
};