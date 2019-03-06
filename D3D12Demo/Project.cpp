#include "Project.hpp"
#include "RenderStage.hpp"
#include "ComputeStage.hpp"
#include "PassThroughStage.hpp"

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

	gThreadPool = new ctpl::thread_pool(10);

	GPUStages[0] = new RenderStage();
	GPUStages[1] = new ComputeStage();
	GPUStages[2] = new PassThroughStage();

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

	WaitForGpu(QUEUE_TYPE_DIRECT);
}

void Project::Start()
{
	gThreadPool->push([this](int id) {mGameStateHandler.Update(id); });
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

	//CloseHandle(gEventHandle);
	SafeRelease(&gDevice5);

	gCommandQueues[0].Release();
	gCommandQueues[1].Release();
	gCommandQueues[2].Release();

	for (int i = 0; i < MAX_PREPARED_FRAMES; ++i) {
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

	SafeRelease(&gRenderTargetsHeap);
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		SafeRelease(&gDescriptorHeap[i]);
		SafeRelease(&gConstantBufferResource[i]);
		SafeRelease(&gRenderTargets[i]);
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
		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device5), nullptr)))
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
	for (int i = 0; i < MAX_PREPARED_FRAMES; ++i) {
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
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	scDesc.BufferCount = NUM_SWAP_BUFFERS;
	scDesc.Scaling = DXGI_SCALING_NONE;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.Flags = 0;
	scDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	IDXGISwapChain1* swapChain1 = nullptr;
	if (SUCCEEDED(factory->CreateSwapChainForHwnd(
		gCommandQueues[QUEUE_TYPE_DIRECT].mQueue,
		wndHandle,
		&scDesc,
		nullptr,
		nullptr,
		&swapChain1)))
	{
		if (SUCCEEDED(swapChain1->QueryInterface(IID_PPV_ARGS(&gSwapChain4))))
		{
			gSwapChain4->Release();
		}
	}

	SafeRelease(&factory);
}


void Project::CreateFenceAndEventHandle()
{
	gCommandQueues[QUEUE_TYPE_DIRECT].CreateFenceAndEventHandle(gDevice5);
	gCommandQueues[QUEUE_TYPE_COPY].CreateFenceAndEventHandle(gDevice5);
	gCommandQueues[QUEUE_TYPE_COMPUTE].CreateFenceAndEventHandle(gDevice5);
}

void Project::CreateRenderTargets()
{
	//Create descriptor heap for render target views.
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = NUM_SWAP_BUFFERS;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	HRESULT hr = gDevice5->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&gRenderTargetsHeap));

	//Create resources for the render targets.
	gRenderTargetDescriptorSize = gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = gRenderTargetsHeap->GetCPUDescriptorHandleForHeapStart();


	//One RTV for each frame.
	for (UINT n = 0; n < NUM_SWAP_BUFFERS; n++)
	{
		hr = gSwapChain4->GetBuffer(n, IID_PPV_ARGS(&gRenderTargets[n]));
		gDevice5->CreateRenderTargetView(gRenderTargets[n], nullptr, cdh);
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
	GPUStages[2]->Init(gDevice5, gRootSignature);
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


void Project::CreateRootSignature()
{
	//define descriptor range(s)
	D3D12_DESCRIPTOR_RANGE  dtRanges[1];
	dtRanges[0].RangeType			= D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	dtRanges[0].NumDescriptors		= 1; //only one CB in this example
	dtRanges[0].BaseShaderRegister	= 0; //register b0
	dtRanges[0].RegisterSpace		= 0; //register(b0,space0);
	dtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Compute shader UAV
	D3D12_DESCRIPTOR_RANGE  cdtRanges[1];

	cdtRanges[0].RangeType			= D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	cdtRanges[0].NumDescriptors		= 1; //only one CB in this example
	cdtRanges[0].BaseShaderRegister = 0; //register u0
	cdtRanges[0].RegisterSpace		= 0; //register(u0,space0);
	cdtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//constant buffers
	D3D12_DESCRIPTOR_RANGE cbdtRanges[1];
	cbdtRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbdtRanges[0].NumDescriptors = 1; //only one CB in this example
	cbdtRanges[0].BaseShaderRegister = 2; //register b0
	cbdtRanges[0].RegisterSpace = 0; //register(b0,space0);
	cbdtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	//create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE dt;
	dt.NumDescriptorRanges = ARRAYSIZE(dtRanges);
	dt.pDescriptorRanges = dtRanges;

	//create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE cdt;
	cdt.NumDescriptorRanges = ARRAYSIZE(cdtRanges);
	cdt.pDescriptorRanges = cdtRanges;

	//create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE cbdt;
	cbdt.NumDescriptorRanges = ARRAYSIZE(cbdtRanges);
	cbdt.pDescriptorRanges = cbdtRanges;

	//create root parameter
	D3D12_ROOT_PARAMETER rootParam[5];

	// constant translate 
	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].Constants = { 0, 0, 4 }; // 4 constants in b0 first register space
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// constant color
	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].Constants = { 1, 0, 4 }; // 4 constants in b0 first register space
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Texture
	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].DescriptorTable = dt;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; //used by both compute and pixel shader in the test version

	// UAV for compute shader
	rootParam[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[3].DescriptorTable = cdt;
	rootParam[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // Visible to compute

	//Constant buffers
	rootParam[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[4].DescriptorTable = cbdt;
	rootParam[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rsDesc.NumParameters = ARRAYSIZE(rootParam);
	rsDesc.pParameters = rootParam;
	rsDesc.NumStaticSamplers = 0;
	rsDesc.pStaticSamplers = nullptr;

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


	hr = gDevice5->CreateRootSignature(
		0,
		sBlob->GetBufferPointer(),
		sBlob->GetBufferSize(),
		IID_PPV_ARGS(&gRootSignature));
}

void Project::CreateComputeShaderResources()
{
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

	HRESULT hr = gDevice5->CreateCommittedResource(
		&hp, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&gUAVResource));

	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = NUM_SWAP_BUFFERS + 1;
	dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	hr = gDevice5->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&gComputeDescriptorHeap));

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE cdh = gComputeDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	gDevice5->CreateUnorderedAccessView(gUAVResource, nullptr, &uavDesc, cdh);

	cdh.ptr += gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	// Create SRVs for all the buffers in the swap chain so that they can be read from by the compute shader
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	for (int i = 0; i < NUM_SWAP_BUFFERS; i++) {
		gDevice5->CreateShaderResourceView(gRenderTargets[i], &srvDesc, cdh);
		cdh.ptr += gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

	UINT cbSizeAligned = (sizeof(ConstantBuffer) + 255) & ~255;	// 256-byte aligned CB.

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.CreationNodeMask = 1; //used when multi-gpu
	heapProperties.VisibleNodeMask = 1; //used when multi-gpu
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width =  cbSizeAligned;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_DESC hDesc = {};
	hDesc.SizeInBytes = cbSizeAligned * TOTAL_TRIS;
	hDesc.Properties = heapProperties;
	hDesc.Alignment = 0;
	hDesc.Flags = D3D12_HEAP_FLAG_NONE;
	//Create a resource heap, descriptor heap, and pointer to cbv for each frame
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		//gDevice5->CreateCommittedResource(
		//	&heapProperties,
		//	D3D12_HEAP_FLAG_NONE,
		//	&resourceDesc,
		//	D3D12_RESOURCE_STATE_GENERIC_READ,
		//	nullptr,
		//	IID_PPV_ARGS(&gConstantBufferResource[i])
		//);
		gDevice5->CreateHeap(&hDesc, IID_PPV_ARGS(&gConstantBufferHeap[i]));


		D3D12_CPU_DESCRIPTOR_HANDLE cdh = gDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart();
		for (int j = 0; j < TOTAL_TRIS; j++) {
			gDevice5->CreatePlacedResource(
				gConstantBufferHeap[i],
				j*cbSizeAligned,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				NULL,
				IID_PPV_ARGS(&gConstantBufferResource[i][j])
			);

			gConstantBufferResource[i][j]->SetName(L"cb heap");
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = gConstantBufferResource[i][j]->GetGPUVirtualAddress();
		//todo hlsl aligned data
			cbvDesc.SizeInBytes = cbSizeAligned;
			gDevice5->CreateConstantBufferView(&cbvDesc, cdh);
			cdh.ptr+=gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}
}


void Project::Render(int id)
{
	static size_t lastRenderIterationIndex = 0;
	thread_local int index;

	CountFPS(mWndHandle);

	//get the thread index
	gThreadIDIndexLock.lock();
	index = lastRenderIterationIndex;
	lastRenderIterationIndex = (++lastRenderIterationIndex) % MAX_PREPARED_FRAMES;
	gThreadIDIndexLock.unlock();

	if (isRunning) {
		// Render geometry
		GPUStages[0]->Run(index, this);

		//todo start new render thread here

		// Execute Compute shader
		GPUStages[1]->Run(index, this);

		// Passthrough to show on screen
		GPUStages[2]->Run(index, this);

		//Present the frame.
		DXGI_PRESENT_PARAMETERS pp = {};
		gSwapChain4->Present1(0, 0, &pp);

		gThreadPool->push([this](int id) {Render(id); });

		//threadIDIndexLock.lock();
		//// Signal that the execution of this list is done
		////! SIGNAL MUST HAPPEN AFTER ExecuteCommandLists
		//{
		//	const UINT64 fenceVal = gCommandQueues[QUEUE_TYPE_DIRECT].mFenceValue;
		//	gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->Signal(fence, fenceVal);
		//	gCommandQueues[QUEUE_TYPE_DIRECT].mFenceValue++;
		//	gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mLastFrameWithThisAllocatorFenceValue = fenceVal;
		//}
		//threadIDIndexLock.unlock();
		//gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();
		//Wait for GPU to finish.
		//NOT BEST PRACTICE, only used as such for simplicity.


		//todo wait for previous render call with this render target index to finish the JPEG part

		// create new render thread
		//gThreadPool.push(Render);
		//std::thread(Render).detach();

	}
}
