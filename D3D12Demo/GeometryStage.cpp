#include "GeometryStage.hpp"
#include "Project.hpp"

GeometryStage::GeometryStage()
{
}


GeometryStage::~GeometryStage()
{
	SafeRelease(&mPipelineState);
}

void GeometryStage::Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig)
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
		{ "NOR",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
	inputLayoutDesc.pInputElementDescs = inputElementDesc;
	inputLayoutDesc.NumElements = ARRAYSIZE(inputElementDesc);

	////// Pipline State //////
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsd = {};

	//Specify pipeline stages:
	gpsd.pRootSignature = rootSig;
	gpsd.InputLayout    = inputLayoutDesc;
	gpsd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsd.VS.pShaderBytecode = reinterpret_cast<void*>(vertexBlob->GetBufferPointer());
	gpsd.VS.BytecodeLength  = vertexBlob->GetBufferSize();
	gpsd.PS.pShaderBytecode = reinterpret_cast<void*>(pixelBlob->GetBufferPointer());
	gpsd.PS.BytecodeLength  = pixelBlob->GetBufferSize();

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
	dsd.DepthEnable    = TRUE;
	dsd.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;

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

void GeometryStage::Run(UINT64 frameIndex, int swapBufferIndex, int threadIndex, Project* p)
{
	PerFrame* frame = &p->gPerFrameAllocatorsListsAndResources[swapBufferIndex];

	ID3D12CommandList* listsToExecute[LISTS_PER_FRAME];
	std::future<void> workers[LISTS_PER_FRAME];

	bool recordTime = false;

#ifdef RECORD_TIME
	int arrIndex = frameIndex - FIRST_TIMESTAMPED_FRAME;
	if (frameIndex >= FIRST_TIMESTAMPED_FRAME && frameIndex < (NUM_TIMESTAMP_PAIRS + FIRST_TIMESTAMPED_FRAME)) {
		recordTime = true;
		int arrIndex = frameIndex - FIRST_TIMESTAMPED_FRAME;
		QueryPerformanceCounter(&p->mCPUTimeStamps[arrIndex][0].Start);
	}
#endif

	// Start recording command lists
	for (int i = 0; i < LISTS_PER_FRAME; ++i) {
		workers[i] = p->gThreadPool->push([=](int id) {this->RenderMeshes(id, frameIndex, swapBufferIndex, threadIndex, p, i, recordTime); });
	}

	// Wait for command lists to finish recording
	for (int i = 0; i < LISTS_PER_FRAME; ++i) {
		workers[i].wait();
		listsToExecute[i] = frame->mAllocatorsAndLists[GEOMETRY_STAGE].mCommandLists[i];
	}

#ifdef RECORD_TIME
	if (recordTime) {
		QueryPerformanceCounter(&p->mCPUTimeStamps[arrIndex][0].Stop);
	}
#endif

	//Execute the command lists.
	p->gCommandQueues[QT_DIR]->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);
}


void GeometryStage::RenderMeshes(int id, const UINT64 frameIndex, const int swapBufferIndex, const int threadIndex, Project* p, const int section, const bool recordTime)
{
	PerFrame* frame = &p->gPerFrameAllocatorsListsAndResources[swapBufferIndex];
	ID3D12CommandAllocator*     dirAllo = frame->mAllocatorsAndLists[GEOMETRY_STAGE].mAllocators[section];
	D3D12GraphicsCommandListPtr dirList = frame->mAllocatorsAndLists[GEOMETRY_STAGE].mCommandLists[section];

	dirAllo->Reset();
	dirList->Reset(dirAllo, mPipelineState);

#ifdef RECORD_TIME
	// start recording GPU time
	int arrIndex = frameIndex - FIRST_TIMESTAMPED_FRAME;
	if (section == 0 && recordTime) {
		// timer start
		p->gpuTimer[0].start(dirList, arrIndex);
	}
#endif

	//Set root signature
	dirList->SetGraphicsRootSignature(p->gRootSignature);


	if (section == 0) {
		// Indicate that the intermediate buffer will be used as a render target
		SetResourceTransitionBarrier(dirList, frame->gIntermediateRenderTarget,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
	}

	//Record commands.
	//Get the handle for the current render target in the intermediate output buffer.
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = p->gIntermediateRenderTargetsDescHeap->GetCPUDescriptorHandleForHeapStart();
	cdh.ptr += p->gRenderTargetDescriptorSize * swapBufferIndex;

	// get a handle to the depth/stencil buffer
	D3D12_CPU_DESCRIPTOR_HANDLE dsvh = p->dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	
	if (section == 0) {
		dirList->ClearDepthStencilView(p->dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}
	// set and clear render targets and depth stencil
	dirList->OMSetRenderTargets(1, &cdh, false, &dsvh);
	if (section == 0) {
		dirList->ClearRenderTargetView(cdh, gClearColor, 0, nullptr);
	}

	//Set necessary states.
	dirList->RSSetViewports(1, &p->gViewport);
	dirList->RSSetScissorRects(1, &p->gScissorRect);

	// Set the vertex buffers (positions and normals)
	dirList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	dirList->IASetVertexBuffers(0, 2, p->gVertexBufferViews);


	D3D12_GPU_VIRTUAL_ADDRESS gpuVir = p->gConstantBufferResource->GetGPUVirtualAddress();

	int startDragon = DRAGONS_PER_LIST * section;
	int endDragon = ((startDragon + 2*DRAGONS_PER_LIST) < TOTAL_DRAGONS) ? startDragon + DRAGONS_PER_LIST : TOTAL_DRAGONS;

	gpuVir += (startDragon * gConstBufferStructSize);

	for (int i = startDragon; i < endDragon; ++i) {
		// Set the per-mesh constant buffer
		dirList->SetGraphicsRootConstantBufferView(0, gpuVir);
		gpuVir += gConstBufferStructSize;

		dirList->DrawInstanced(300'000, 1, 0, 0);
	}

	if (section == LISTS_PER_FRAME - 1) {
		// Transition the intermediate rendertarget to common so that 
		SetResourceTransitionBarrier(dirList, frame->gIntermediateRenderTarget,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COMMON
		);
	}

#ifdef RECORD_TIME
	if (section == LISTS_PER_FRAME - 1 && recordTime) {
		// timer end
		p->gpuTimer[0].stop(dirList, arrIndex);
		p->gpuTimer[0].resolveQueryToCPU(dirList, arrIndex);
	}
#endif

	//Close the list to prepare it for execution.
	dirList->Close();
}