#pragma once

#include "GPUStage.hpp"

class PassThroughStage : public GPUStage
{
public:
	PassThroughStage();
	virtual ~PassThroughStage();
	void Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig);

	void Run(int index, Project* p);
private:

};