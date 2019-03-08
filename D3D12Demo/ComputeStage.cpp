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

void ComputeStage::Run(int index, Project* p)
{
	// wait for the render pass of this frame to be finished
	//p->gIntraFrameFence[index].WaitForPrevFence();


	UINT backBufferIndex = p->gSwapChain4->GetCurrentBackBufferIndex();
	//UINT backBufferIndex = index;


	//UINT backBufferIndex = index;


	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)

	ID3D12CommandAllocator* computeAllocator = p->gAllocatorsAndLists[index][QUEUE_TYPE_COMPUTE].mAllocator;
	D3D12GraphicsCommandListPtr computeList  = p->gAllocatorsAndLists[index][QUEUE_TYPE_COMPUTE].mCommandList;



	//// Compute shader part ////

	//p->gCommandQueues[QUEUE_TYPE_COMPUTE].WaitForGpu();

	computeAllocator->Reset();
	computeList->Reset(computeAllocator, mPipelineState);

	//Set root signature
	computeList->SetComputeRootSignature(p->gRootSignature);

	// Set the correct state for the input, will be set to common between queue types
	SetResourceTransitionBarrier(computeList, p->gIntermediateRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	);


	ID3D12DescriptorHeap* dheap1[] = { p->gComputeDescriptorHeap };
	computeList->SetDescriptorHeaps(_countof(dheap1), dheap1);

	D3D12_GPU_DESCRIPTOR_HANDLE gdh = p->gComputeDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*backBufferIndex;
	computeList->SetComputeRootDescriptorTable(2, gdh);
	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*(NUM_SWAP_BUFFERS-backBufferIndex);
	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*backBufferIndex;
	computeList->SetComputeRootDescriptorTable(1, gdh);


	SetResourceTransitionBarrier(computeList, p->gUAVResource[backBufferIndex],
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);


	static const UINT squaresWide = SCREEN_WIDTH / 40U;
	static const UINT squaresHigh = SCREEN_HEIGHT / 20U;

	computeList->Dispatch(squaresWide, squaresHigh, 1);

	SetResourceTransitionBarrier(computeList, p->gUAVResource[backBufferIndex],
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);


	// set state to common since this is used in by the direct queue as well
	SetResourceTransitionBarrier(computeList, p->gIntermediateRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COMMON
	);


	//Close the list to prepare it for execution.
	computeList->Close();



	//wait for current frame to finish rendering before using it in the compute pass
	//p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	// wait for the previous usage of this compute shader output texture to be free to use
	//p->gUAVFence[index].WaitForPrevFence();

	

	//Execute the command list.
	ID3D12CommandList* listsToExecute2[] = { computeList };
	p->gCommandQueues[QUEUE_TYPE_COMPUTE].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);


	// signal that the pixel shader output texture is free to use again
	//p->gIntermediateBufferFence[index].SignalFence(p->gCommandQueues[QUEUE_TYPE_COMPUTE].mQueue);

	//p->gIntraFrameFence[index].SignalFence(p->gCommandQueues[QUEUE_TYPE_COMPUTE].mQueue);

}