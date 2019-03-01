#pragma once

#include "GPUStage.hpp"

class RenderStage : public GPUStage
{
public:
	RenderStage();
	virtual ~RenderStage();
	void Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig);

	void Run(int index, Project* p);
private:

};