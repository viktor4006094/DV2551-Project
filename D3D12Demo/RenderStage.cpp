#include "RenderStage.hpp"
#include "Project.hpp"

RenderStage::RenderStage()
{

}


RenderStage::~RenderStage()
{

}

void RenderStage::Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig)
{
	////// Shader Compiles //////
	ID3DBlob* vertexBlob;
	D3DCompileFromFile(
		L"VertexShader.hlsl", // filename
		nullptr,		// optional macros
		nullptr,		// optional include files
		"VS_main",		// entry point
		"vs_5_0",		// shader model (target)
		0,				// shader compile options			// here DEBUGGING OPTIONS
		0,				// effect compile options
		&vertexBlob,	// double pointer to ID3DBlob		
		nullptr			// pointer for Error Blob messages.
						// how to use the Error blob, see here
						// https://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
	);

	ID3DBlob* pixelBlob;
	D3DCompileFromFile(
		L"PixelShader.hlsl", // filename
		nullptr,		// optional macros
		nullptr,		// optional include files
		"PS_main",		// entry point
		"ps_5_0",		// shader model (target)
		0,				// shader compile options			// here DEBUGGING OPTIONS
		0,				// effect compile options
		&pixelBlob,		// double pointer to ID3DBlob		
		nullptr			// pointer for Error Blob messages.
						// how to use the Error blob, see here
						// https://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
	);

	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"NOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
	inputLayoutDesc.pInputElementDescs = inputElementDesc;
	inputLayoutDesc.NumElements = ARRAYSIZE(inputElementDesc);

	////// Pipline State //////
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsd = {};

	//Specify pipeline stages:
	gpsd.pRootSignature = rootSig;
	gpsd.InputLayout = inputLayoutDesc;
	gpsd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsd.VS.pShaderBytecode = reinterpret_cast<void*>(vertexBlob->GetBufferPointer());
	gpsd.VS.BytecodeLength = vertexBlob->GetBufferSize();
	gpsd.PS.pShaderBytecode = reinterpret_cast<void*>(pixelBlob->GetBufferPointer());
	gpsd.PS.BytecodeLength = pixelBlob->GetBufferSize();

	//Specify render target and depthstencil usage.
	gpsd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsd.NumRenderTargets = 1;

	gpsd.SampleDesc.Count = 1;
	gpsd.SampleMask = UINT_MAX;

	//Specify rasterizer behaviour.
	gpsd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpsd.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

	// specify depth stencil state

	D3D12_DEPTH_STENCIL_DESC dsd = {};
	dsd.DepthEnable = TRUE;
	dsd.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	gpsd.DepthStencilState = dsd;
	gpsd.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//Specify blend descriptions.
	D3D12_RENDER_TARGET_BLEND_DESC defaultRTdesc = {
		false, false,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL };
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		gpsd.BlendState.RenderTarget[i] = defaultRTdesc;

	dev->CreateGraphicsPipelineState(&gpsd, IID_PPV_ARGS(&mPipelineState));
}

void RenderStage::Run(UINT64 frameIndex, int swapBufferIndex, int threadIndex, Project* p)
{
	static size_t lastRenderIterationIndex = 0;

	//UINT backBufferIndex = p->gSwapChain4->GetCurrentBackBufferIndex();
	//UINT backBufferIndex = index;
	
	PerFrameResources* perFrame = &p->gPerFrameResources[swapBufferIndex];


	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	ID3D12CommandAllocator* directAllocator = p->gAllocatorsAndLists[threadIndex][QUEUE_TYPE_DIRECT].mAllocator;
	D3D12GraphicsCommandListPtr	directList = p->gAllocatorsAndLists[threadIndex][QUEUE_TYPE_DIRECT].mCommandList;

	ID3D12Fence1* fence = p->gCommandQueues[QUEUE_TYPE_DIRECT].mFence;
	HANDLE eventHandle = p->gAllocatorsAndLists[threadIndex][QUEUE_TYPE_DIRECT].mEventHandle;

	////! since WaitForGPU is called just before the commandlist is executed in the previous frame this does 
	////! not need to be done here since the command allocator is already guaranteed to have finished executing
	//// wait for previous usage of this command allocator to be done executing before it is reset
	//UINT64 prevFence = gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mLastFrameWithThisAllocatorFenceValue;
	//if (fence->GetCompletedValue() < prevFence) {
	//	fence->SetEventOnCompletion(prevFence, eventHandle);
	//	WaitForSingleObject(eventHandle, INFINITE);
	//}

	directAllocator->Reset();
	directList->Reset(directAllocator, mPipelineState);

#ifdef RECORD_TIME
	QueryPerformanceCounter(&p->mCPUTimeStamps[frameIndex][0].Start);

	// timer start
	p->gpuTimer[0].start(directList, frameIndex);
#endif

	//Set root signature
	directList->SetGraphicsRootSignature(p->gRootSignature);


	// Indicate that the intermediate buffer will be used as a render target
	SetResourceTransitionBarrier(directList, perFrame->gIntermediateRenderTarget,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);


	//Record commands.
	//Get the handle for the current render target in the intermediate output buffer.
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = p->gIntermediateRenderTargetsDescHeap->GetCPUDescriptorHandleForHeapStart();
	cdh.ptr += p->gRenderTargetDescriptorSize * swapBufferIndex;

	// get a handle to the depth/stencil buffer
	D3D12_CPU_DESCRIPTOR_HANDLE dsvh = p->dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	directList->ClearDepthStencilView(p->dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// set and clear render targets and depth stencil
	float clearColor[4] = { 0.0f,0.0f,0.0f,1.0f };
	directList->OMSetRenderTargets(1, &cdh, false, &dsvh);
	//directList->ClearRenderTargetView(cdh,clearColor, 0, nullptr);
	//directList->OMSetRenderTargets(1, &cdh, true, nullptr);
	directList->ClearRenderTargetView(cdh, gClearColor, 0, nullptr);




	//Set root signature
	directList->SetGraphicsRootSignature(p->gRootSignature);

	//Set necessary states.
	directList->RSSetViewports(1, &p->gViewport);
	directList->RSSetScissorRects(1, &p->gScissorRect);



	directList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	directList->IASetVertexBuffers(0, 2, p->gVertexBufferView);

	//get the current game state

	//->mGameStateHandler.writeNewestGameStateToReadOnlyAtIndex(index);
	/*gBufferTransferLock.lock();
	readOnlyState[index] = bufferState;
	gBufferTransferLock.unlock();*/

	//Update constant buffers and draw triangles
	//for (int i = 0; i < 100; ++i) {

	//D3D12_GPU_DESCRIPTOR_HANDLE gdh = p->gRenderTargetsHeap->GetGPUDescriptorHandleForHeapStart();

	D3D12_GPU_VIRTUAL_ADDRESS gpuVir = p->gConstantBufferResource[swapBufferIndex]->GetGPUVirtualAddress();

	for(int i = 0; i < TOTAL_TRIS; ++i) {
	//for (auto &m : p->mGameStateHandler.getReadOnlyStateAtIndex(index)->meshes) {
		directList->SetGraphicsRootConstantBufferView(0, gpuVir);
		//directList->SetComputeRootConstantBufferView(0, gpuVir);
		gpuVir += sizeof(CONSTANT_BUFFER_DATA);

		/*directList->SetGraphicsRoot32BitConstants(0, 4, &m.translate, 0);
		directList->SetGraphicsRoot32BitConstants(1, 4, &m.color, 0);*/

		//directList->SetGraphicsRootDescriptorTable(4, gdh);
		//gdh.ptr += p->gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		directList->DrawInstanced(300'000, 1, 0, 0);
	}

	// set state to common since this is used in by the compute queue as well
	SetResourceTransitionBarrier(directList, perFrame->gIntermediateRenderTarget,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COMMON
	);


#ifdef RECORD_TIME
	// timer end
	p->gpuTimer[0].stop(directList, frameIndex);
	p->gpuTimer[0].resolveQueryToCPU(directList, frameIndex);

	QueryPerformanceCounter(&p->mCPUTimeStamps[frameIndex][0].Stop);
#endif

	//Close the list to prepare it for execution.
	directList->Close();

	//wait for current frame to finish rendering before pushing the next one to GPU
	//p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	//Execute the command list.
	ID3D12CommandList* listsToExecute[] = { directList };
	p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);
}