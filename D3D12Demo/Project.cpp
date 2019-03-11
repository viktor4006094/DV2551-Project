#include "Project.hpp"
#include "RenderStage.hpp"
#include "ComputeStage.hpp"

#include <chrono>
#include <ctime>
#include <string>



#include <functional>

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
	CreateCommandInterfacesAndSwapChain(wndHandle);	//3. Create CommandQueue and SwapChain
	CreateFenceAndEventHandle();						//4. Create Fence and Event handle
	CreateRenderTargets();								//5. Create render targets for backbuffer
	CreateViewportAndScissorRect();						//6. Create viewport and rect
	CreateRootSignature();								//7. Create root signature
	CreateShadersAndPipelineStates();					//8. Set up the pipeline state

	CreateConstantBufferResources();					//9. Create constant buffer data
	CreateTriangleData();								//10. Create vertexdata
	mGameStateHandler.CreateMeshes();
	//CreateMeshes();										//11. Create meshes (all use same triangle but different constant buffers)
	CreateComputeShaderResources();

	for (int i = 0; i < NUM_THREADS; ++i) {
		gThreadFenceValues[i] = 0;
		gThreadFenceEvents[i] = CreateEvent(0, false, false, 0);
		gDevice5->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gThreadFences[i]));
	}

	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		gSwapBufferFenceValues[i] = 0;// NUM_SWAP_BUFFERS; //? 0
		//gSwapBufferWaitValues[i] = 0;
		gSwapBufferFenceEvents[i] = CreateEvent(0, false, false, 0);
		gDevice5->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gSwapBufferFences[i]));
	
		
		gBackBufferFenceEvent[i] = CreateEvent(0, false, false, 0);
	}

	gDevice5->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gBackBufferFence));

	// todo move elsewhere?
#ifdef _DEBUG
	std::wstring intStr = L"InterMediateRenderTarget";
	std::wstring uavStr = L"UAVResource";
	std::wstring swapStr = L"SwapChainRenderTarget";
	HRESULT hr = S_OK;
	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		std::wstring wsInt = intStr + std::to_wstring(i);
		LPCWSTR cInt = wsInt.c_str();
		hr = gPerFrameResources[i].gIntermediateRenderTarget->SetName(cInt);

		std::wstring wsUAV = uavStr + std::to_wstring(i);
		LPCWSTR cUAV = wsUAV.c_str();
		hr = gPerFrameResources[i].gUAVResource->SetName(cUAV);

		std::wstring wsSwap = swapStr + std::to_wstring(i);
		LPCWSTR cSwap = wsSwap.c_str();
		hr = gSwapChainRenderTargets[i]->SetName(cSwap);
	}


	gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->SetName(L"DirectQueue");
	gCommandQueues[QUEUE_TYPE_COPY].mQueue->SetName(L"CopyQueue");
	gCommandQueues[QUEUE_TYPE_COMPUTE].mQueue->SetName(L"ComputeQueue");
#endif


	WaitForGpu(QUEUE_TYPE_DIRECT);
}

void Project::Start()
{
	gThreadPool->push([this](int id) {mGameStateHandler.Update(id, &mLatestBackBufferIndex); });
	gThreadPool->push([this](int id) {this->Render(id); });
}

void Project::Stop()
{
	isRunning = false;
}


void Project::Shutdown()
{
	mGameStateHandler.ShutDown();

	WaitForGpu(QUEUE_TYPE_DIRECT); //todo DON'T

	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		gConstantBufferResource[i]->Unmap(0, nullptr);
	}

	//CloseHandle(gEventHandle);
	SafeRelease(&gDevice5);

	gCommandQueues[0].Release();
	gCommandQueues[1].Release();
	gCommandQueues[2].Release();

	for (int i = 0; i < NUM_THREADS; ++i) {
		gAllocatorsAndLists[i][0].Release();
		gAllocatorsAndLists[i][1].Release();
		gAllocatorsAndLists[i][2].Release();
	}

	//gGraphicsQueue.Release();
	//gCopyQueue.Release();
	//gComputeQueue.Release();

	//SafeRelease(&gGraphicsQueue);
	//SafeRelease(&gCopyQueue);
	//SafeRelease(&gComputeQueue);
	//SafeRelease(&gCommandAllocator);
	//SafeRelease(&gGraphicsCommandList);
	//SafeRelease(&gCopyCommandList);
	//SafeRelease(&gComputeCommandList);
	SafeRelease(&gSwapChain4);

	//SafeRelease(&gFence);

	SafeRelease(&gSwapChainRenderTargetsDescHeap);
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		SafeRelease(&gDescriptorHeap[i]);
		SafeRelease(&gConstantBufferResource[i]);
		
		
		gPerFrameResources[i].Release();
		//SafeRelease(&gSwapChainRenderTargets[i]);
	}

	SafeRelease(&gRootSignature);
	//SafeRelease(&gRenderPipeLineState);

	SafeRelease(&gVertexBufferResource);
}

//Helper function for syncronization of GPU/CPU
void Project::WaitForGpu(QueueType type)
{
	gCommandQueues[type].WaitForGpu();
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
	gDevice5->CreateCommandQueue(&cqd, IID_PPV_ARGS(&gCommandQueues[QUEUE_TYPE_DIRECT].mQueue));

	cqd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	gDevice5->CreateCommandQueue(&cqd, IID_PPV_ARGS(&gCommandQueues[QUEUE_TYPE_COPY].mQueue));

	cqd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	gDevice5->CreateCommandQueue(&cqd, IID_PPV_ARGS(&gCommandQueues[QUEUE_TYPE_COMPUTE].mQueue));


	//Create command allocator. The command allocator object corresponds
	//to the underlying allocations in which GPU commands are stored.
	//Create command list.
	for (int i = 0; i < NUM_THREADS; ++i) {
		gAllocatorsAndLists[i][0].CreateCommandListAndAllocator(QUEUE_TYPE_DIRECT, gDevice5);
		gAllocatorsAndLists[i][1].CreateCommandListAndAllocator(QUEUE_TYPE_COPY, gDevice5);
		gAllocatorsAndLists[i][2].CreateCommandListAndAllocator(QUEUE_TYPE_COMPUTE, gDevice5);
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
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT; // Enable GetFrameLatencyWaitableObject();
	scDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	IDXGISwapChain1* swapChain1 = nullptr;
	
	HRESULT hr = factory->CreateSwapChainForHwnd(
		gCommandQueues[QUEUE_TYPE_DIRECT].mQueue,
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

	SafeRelease(&factory);
}


void Project::CreateFenceAndEventHandle()
{
	gCommandQueues[QUEUE_TYPE_DIRECT].CreateFenceAndEventHandle(gDevice5);
	gCommandQueues[QUEUE_TYPE_COPY].CreateFenceAndEventHandle(gDevice5);
	gCommandQueues[QUEUE_TYPE_COMPUTE].CreateFenceAndEventHandle(gDevice5);

	for (int i = 0; i < NUM_SWAP_BUFFERS; ++i) {
		gPerFrameResources[i].CreateFenceAndEventHandle(gDevice5);
	}

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


void Project::CreateTriangleData()
{
	Vertex triangleVertices[3] =
	{
		{ 0.0f,  0.05f, 0.0f, 1.0f },
		{ 0.05f, -0.05f, 0.0f, 1.0f },
		{ -0.05f, -0.05f, 0.0f, 1.0f }
	};

	//Note: using upload heaps to transfer static data like vert buffers is not 
	//recommended. Every time the GPU needs it, the upload heap will be marshalled 
	//over. Please read up on Default Heap usage. An upload heap is used here for 
	//code simplicity and because there are very few vertices to actually transfer.
	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type				= D3D12_HEAP_TYPE_UPLOAD;
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
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&gVertexBufferResource));

	gVertexBufferResource->SetName(L"vb heap");

	//Copy the triangle data to the vertex buffer.
	void* dataBegin = nullptr;
	D3D12_RANGE range = { 0, 0 }; //We do not intend to read this resource on the CPU.
	gVertexBufferResource->Map(0, &range, &dataBegin);
	memcpy(dataBegin, triangleVertices, sizeof(triangleVertices));
	gVertexBufferResource->Unmap(0, nullptr);

	//Initialize vertex buffer view, used in the render call.
	gVertexBufferView.BufferLocation = gVertexBufferResource->GetGPUVirtualAddress();
	gVertexBufferView.StrideInBytes  = sizeof(Vertex);
	gVertexBufferView.SizeInBytes    = sizeof(triangleVertices);
}

// todo remove unused root parameters and tables
void Project::CreateRootSignature()
{
	//define descriptor range(s)
	D3D12_DESCRIPTOR_RANGE  dtRanges[1];
	dtRanges[0].RangeType			= D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	dtRanges[0].NumDescriptors		= 1;
	dtRanges[0].BaseShaderRegister	= 0; //register b0
	dtRanges[0].RegisterSpace		= 0; //register(b0,space0);
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
	rootParam[0].Descriptor = { 0, 0 }; // b0, s0
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
	bilinearSampler.ShaderRegister = 0;
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
			D3D12_RESOURCE_STATE_COMMON,
			&clv, IID_PPV_ARGS(&gPerFrameResources[i].gIntermediateRenderTarget));
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
			nullptr, IID_PPV_ARGS(&gPerFrameResources[i].gUAVResource));
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
		gDevice5->CreateUnorderedAccessView(gPerFrameResources[i].gUAVResource, nullptr, &uavDesc, cdh);
		cdh.ptr += gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Create SRVs for all the buffers in the swap chain so that they can be read from by the compute shader
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	for (int i = 0; i < NUM_SWAP_BUFFERS; i++) {
		//gDevice5->CreateShaderResourceView(gSwapChainRenderTargets[i], &srvDesc, cdh); //old
		gDevice5->CreateShaderResourceView(gPerFrameResources[i].gIntermediateRenderTarget, &srvDesc, cdh); //new
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
		//hr = gSwapChain4->GetBuffer(n, IID_PPV_ARGS(&gIntermediateRenderTargets[n]));
		gDevice5->CreateRenderTargetView(gPerFrameResources[n].gIntermediateRenderTarget, nullptr, rtvcdh);
		rtvcdh.ptr += gRenderTargetDescriptorSize;
	}


}

// todo remove unused stuff
void Project::CreateConstantBufferResources()
{
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
		heapDescriptorDesc.NumDescriptors = TOTAL_TRIS;
		heapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		gDevice5->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&gDescriptorHeap[i]));
	}

	//UINT cbSizeAligned = (sizeof(ConstantBuffer) + 255) & ~255;	// 256-byte aligned CB.

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

	//D3D12_HEAP_DESC hDesc = {};
	//hDesc.SizeInBytes =sizeof(cbData);
	//hDesc.Properties = heapProperties;
	//hDesc.Alignment = 0;
	//hDesc.Flags = D3D12_HEAP_FLAG_NONE;

	//Create a resource heap, descriptor heap, and pointer to cbv for each frame
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		if (SUCCEEDED(gDevice5->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&gConstantBufferResource[i]))))
		{
			if (SUCCEEDED(gConstantBufferResource[i]->Map(0, 0, (void**)&mGameStateHandler.pMappedCB[i])))
			{
				//memcpy(pMappedCB[index], cbData, sizeof(cbData));
			}

		}
		//gDevice5->CreateHeap(&hDesc, IID_PPV_ARGS(&gConstantBufferHeap[i]));
		

		//D3D12_CPU_DESCRIPTOR_HANDLE cdh = gDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart();
		//for (int j = 0; j < TOTAL_TRIS; j++) {
			//gDevice5->CreatePlacedResource(
			//	gConstantBufferHeap[i],
			//	j*cbSizeAligned,
			//	&resourceDesc,
			//	D3D12_RESOURCE_STATE_GENERIC_READ,
			//	NULL,
			//	IID_PPV_ARGS(&gConstantBufferResource[i][j])
			//);

		//	gConstantBufferResource[i][j]->SetName(L"cb heap");
		//	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		//	cbvDesc.BufferLocation = gConstantBufferResource[i][j]->GetGPUVirtualAddress();
		////todo hlsl aligned data
		//	cbvDesc.SizeInBytes = cbSizeAligned;
		//	gDevice5->CreateConstantBufferView(&cbvDesc, cdh);
		//	cdh.ptr+=gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//}
	}
}

void Project::CopyComputeOutputToBackBuffer(int swapBufferIndex, int threadIndex)
{
	//// Present part ////

	UINT backBufferIndex = gSwapChain4->GetCurrentBackBufferIndex();
	
	PerFrameResources* perFrame = &gPerFrameResources[swapBufferIndex];
	
	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	ID3D12CommandAllocator* directAllocator = gAllocatorsAndLists[threadIndex][QUEUE_TYPE_DIRECT].mAllocator;
	D3D12GraphicsCommandListPtr directList = gAllocatorsAndLists[threadIndex][QUEUE_TYPE_DIRECT].mCommandList;


	directAllocator->Reset();
	directList->Reset(directAllocator, nullptr);

	SetResourceTransitionBarrier(directList, perFrame->gUAVResource,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	);

	//Indicate that the back buffer will be used as render target.
	SetResourceTransitionBarrier(directList, gSwapChainRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_COPY_DEST
	);


	directList->CopyResource(gSwapChainRenderTargets[backBufferIndex], perFrame->gUAVResource);

	//Indicate that the back buffer will be used as render target.
	SetResourceTransitionBarrier(directList, gSwapChainRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PRESENT
	);


	//Close the list to prepare it for execution.
	directList->Close();

	//Execute the command list.
	ID3D12CommandList* listsToExecute2[] = { directList };
	gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);
}

void Project::Render(int id)
{
	static int lastRenderIterationSwapBufferIndex = 0;
	static int lastRenderIterationThreadIndex = 0;
	thread_local int swapBufferIndex;
	thread_local int threadIndex;
	thread_local UINT64 backBufferFenceWaitValue;
	thread_local UINT64 backBufferFenceSignalValue;
	//static int testCounter = 0;

	//get the thread index
	gThreadIDIndexLock.lock();
	CountFPS(mWndHandle);
	
	swapBufferIndex = lastRenderIterationSwapBufferIndex;
	lastRenderIterationSwapBufferIndex = (++lastRenderIterationSwapBufferIndex) % NUM_SWAP_BUFFERS;

	threadIndex = lastRenderIterationThreadIndex;
	lastRenderIterationThreadIndex = (++lastRenderIterationThreadIndex) % NUM_THREADS;

	gThreadIDIndexLock.unlock();

	if (isRunning) {

		// Update the index used by the CPU update loop
		mLatestBackBufferIndex = swapBufferIndex;

		PerFrameResources* perFrame = &gPerFrameResources[swapBufferIndex];
		PerThreadFenceHandle* perThread = &gPerThreadFenceHandles[threadIndex];

		// Wait for the swap chain so that no more than MAX_FRAME_LATENCY frames are being processed simultaneously
		// See MAX_FRAME_LATENCY in ConstantsAndGlobals.hpp for the specified amount
		WaitForSingleObjectEx(gSwapChainWaitableObject, 1000, true);

		// Render the geometry
		GPUStages[0]->Run(swapBufferIndex, threadIndex, this);

		//todo move wait calls to after the next list has been recorded but before it starts being executed
		// Wait for the render stage to finish
		UINT64 threadFenceValue = InterlockedIncrement(&gThreadFenceValues[threadIndex]);
		gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->Signal(gThreadFences[threadIndex], threadFenceValue);
		gThreadFences[threadIndex]->SetEventOnCompletion(threadFenceValue, gThreadFenceEvents[threadIndex]);
		WaitForSingleObject(gThreadFenceEvents[threadIndex], INFINITE);
		
		// Begin the rendering of the next frame once the first part of this frame is done.
		gThreadPool->push([this](int id) {Render(id); });


		// Apply FXAA to the rendered image with a compute shader
		GPUStages[1]->Run(swapBufferIndex, threadIndex, this);

		// Wait for the compute stage to finish executing
		threadFenceValue = InterlockedIncrement(&gThreadFenceValues[threadIndex]);
		gCommandQueues[QUEUE_TYPE_COMPUTE].mQueue->Signal(gThreadFences[threadIndex], threadFenceValue);
		gThreadFences[threadIndex]->SetEventOnCompletion(threadFenceValue, gThreadFenceEvents[threadIndex]);
		WaitForSingleObject(gThreadFenceEvents[threadIndex], INFINITE);

	
		// Copy the result of the FXAA compute shader to the back buffer
		CopyComputeOutputToBackBuffer(swapBufferIndex, threadIndex);


		// Present the frame.
		DXGI_PRESENT_PARAMETERS pp = {};
		gSwapChain4->Present1(0, 0, &pp);
	}
}
