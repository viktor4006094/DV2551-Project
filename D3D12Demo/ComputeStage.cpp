#include "ComputeStage.hpp"
#include "Project.hpp"

ComputeStage::ComputeStage()
{

}

ComputeStage::~ComputeStage()
{

}

void ComputeStage::Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig)
{
	std::string widthString = std::to_string(SCREEN_WIDTH);
	LPCSTR widthLPCSTR = widthString.c_str();
	std::string heightString = std::to_string(SCREEN_HEIGHT);
	LPCSTR heightLPCSTR = heightString.c_str();

	D3D_SHADER_MACRO computeDefines[] =
	{
		"TEXTURE_WIDTH", widthLPCSTR,
		"TEXTURE_HEIGHT", heightLPCSTR,
		NULL, NULL
	};

	/*D3D_SHADER_MACRO computeDefines[] =
	{
		"TEXTURE_WIDTH", "640",
		"TEXTURE_HEIGHT", "480",
		NULL, NULL
	};*/

	////// Shader Compiles //////
	ID3DBlob* computeBlob;
	ID3DBlob* errorBlob;
	HRESULT hr = D3DCompileFromFile(
		L"CS_FXAA.hlsl", // filename
		computeDefines,	// optional macros
		nullptr,		// optional include files
		"FXAA_main",	// entry point
		"cs_5_0",		// shader model (target)
		0,				// shader compile options			// here DEBUGGING OPTIONS
		0,				// effect compile options
		&computeBlob,	// double pointer to ID3DBlob		
		&errorBlob			// pointer for Error Blob messages.
						// how to use the Error blob, see here
						// https://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
	);

	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	////// Pipline State //////
	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};

	//Specify pipeline stages:
	cpsd.pRootSignature = rootSig;
	cpsd.CS.pShaderBytecode = reinterpret_cast<void*>(computeBlob->GetBufferPointer());
	cpsd.CS.BytecodeLength = computeBlob->GetBufferSize();


	hr = dev->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&mPipelineState));
	if (FAILED(hr))
	{
		
	}

	//todo release pointers
}

void ComputeStage::Run(UINT64 frameIndex, int swapBufferIndex, int threadIndex, Project* p)
{
	PerFrame* frame = &p->gPerFrameAllocatorsListsAndResources[swapBufferIndex];
	ID3D12CommandAllocator*     compAllo = frame->mAllocatorsAndLists[FXAA_STAGE].mAllocator;
	D3D12GraphicsCommandListPtr compList = frame->mAllocatorsAndLists[FXAA_STAGE].mCommandList;

	//// Compute shader part ////

	compAllo->Reset();
	compList->Reset(compAllo, mPipelineState);
	
#ifdef RECORD_TIME
	if (frameIndex >= FIRST_TIMESTAMPED_FRAME && frameIndex < (NUM_TIMESTAMP_PAIRS + FIRST_TIMESTAMPED_FRAME)) {
		int arrIndex = frameIndex - FIRST_TIMESTAMPED_FRAME;
		QueryPerformanceCounter(&p->mCPUTimeStamps[arrIndex][1].Start);

		// timer start
		p->gpuTimer[1].start(compList, arrIndex);
	}
#endif
	//Set root signature
	compList->SetComputeRootSignature(p->gRootSignature);

	// Set the correct state for the input, will be set to common between queue types
	SetResourceTransitionBarrier(compList, frame->gIntermediateRenderTarget,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	);


	ID3D12DescriptorHeap* dheap1[] = { p->gComputeDescriptorHeap };
	compList->SetDescriptorHeaps(_countof(dheap1), dheap1);

	D3D12_GPU_DESCRIPTOR_HANDLE gdh = p->gComputeDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	// todo save increment size
	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*swapBufferIndex;
	compList->SetComputeRootDescriptorTable(2, gdh);
	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*(NUM_SWAP_BUFFERS - swapBufferIndex);
	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*swapBufferIndex;
	compList->SetComputeRootDescriptorTable(1, gdh);


	SetResourceTransitionBarrier(compList, frame->gUAVResource,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);


	static const UINT squaresWide = SCREEN_WIDTH / 32U + 1;
	static const UINT squaresHigh = SCREEN_HEIGHT / 32U + 1;

	compList->Dispatch(squaresWide, squaresHigh, 1);

	SetResourceTransitionBarrier(compList, frame->gUAVResource,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);


	// set state to common since this is used in by the direct queue as well
	SetResourceTransitionBarrier(compList, frame->gIntermediateRenderTarget,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COMMON
	);


#ifdef RECORD_TIME
	if (frameIndex >= FIRST_TIMESTAMPED_FRAME && frameIndex < (NUM_TIMESTAMP_PAIRS + FIRST_TIMESTAMPED_FRAME)) {
		int arrIndex = frameIndex - FIRST_TIMESTAMPED_FRAME;
		// timer end
		p->gpuTimer[1].stop(compList, arrIndex);
		p->gpuTimer[1].resolveQueryToCPU(compList, arrIndex);

		QueryPerformanceCounter(&p->mCPUTimeStamps[arrIndex][1].Stop);
	}
#endif

	//Close the list to prepare it for execution.
	compList->Close();


	// Wait for the geometry stage to be finished
	p->gCommandQueues[QT_COMP]->Wait(p->gSwapBufferFences[swapBufferIndex], p->gSwapBufferFenceValues[swapBufferIndex]);

	//Execute the command list.
	ID3D12CommandList* listsToExecute2[] = { compList };
	p->gCommandQueues[QT_COMP]->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);
}