#include "PassThroughStage.hpp"
#include "Project.hpp"


PassThroughStage::PassThroughStage()
{

}

PassThroughStage::~PassThroughStage()
{

}

void PassThroughStage::Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig)
{
	////// Shader Compiles //////
	ID3DBlob* vertexBlob;
	D3DCompileFromFile(
		L"VS_FullScreenTriangle.hlsl", // filename
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
		L"PS_Passthrough.hlsl", // filename
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

	////// Input Layout //////
	//D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "COLOR"	, 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	//};	


	// Empty input layout
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

void PassThroughStage::Run(int index, Project* p)
{
	UINT backBufferIndex = p->gSwapChain4->GetCurrentBackBufferIndex();
	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	ID3D12CommandAllocator* directAllocator = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mAllocator;
	D3D12GraphicsCommandListPtr	directList = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mCommandList;


	//// Passthrough ////

	p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	directAllocator->Reset();
	directList->Reset(directAllocator, mPipelineState);

	//Set root signature
	directList->SetGraphicsRootSignature(p->gRootSignature);

	//Set necessary states.
	directList->RSSetViewports(1, &p->gViewport);
	directList->RSSetScissorRects(1, &p->gScissorRect);

	////Indicate that the back buffer will be used as render target.
	//SetResourceTransitionBarrier(directList,
	//	gRenderTargets[backBufferIndex],
	//	D3D12_RESOURCE_STATE_PRESENT,		//state before
	//	D3D12_RESOURCE_STATE_RENDER_TARGET	//state after
	//);

	//Record commands.
	//Get the handle for the current render target used as back buffer.
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = p->gRenderTargetsHeap->GetCPUDescriptorHandleForHeapStart();
	cdh.ptr += p->gRenderTargetDescriptorSize * backBufferIndex;

	directList->OMSetRenderTargets(1, &cdh, true, nullptr);

	float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	directList->ClearRenderTargetView(cdh, clearColor, 0, nullptr);

	directList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* dheap[] = { p->gComputeDescriptorHeap };
	directList->SetDescriptorHeaps(_countof(dheap), dheap);
	directList->SetGraphicsRootDescriptorTable(2, p->gComputeDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	directList->DrawInstanced(3, 1, 0, 0);


	//Indicate that the back buffer will now be used to present.
	SetResourceTransitionBarrier(directList,
		p->gRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_RENDER_TARGET,	//state before
		D3D12_RESOURCE_STATE_PRESENT		//state after
	);

	SetResourceTransitionBarrier(directList,
		p->gUAVResource,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,		//state before
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS		//state after
	);

	//Close the list to prepare it for execution.
	directList->Close();

	//wait for current frame to finish rendering before pushing the next one to GPU
	p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	// set the just executed command allocator and list to inactive
	//gAllocatorsAndLists[(index + MAX_THREAD_COUNT - 1) % MAX_THREAD_COUNT][QUEUE_TYPE_DIRECT].isActive = false;

	//Execute the command list.
	ID3D12CommandList* listsToExecute3[] = { directList };
	p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute3), listsToExecute3);
}