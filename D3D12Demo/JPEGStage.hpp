#pragma once

#include "GPUStage.hpp"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

class JPEGStage : public GPUStage
{
public:
	JPEGStage();
	virtual ~JPEGStage();

	void Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig);

	void Run(int index, Project* p);
private:
	ID3D12PipelineState* mCbPipelineState = nullptr;
	ID3D12PipelineState* mCrPipelineState = nullptr;

	class JpegEncoderGPU* mJPEGEncoder = nullptr;
	
};