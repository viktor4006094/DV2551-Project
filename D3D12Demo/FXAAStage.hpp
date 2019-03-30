#pragma once

#include "GPUStage.hpp"

class FXAAStage : public GPUStage
{
public:
	FXAAStage();
	virtual ~FXAAStage();

	void Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig);

	void Run(UINT64 frameIndex, int swapBufferIndex, int threadIndex, Project* p);
private:
	UINT mDescriptorHandleSize = 0;
};