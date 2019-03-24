#include "Project.hpp"
#include "RenderStage.hpp"
#include "ComputeStage.hpp"
#include "DragonTriangles.h"
#include <chrono>
#include <ctime>
#include <string>

#include <sstream>


Project::Project()
{
}

Project::~Project()
{

}

void Project::Init(HWND wndHandle)
{
	// copy window handle so that we can change the title of the window later
	mWndHandle = wndHandle;

	gThreadPool = new ctpl::thread_pool(NUM_THREADS);

	GPUStages[0] = new RenderStage();
	GPUStages[1] = new ComputeStage();

	CreateDirect3DDevice(wndHandle);					//2. Create Device
	CreateCommandInterfacesAndSwapChain(wndHandle);		//3. Create CommandQueue and SwapChain
	CreateFencesAndEventHandles();						//4. Create Fence and Event handle
	CreateRenderTargets();								//5. Create render targets for backbuffer
	CreateViewportAndScissorRect();						//6. Create viewport and rect
	CreateRootSignature();								//7. Create root signature
	CreateShadersAndPipelineStates();					//8. Set up the pipeline state
	CreateConstantBufferResources();					//9. Create constant buffer data
	UploadMeshData();									//10. Uploads the mesh data to the GPU
	mGameStateHandler.CreatePerMeshData();				//11. Set the colors and positions of the meshes
	CreateComputeShaderResources();						//12. Create UAVs and SRVs for the FXAA stage


	// todo move elsewhere?
#ifdef _DEBUG
	// Set names to make debugging easier
	std::wstring intStr = L"InterMediateRenderTarget";
	std::wstring uavStr = L"UAVResource";
	std::wstring swapStr = L"SwapChainRenderTarget";
	HRESULT hr = S_OK;
	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		std::wstring wsInt = intStr + std::to_wstring(i);
		LPCWSTR cInt = wsInt.c_str();
		hr = gPerFrameAllocatorsListsAndResources[i].gIntermediateRenderTarget->SetName(cInt);


		std::wstring wsUAV = uavStr + std::to_wstring(i);
		LPCWSTR cUAV = wsUAV.c_str();
		hr = gPerFrameAllocatorsListsAndResources[i].gUAVResource->SetName(cUAV);

		std::wstring wsSwap = swapStr + std::to_wstring(i);
		LPCWSTR cSwap = wsSwap.c_str();
		hr = gSwapChainRenderTargets[i]->SetName(cSwap);
	}


	gCommandQueues[QT_DIR]->SetName(L"DirectQueue");
	gCommandQueues[QT_COMP]->SetName(L"ComputeQueue");
#endif
#ifdef RECORD_TIME
	gpuTimer[0].init(gDevice5, 100);
	gpuTimer[1].init(gDevice5, 100);
	gpuTimer[2].init(gDevice5, 100);
#endif

}

void Project::Start()
{
	gThreadPool->push([this](int id) {mGameStateHandler.Update(id); });
	gThreadPool->push([this](int id) {this->Render(id); });
#ifdef RECORD_TIME
	gCommandQueues[QT_DIR]->GetClockCalibration(&mClockCalibration.gpuTimeStamp, &mClockCalibration.cpuTimeStamp);
#endif
}

void Project::Stop()
{
	isRunning = false;
}


void Project::Shutdown()
{
	//todo wait for gpu here instead of sleep in main()
	mGameStateHandler.ShutDown();


	gConstantBufferResource->Unmap(0, nullptr);


	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		gPerFrameAllocatorsListsAndResources[i].Release();
		SafeRelease(&gSwapBufferFences[i]);
		SafeRelease(&gSwapChainRenderTargets[i]);
	}

	SafeRelease(&gSwapChain4);
	SafeRelease(&gSwapChainRenderTargetsDescHeap);
	SafeRelease(&gConstantBufferDescriptorHeap);
	SafeRelease(&gConstantBufferResource);
	SafeRelease(&gDevice5);
	SafeRelease(&gCommandQueues[0]);
	SafeRelease(&gCommandQueues[1]);
	SafeRelease(&depthStencilBuffer);
	SafeRelease(&dsDescriptorHeap);
	SafeRelease(&gVertexBufferResource);
	SafeRelease(&gVertexBufferNormalResource);
	SafeRelease(&gVertexStagingBufferResource);
	SafeRelease(&gNormalStagingBufferResource);
	SafeRelease(&gRootSignature);
	SafeRelease(&gBackBufferFence);
	SafeRelease(&gIntermediateRenderTargetsDescHeap);
	SafeRelease(&gComputeDescriptorHeap);

	delete GPUStages[0];
	delete GPUStages[1];
	delete gThreadPool;

#ifdef _DEBUG
	IDXGIDebug1* dxgiDebug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY || DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		dxgiDebug->Release();
	}
#endif
}


void Project::CreateDirect3DDevice(HWND wndHandle)
{
#ifdef _DEBUG
	//Enable the D3D12 debug layer.
	ID3D12Debug* debugController = nullptr;

#ifdef STATIC_LINK_DEBUGSTUFF
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
	SafeRelease(debugController);
#else
	HMODULE mD3D12 = GetModuleHandle(L"D3D12.dll");
	PFN_D3D12_GET_DEBUG_INTERFACE f = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(mD3D12, "D3D12GetDebugInterface");
	if (SUCCEEDED(f(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

	}
	SafeRelease(&debugController);
#endif
#endif

	//dxgi1_6 is only needed for the initialization process using the adapter.
	IDXGIFactory6*	factory = nullptr;
	IDXGIAdapter1*	adapter = nullptr;
	//First a factory is created to iterate through the adapters available.
	CreateDXGIFactory(IID_PPV_ARGS(&factory));
	for (UINT adapterIndex = 0;; ++adapterIndex)
	{
		adapter = nullptr;
		if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter))
		{
			break; //No more adapters to enumerate.
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(gDevice5), nullptr)))
		{
			break;
		}

		SafeRelease(&adapter);
	}
	if (adapter)
	{
		HRESULT hr = S_OK;
		//Create the actual device.
		if (SUCCEEDED(hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&gDevice5))))
		{

		}

		SafeRelease(&adapter);
	}
	else
	{
		//Create warp device if no adapter was found.
		factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
		D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&gDevice5));
	}

	SafeRelease(&factory);
}


void Project::CreateCommandInterfacesAndSwapChain(HWND wndHandle)
{
	//Describe and create the command queues.
	D3D12_COMMAND_QUEUE_DESC cqd = {};

	cqd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	gDevice5->CreateCommandQueue(&cqd, IID_PPV_ARGS(&gCommandQueues[QT_DIR]));

	cqd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	gDevice5->CreateCommandQueue(&cqd, IID_PPV_ARGS(&gCommandQueues[QT_COMP]));


	//Create command allocator. The command allocator object corresponds
	//to the underlying allocations in which GPU commands are stored.
	//Create command list.
	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		gPerFrameAllocatorsListsAndResources[i].CreateCommandListsAndAllocators(gDevice5);
	}

	IDXGIFactory5*	factory = nullptr;
	CreateDXGIFactory(IID_PPV_ARGS(&factory));

	//Create swap chain.
	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.Width = 0;
	scDesc.Height = 0;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.Stereo = FALSE;
	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.BufferCount = NUM_SWAP_BUFFERS;
	scDesc.Scaling = DXGI_SCALING_NONE;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Enable GetFrameLatencyWaitableObject();
	scDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	IDXGISwapChain1* swapChain1 = nullptr;
	
	HRESULT hr = factory->CreateSwapChainForHwnd(
		gCommandQueues[QT_DIR],
		wndHandle,
		&scDesc,
		nullptr,
		nullptr,
		&swapChain1);


	if (SUCCEEDED(hr)) {
		if (SUCCEEDED(swapChain1->QueryInterface(IID_PPV_ARGS(&gSwapChain4)))) {
			gSwapChain4->SetMaximumFrameLatency(MAX_FRAME_LATENCY);
			gSwapChainWaitableObject = gSwapChain4->GetFrameLatencyWaitableObject();
			
			swapChain1->Release();
		}
	}

	#pragma region depthStencil
	// depth stencil
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = gDevice5->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));


	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;



	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = SCREEN_WIDTH;
	rd.Height = SCREEN_HEIGHT;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rd.Format = DXGI_FORMAT_D32_FLOAT;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;


	//Creates both a resource and an implicit heap, such that the heap is big enough
	//to contain the entire resource and the resource is mapped to the heap. 
	hr = gDevice5->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&rd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&depthStencilBuffer));


	gDevice5->CreateDepthStencilView(depthStencilBuffer, &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	#pragma endregion

	SafeRelease(&factory);
}


void Project::CreateFencesAndEventHandles()
{
	// backbuffer fence created here since it's used in UploadMeshData()
	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		gSwapBufferFenceValues[i] = 0;
		gDevice5->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gSwapBufferFences[i]));

		// Create per swap buffer events and fence to present the frames in the correct order
		gBackBufferFenceEvent[i] = CreateEvent(0, false, false, 0);
	}

	gDevice5->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gBackBufferFence));
}

void Project::CreateRenderTargets()
{
	//Create descriptor heap for render target views.
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = NUM_SWAP_BUFFERS;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	HRESULT hr = gDevice5->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&gSwapChainRenderTargetsDescHeap));

	//Create resources for the render targets.
	gRenderTargetDescriptorSize = gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = gSwapChainRenderTargetsDescHeap->GetCPUDescriptorHandleForHeapStart();


	//One RTV for each frame.
	for (UINT n = 0; n < NUM_SWAP_BUFFERS; n++)
	{
		hr = gSwapChain4->GetBuffer(n, IID_PPV_ARGS(&gSwapChainRenderTargets[n]));
		gDevice5->CreateRenderTargetView(gSwapChainRenderTargets[n], nullptr, cdh);
		cdh.ptr += gRenderTargetDescriptorSize;
	}
}

void Project::CreateViewportAndScissorRect()
{
	gViewport.TopLeftX	= 0.0f;
	gViewport.TopLeftY	= 0.0f;
	gViewport.Width		= (float)SCREEN_WIDTH;
	gViewport.Height	= (float)SCREEN_HEIGHT;
	gViewport.MinDepth	= 0.0f;
	gViewport.MaxDepth	= 1.0f;

	gScissorRect.left	= (long)0;
	gScissorRect.right	= (long)SCREEN_WIDTH;
	gScissorRect.top	= (long)0;
	gScissorRect.bottom = (long)SCREEN_HEIGHT;
}

void Project::CreateShadersAndPipelineStates()
{
	GPUStages[0]->Init(gDevice5, gRootSignature);
	GPUStages[1]->Init(gDevice5, gRootSignature);
}

/// Uploads the mesh to the GPU via an upload heap and copies it to a default heap
void Project::UploadMeshData()
{
	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type				= D3D12_HEAP_TYPE_DEFAULT;
	hp.CreationNodeMask = 1;
	hp.VisibleNodeMask	= 1;

	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension		= D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width			= sizeof(triangleVertices);
	rd.Height			= 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels		= 1;
	rd.SampleDesc.Count = 1;
	rd.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//Creates both a resource and an implicit heap, such that the heap is big enough
	//to contain the entire resource and the resource is mapped to the heap. 
	gDevice5->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&rd,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&gVertexBufferResource));

	gVertexBufferResource->SetName(L"vb heap");

	gDevice5->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&rd,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&gVertexBufferNormalResource));

	gVertexBufferNormalResource->SetName(L"vbn heap");

	D3D12_HEAP_PROPERTIES hp2 = {};
	hp2.Type = D3D12_HEAP_TYPE_UPLOAD;
	hp2.CreationNodeMask = 1;
	hp2.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC rd2 = {};
	rd2.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd2.Width = sizeof(triangleVertices);
	rd2.Height = 1;
	rd2.DepthOrArraySize = 1;
	rd2.MipLevels = 1;
	rd2.SampleDesc.Count = 1;
	rd2.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	gDevice5->CreateCommittedResource(
		&hp2,
		D3D12_HEAP_FLAG_NONE,
		&rd2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&gVertexStagingBufferResource));

	gVertexStagingBufferResource->SetName(L"vertex staging heap");

	gDevice5->CreateCommittedResource(
		&hp2,
		D3D12_HEAP_FLAG_NONE,
		&rd2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&gNormalStagingBufferResource));

	gNormalStagingBufferResource->SetName(L"normal staging heap");

	//Copy the triangle data to the vertex buffer.
	void* dataBegin = nullptr;
	D3D12_RANGE range = { 0, 0 }; //We do not intend to read this resource on the CPU.
	gVertexStagingBufferResource->Map(0, &range, &dataBegin);
	memcpy(dataBegin, triangleVertices, sizeof(triangleVertices));
	gVertexStagingBufferResource->Unmap(0, nullptr);

	//Copy the triangle data to the vertex buffer.
	dataBegin = nullptr;
	range = { 0, 0 }; //We do not intend to read this resource on the CPU.
	gNormalStagingBufferResource->Map(0, &range, &dataBegin);
	memcpy(dataBegin, normalVertices, sizeof(normalVertices));
	gNormalStagingBufferResource->Unmap(0, nullptr);

	// copy pointers for easier to read code
	ID3D12CommandAllocator*		dirAllo = gPerFrameAllocatorsListsAndResources[0].mAllocatorsAndLists[GEOMETRY_STAGE].mAllocators[0];
	D3D12GraphicsCommandListPtr dirList = gPerFrameAllocatorsListsAndResources[0].mAllocatorsAndLists[GEOMETRY_STAGE].mCommandLists[0];

	//gAllocatorsAndLists[0][GEOMETRY_STAGE].mCommandList->Reset(gAllocatorsAndLists[0][GEOMETRY_STAGE].mAllocator, NULL);
	dirList->Reset(dirAllo, NULL);
	dirList->CopyBufferRegion(gVertexBufferResource, 0, gVertexStagingBufferResource, 0, sizeof(triangleVertices));
	dirList->CopyBufferRegion(gVertexBufferNormalResource, 0, gNormalStagingBufferResource, 0, sizeof(triangleVertices));

	//  todo change to transition helper function
	D3D12_RESOURCE_BARRIER barrierDesc{};
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = gVertexBufferResource;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	dirList->ResourceBarrier(1, &barrierDesc);

	barrierDesc.Transition.pResource = gVertexBufferNormalResource;
	dirList->ResourceBarrier(1, &barrierDesc);

	//Initialize vertex buffer view, used in the render call.
	gVertexBufferViews[0].BufferLocation = gVertexBufferResource->GetGPUVirtualAddress();
	gVertexBufferViews[0].StrideInBytes  = sizeof(float4);
	gVertexBufferViews[0].SizeInBytes    = sizeof(triangleVertices);
	gVertexBufferViews[1].BufferLocation = gVertexBufferNormalResource->GetGPUVirtualAddress();
	gVertexBufferViews[1].StrideInBytes = sizeof(float4);
	gVertexBufferViews[1].SizeInBytes = sizeof(normalVertices);


	dirList->Close();
	std::vector<ID3D12CommandList*> ppCommandLists{ dirList };
	gCommandQueues[QT_DIR]->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());
	

	// Wait for GPU to upload the vertex data before continuing
	gCommandQueues[QT_DIR]->Signal(gBackBufferFence, 1);
	if (gBackBufferFence->GetCompletedValue() < 1) {
		gBackBufferFence->SetEventOnCompletion(1, gBackBufferFenceEvent[0]);
		WaitForSingleObject(gBackBufferFenceEvent[0], INFINITE);
	}
}

// todo remove unused root parameters and tables
void Project::CreateRootSignature()
{
	//define descriptor range(s)
	D3D12_DESCRIPTOR_RANGE  dtRanges[1];
	dtRanges[0].RangeType			= D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	dtRanges[0].NumDescriptors		= 1;
	dtRanges[0].BaseShaderRegister	= 0; //register t0
	dtRanges[0].RegisterSpace		= 0; //register(t0,space0);
	dtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Compute shader UAV
	D3D12_DESCRIPTOR_RANGE  cdtRanges[1];

	cdtRanges[0].RangeType			= D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	cdtRanges[0].NumDescriptors		= 1; 
	cdtRanges[0].BaseShaderRegister = 0; //register u0
	cdtRanges[0].RegisterSpace		= 0; //register(u0,space0);
	cdtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE dt;
	dt.NumDescriptorRanges = ARRAYSIZE(dtRanges);
	dt.pDescriptorRanges = dtRanges;

	//create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE cdt;
	cdt.NumDescriptorRanges = ARRAYSIZE(cdtRanges);
	cdt.pDescriptorRanges = cdtRanges;



	//create root parameter
	D3D12_ROOT_PARAMETER rootParam[3];

	// constant buffer
	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParam[0].Descriptor = { 0, 0 }; // (b0, space0)
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// input texture of compute shader
	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].DescriptorTable = dt;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; //used by both compute and pixel shader in the test version

	// UAV output of compute shader
	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].DescriptorTable = cdt;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // Visible to compute



	// create a static sampler
	D3D12_STATIC_SAMPLER_DESC bilinearSampler = {};
	bilinearSampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	bilinearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	bilinearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	bilinearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	bilinearSampler.MipLODBias = 0;
	bilinearSampler.MaxAnisotropy = 0;
	bilinearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	bilinearSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	bilinearSampler.MinLOD = 0.0f;
	bilinearSampler.MaxLOD = D3D12_FLOAT32_MAX;
	bilinearSampler.ShaderRegister = 0; // (s0)
	bilinearSampler.RegisterSpace = 0;
	bilinearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	D3D12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rsDesc.NumParameters = ARRAYSIZE(rootParam);
	rsDesc.pParameters = rootParam;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.pStaticSamplers = &bilinearSampler;

	ID3DBlob* sBlob;
	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rsDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&sBlob,
		&errorBlob);

	if (FAILED(hr)) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		errorBlob->Release();
	}


	if (FAILED(gDevice5->CreateRootSignature(
		0,
		sBlob->GetBufferPointer(),
		sBlob->GetBufferSize(),
		IID_PPV_ARGS(&gRootSignature)))) {

	}
}


// todo: rename some stuff
void Project::CreateComputeShaderResources()
{
	//// pixel shader output render targets/compute shader inputs ////
	D3D12_RESOURCE_DESC intRTresDesc = {};
	intRTresDesc.DepthOrArraySize = 1;
	intRTresDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	intRTresDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	intRTresDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	intRTresDesc.Width = SCREEN_WIDTH;
	intRTresDesc.Height = SCREEN_HEIGHT;
	intRTresDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	intRTresDesc.MipLevels = 1;
	intRTresDesc.SampleDesc.Count = 1;

	D3D12_HEAP_PROPERTIES intRThp = {};
	intRThp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clv = {};
	clv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clv.Color[0] = gClearColor[0];
	clv.Color[1] = gClearColor[1];
	clv.Color[2] = gClearColor[2];
	clv.Color[3] = gClearColor[3];

	// Create a commited resource on the GPU for all intermediate render targets used by the geometry pass
	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		HRESULT hr = gDevice5->CreateCommittedResource(
			&intRThp, D3D12_HEAP_FLAG_NONE, &intRTresDesc,
			//D3D12_RESOURCE_STATE_NON_PIXEL_SHADER,
			D3D12_RESOURCE_STATE_COMMON,
			&clv, IID_PPV_ARGS(&gPerFrameAllocatorsListsAndResources[i].gIntermediateRenderTarget));
	}


	//// Compute shader outputs ////

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDesc.Width = SCREEN_WIDTH;
	resDesc.Height = SCREEN_HEIGHT;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;

	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	// Initialized as copy source since that's the state it'll be in at the end of every frame
	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		HRESULT hr = gDevice5->CreateCommittedResource(
			&hp, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			nullptr, IID_PPV_ARGS(&gPerFrameAllocatorsListsAndResources[i].gUAVResource));
	}


	//// Descriptor heap for the CBV, SRV, and UAV ////
	//
	// Layout of the descriptor heap
	//    slot     descriptor
	//     0-2     UAV, output of compute shader
	//     3-5     SRV, of the output of the pixel shader

	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = NUM_SWAP_BUFFERS + NUM_SWAP_BUFFERS;
	dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	HRESULT hr = gDevice5->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&gComputeDescriptorHeap));

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE cdh = gComputeDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		gDevice5->CreateUnorderedAccessView(gPerFrameAllocatorsListsAndResources[i].gUAVResource, nullptr, &uavDesc, cdh);
		cdh.ptr += gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Create SRVs for all the buffers in the swap chain so that they can be read from by the compute shader
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	for (int i = 0; i < NUM_SWAP_BUFFERS; i++) {
		gDevice5->CreateShaderResourceView(gPerFrameAllocatorsListsAndResources[i].gIntermediateRenderTarget, &srvDesc, cdh);
		cdh.ptr += gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}


	//// Descriptor heap for the intermediate render targets ////
	D3D12_DESCRIPTOR_HEAP_DESC RTVdhd = {};
	RTVdhd.NumDescriptors = NUM_SWAP_BUFFERS;
	RTVdhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;


	hr = gDevice5->CreateDescriptorHeap(&RTVdhd, IID_PPV_ARGS(&gIntermediateRenderTargetsDescHeap));

	//Create resources for the render targets.
	gRenderTargetDescriptorSize = gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvcdh = gIntermediateRenderTargetsDescHeap->GetCPUDescriptorHandleForHeapStart();

	//One RTV for each frame.
	for (UINT n = 0; n < NUM_SWAP_BUFFERS; n++)
	{
		gDevice5->CreateRenderTargetView(gPerFrameAllocatorsListsAndResources[n].gIntermediateRenderTarget, nullptr, rtvcdh);
		rtvcdh.ptr += gRenderTargetDescriptorSize;
	}


}

// todo remove unused stuff
void Project::CreateConstantBufferResources()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
	heapDescriptorDesc.NumDescriptors = TOTAL_DRAGONS;
	heapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	gDevice5->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&gConstantBufferDescriptorHeap));

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.CreationNodeMask = 1; //used when multi-gpu
	heapProperties.VisibleNodeMask = 1; //used when multi-gpu
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(mGameStateHandler.cbData);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	if (SUCCEEDED(gDevice5->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&gConstantBufferResource))))
	{
		if (SUCCEEDED(gConstantBufferResource->Map(0, 0, (void**)&mGameStateHandler.pMappedCB)))
		{
			//memcpy(pMappedCB[index], cbData, sizeof(cbData));
		}

	}

}

void Project::CopyComputeOutputToBackBuffer(UINT64 frameIndex, int swapBufferIndex, int threadIndex)
{
	UINT backBufferIndex = gSwapChain4->GetCurrentBackBufferIndex();

	PerFrame* frame = &gPerFrameAllocatorsListsAndResources[swapBufferIndex];
	ID3D12CommandAllocator*     dirAllo = frame->mAllocatorsAndLists[PRESENT_STAGE].mAllocators[0];
	D3D12GraphicsCommandListPtr dirList = frame->mAllocatorsAndLists[PRESENT_STAGE].mCommandLists[0];


	dirAllo->Reset();
	dirList->Reset(dirAllo, nullptr);
#ifdef RECORD_TIME
	if (frameIndex >= FIRST_TIMESTAMPED_FRAME && frameIndex < (NUM_TIMESTAMP_PAIRS + FIRST_TIMESTAMPED_FRAME)) {
		int arrIndex = frameIndex - FIRST_TIMESTAMPED_FRAME;
		QueryPerformanceCounter(&mCPUTimeStamps[arrIndex][2].Start);

		// gpu timer start
		gpuTimer[2].start(dirList, arrIndex);
	}
#endif

	//Indicate that the back buffer will be used as render target.
	SetResourceTransitionBarrier(dirList, gSwapChainRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_COPY_DEST
	);


	dirList->CopyResource(gSwapChainRenderTargets[backBufferIndex], frame->gUAVResource);

	//Indicate that the back buffer will be used as render target.
	SetResourceTransitionBarrier(dirList, gSwapChainRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PRESENT
	);

#ifdef RECORD_TIME
	if (frameIndex >= FIRST_TIMESTAMPED_FRAME && frameIndex < (NUM_TIMESTAMP_PAIRS + FIRST_TIMESTAMPED_FRAME)) {
		int arrIndex = frameIndex - FIRST_TIMESTAMPED_FRAME;
		gpuTimer[2].stop(dirList, arrIndex);
		gpuTimer[2].resolveQueryToCPU(dirList, arrIndex);
		//Close the list to prepare it for execution.

		QueryPerformanceCounter(&mCPUTimeStamps[arrIndex][2].Stop);
	}
#endif
	//Close the list to prepare it for execution.
	dirList->Close();

	// Wait for the FXAA stage to be finished
	gCommandQueues[QT_DIR]->Wait(gSwapBufferFences[swapBufferIndex], gSwapBufferFenceValues[swapBufferIndex]);

	//Execute the command list.
	ID3D12CommandList* listsToExecute2[] = { dirList };
	gCommandQueues[QT_DIR]->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);
}

void Project::Render(int id)
{
	static int lastRenderIterationSwapBufferIndex = 0;
	static int lastRenderIterationThreadIndex = 0;
	static UINT64 frameCounter = 1; //! should start at 1 because of the signal at the end of UploadMeshData()
	thread_local int swapBufferIndex;
	thread_local int threadIndex;
	thread_local UINT64 frameIndex;

	// get the value used to ensure that frames are presented in the correct order
	frameIndex = frameCounter++;

	// Wait for the swap chain so that no more than MAX_FRAME_LATENCY frames are being processed simultaneously
	// See MAX_FRAME_LATENCY in ConstantsAndGlobals.hpp for the specified amount
	WaitForSingleObjectEx(gSwapChainWaitableObject, 1000, true);

	CountFPS(mWndHandle);
	
	//get the swap buffer index
	swapBufferIndex = lastRenderIterationSwapBufferIndex;
	lastRenderIterationSwapBufferIndex = (++lastRenderIterationSwapBufferIndex) % NUM_SWAP_BUFFERS;

	//get the thread index
	threadIndex = lastRenderIterationThreadIndex;
	lastRenderIterationThreadIndex = (++lastRenderIterationThreadIndex) % NUM_THREADS;

	if (isRunning) {
		////////// Render geometry section //////////

		// Render the geometry
		GPUStages[0]->Run(frameIndex, swapBufferIndex, threadIndex, this);

		// Signal that the geometry stage is finished
		gCommandQueues[QT_DIR]->Signal(gSwapBufferFences[swapBufferIndex], ++gSwapBufferFenceValues[swapBufferIndex]);


		// Begin the rendering of the next frame once the first part of this frame is done.
		gThreadPool->push([this](int id) {Render(id); });

		////////// FXAA section //////////

		// Apply FXAA to the rendered image with a compute shader
		GPUStages[1]->Run(frameIndex, swapBufferIndex, threadIndex, this);

		// Signal that the FXAA stage is finished
		gCommandQueues[QT_COMP]->Signal(gSwapBufferFences[swapBufferIndex], ++gSwapBufferFenceValues[swapBufferIndex]);



		////////// Present section //////////

		// Wait for the previous frame to have been sent to present
		if (gBackBufferFence->GetCompletedValue() < frameIndex) {
			gBackBufferFence->SetEventOnCompletion(frameIndex, gBackBufferFenceEvent[swapBufferIndex]);
			WaitForSingleObject(gBackBufferFenceEvent[swapBufferIndex], INFINITE);
		}
		//gCommandQueues[QT_DIR].mQueue->Wait(gBackBufferFence, frameIndex);



		// Lock this section since if Present is called in another thread whilst in this section the backbufferIndex 
		// used in CopyComputeOutputToBackBuffer() becomes invalid and the application crashes
		gPresentLock.lock();

		// Copy the result of the FXAA compute shader to the back buffer
		CopyComputeOutputToBackBuffer(frameIndex, swapBufferIndex, threadIndex);


		// Present the frame.
		DXGI_PRESENT_PARAMETERS pp = {};
		gSwapChain4->Present1(0, DXGI_PRESENT_ALLOW_TEARING, &pp);

		// Signal that the frame with frameIndex+1 can enter the present section after this one is done
		gCommandQueues[QT_DIR]->Signal(gBackBufferFence, frameIndex + 1);

		gPresentLock.unlock();
	}
#ifdef RECORD_TIME
	if (frameIndex == FIRST_TIMESTAMPED_FRAME + 2*NUM_TIMESTAMP_PAIRS) {
		UINT64 gpuEpoch = mClockCalibration.gpuTimeStamp;
		UINT64 gpuFreq;
		gCommandQueues[QT_DIR]->GetTimestampFrequency(&gpuFreq);
		double gpuTimestampToMs = (1.0 / gpuFreq) * 1'000.0;

		UINT64 cpuEpoch = mClockCalibration.cpuTimeStamp;
		LARGE_INTEGER cpuLIFreq;
		QueryPerformanceFrequency(&cpuLIFreq);
		UINT64 cpuFreq = cpuLIFreq.QuadPart;
		double cpuTimestampToMs = (1.0 / cpuFreq) * 1'000.0;

		FILE *f;

		std::stringstream ss;
		ss << "data" << MAX_FRAME_LATENCY << ".tex";
		

		if (fopen_s(&f, ss.str().c_str(), "w") != 0) {
			OutputDebugStringA("Error: could not open file to write data to.\n");
			return;
		}

		char buffer[1000];
		sprintf_s(buffer, "%% Timestamps with max frame latency: %d\n\n\n", MAX_FRAME_LATENCY);
		fprintf(f, "%s", buffer);

		std::string numberStr[3] = { "Zero", "One", "Two" };

		// save timestamps to file
		for (int i = 0; i < NUM_TIMESTAMP_PAIRS; ++i) {
			CPUTimeStampPair cpuTimePair = mCPUTimeStamps[i][0];
			int frame = i + FIRST_TIMESTAMPED_FRAME;

			double cpuTimes[3][2];
			double gpuTimes[3][2];

			for (int part = 0; part < 3; part++) {
				cpuTimes[part][0] = (mCPUTimeStamps[i][part].Start.QuadPart - cpuEpoch) * cpuTimestampToMs;
				cpuTimes[part][1] = (mCPUTimeStamps[i][part].Stop.QuadPart - cpuEpoch) * cpuTimestampToMs;

				D3D12::GPUTimestampPair gpuTSP = gpuTimer[part].getTimestampPair(i);
				gpuTimes[part][0] = ((gpuTSP.Start - gpuEpoch) * gpuTimestampToMs);
				gpuTimes[part][1] = ((gpuTSP.Stop - gpuEpoch) * gpuTimestampToMs);
			}


			sprintf_s(buffer, "%% CPU frame: %d\n", frame);
			fprintf(f, "%s", buffer);
			sprintf_s(buffer, "\\addplot[geomStyle%s] coordinates{ (%.6f,%.1f) (%.6f,%.1f) }; \n", numberStr[(i % 3)].c_str(), cpuTimes[0][0], (0.9 + (i % 3) / 10.0), cpuTimes[0][1], (0.9 + (i % 3) / 10.0));
			fprintf(f, "%s", buffer);
			sprintf_s(buffer, "\\addplot[fxaaStyle%s] coordinates{ (%.6f,%.1f) (%.6f,%.1f) }; \n", numberStr[(i % 3)].c_str(), cpuTimes[1][0], (0.9 + (i % 3) / 10.0), cpuTimes[1][1], (0.9 + (i % 3) / 10.0));
			fprintf(f, "%s", buffer);
			sprintf_s(buffer, "\\addplot[presStyle%s] coordinates{ (%.6f,%.1f) (%.6f,%.1f) }; \n", numberStr[(i % 3)].c_str(), cpuTimes[2][0], (0.9 + (i % 3) / 10.0), cpuTimes[2][1], (0.9 + (i % 3) / 10.0));
			fprintf(f, "%s", buffer);

			sprintf_s(buffer, "%% GPU frame: %d\n", frame);
			fprintf(f, "%s", buffer);
			sprintf_s(buffer, "\\addplot[geomStyle%s] coordinates{ (%.6f,2) (%.6f,2) }; \n", numberStr[(i % 3)].c_str(), gpuTimes[0][0], gpuTimes[0][1]);
			fprintf(f, "%s", buffer);
			sprintf_s(buffer, "\\addplot[fxaaStyle%s] coordinates{ (%.6f,1.75) (%.6f,1.75) }; \n", numberStr[(i % 3)].c_str(), gpuTimes[1][0], gpuTimes[1][1]);
			fprintf(f, "%s", buffer);
			sprintf_s(buffer, "\\addplot[presStyle%s] coordinates{ (%.6f,2) (%.6f,2) }; \n\n", numberStr[(i % 3)].c_str(), gpuTimes[2][0], gpuTimes[2][1]);
			fprintf(f, "%s", buffer);
		}
		fclose(f);
	}
#endif
}
