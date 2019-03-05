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
	D3D_SHADER_MACRO computeDefines[] =
	{
		"TEXTURE_WIDTH", "640",
		"TEXTURE_HEIGHT", "480",
		NULL, NULL
	};


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
	UINT backBufferIndex = p->gSwapChain4->GetCurrentBackBufferIndex();
	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	ID3D12CommandAllocator* directAllocator = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mAllocator;
	D3D12GraphicsCommandListPtr directList = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mCommandList;


	//// Compute shader part ////

	p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	directAllocator->Reset();
	directList->Reset(directAllocator, mPipelineState);

	//Set root signature
	directList->SetComputeRootSignature(p->gRootSignature);

	SetResourceTransitionBarrier(directList,
		p->gRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_RENDER_TARGET,				//state before
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE		//state after
	);


	ID3D12DescriptorHeap* dheap1[] = { p->gComputeDescriptorHeap };
	directList->SetDescriptorHeaps(_countof(dheap1), dheap1);

	D3D12_GPU_DESCRIPTOR_HANDLE gdh = p->gComputeDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	directList->SetComputeRootDescriptorTable(3, gdh);

	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*backBufferIndex;
	directList->SetComputeRootDescriptorTable(2, gdh);


	static const UINT squaresWide = SCREEN_WIDTH / 40U;
	static const UINT squaresHigh = SCREEN_HEIGHT / 20U;

	directList->Dispatch(squaresWide, squaresHigh, 1);
	//directList->Dispatch(16, 24, 1);

	SetResourceTransitionBarrier(directList,
		p->gUAVResource,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,		//state before
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE		//state after
	);


	//Indicate that the back buffer will be used as render target.
	SetResourceTransitionBarrier(directList,
		p->gRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,		//state before
		D3D12_RESOURCE_STATE_RENDER_TARGET		//state after
		//todo transition to present here when passthrough isn't used
		//D3D12_RESOURCE_STATE_PRESENT		//state after 
	);


	//Close the list to prepare it for execution.
	directList->Close();

	//wait for current frame to finish rendering before pushing the next one to GPU
	p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	// set the just executed command allocator and list to inactive
	//gAllocatorsAndLists[(index + MAX_THREAD_COUNT - 1) % MAX_THREAD_COUNT][QUEUE_TYPE_DIRECT].isActive = false;

	//Execute the command list.
	ID3D12CommandList* listsToExecute2[] = { directList };
	p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);
}