#pragma once

#include "GPUStage.hpp"

class ComputeStage : public GPUStage
{
public:
	ComputeStage();
	virtual ~ComputeStage();

	void Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig);

	void Run(int index, Project* p);
private:

};