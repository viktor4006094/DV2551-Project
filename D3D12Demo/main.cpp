//--------------------------------------------------------------------------------------
// BTH - 2018
// 170123: Mikael Olofsson, initial version
// 170131: Stefan Petersson, added constant buffer and some cleanups/updates
// 180129: Stefan Petersson, cleanups/updates DXGI 1.5
// 181009: Stefan Petersson, cleanups/updates DXGI 1.6
//--------------------------------------------------------------------------------------

#include "main.hpp"
#include <thread>
#include <mutex>
#include <vector>


#include "..\extLib\ctpl_stl.h"

struct Vertex
{
	float x,y,z,w; // Position
	//float r,g,b; // Color
};



bool isRunning = true;
std::mutex threadIDIndexLock;
std::mutex bufferTransferLock;




#pragma region wwinMain
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	MSG msg			= {0};
	wndHandle	= InitWindow(hInstance);			//1. Create Window
	if(wndHandle)
	{
		CreateDirect3DDevice(wndHandle);					//2. Create Device
		
		CreateCommandInterfacesAndSwapChain(wndHandle);	//3. Create CommandQueue and SwapChain
		
		CreateFenceAndEventHandle();						//4. Create Fence and Event handle
		
		CreateRenderTargets();								//5. Create render targets for backbuffer
		
		CreateViewportAndScissorRect();						//6. Create viewport and rect

		CreateRootSignature();								//7. Create root signature

		CreateShadersAndPiplelineState();					//8. Set up the pipeline state

		CreateConstantBufferResources();					//9. Create constant buffer data
	
		CreateTriangleData();								//10. Create vertexdata

		CreateMeshes();										//11. Create meshes (all use same triangle but different constant buffers)

		WaitForGpu(QUEUE_TYPE_DIRECT);
		
		std::thread(Update).detach();
		std::thread(Render).detach();
		ShowWindow(wndHandle, nCmdShow);
		while(WM_QUIT != msg.message)
		{
			if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				
			}
			else
			{
				//Update();
				//Render();
			}
		}
		isRunning = false;
	}
	
	//Wait for GPU execution to be done and then release all interfaces.
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
	SafeRelease(&gPipeLineState);

	SafeRelease(&gVertexBufferResource);

	return (int)msg.wParam;
}
#pragma endregion

#pragma region InitWindow
HWND InitWindow(HINSTANCE hInstance)//1. Create Window
{
	WNDCLASSEX wcex		= {0};
	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.lpfnWndProc	= WndProc;
	wcex.hInstance		= hInstance;
	wcex.lpszClassName	= L"DV2551_Project";
	if (!RegisterClassEx(&wcex))
	{
		return false;
	}

	RECT rc = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

	return CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,
		L"DV2551_Project",
		L"DV2551 JPEG Project",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);
}
#pragma endregion

#pragma region WndProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) 
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;		
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
#pragma endregion

#pragma region WaitForGpu
void WaitForGpu(QueueType type)
{
	gCommandQueues[type].WaitForGpu();
}
#pragma endregion

#pragma region SetResourceTransitionBarrier
void SetResourceTransitionBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
	D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter)
{
	D3D12_RESOURCE_BARRIER barrierDesc = {};

	barrierDesc.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource	= resource;
	barrierDesc.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore	= StateBefore;
	barrierDesc.Transition.StateAfter	= StateAfter;

	commandList->ResourceBarrier(1, &barrierDesc);
}
#pragma endregion

#pragma region CreateDirect3DDevice
void CreateDirect3DDevice(HWND wndHandle)
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
	for(UINT adapterIndex = 0;; ++adapterIndex)
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
	if(adapter)
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
#pragma endregion

#pragma region CreateCommandInterfacesAndSwapChain
void CreateCommandInterfacesAndSwapChain(HWND wndHandle)
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
		gAllocatorsAndLists[i][0].CreateCommandListAndAllocator(QUEUE_TYPE_DIRECT);
		gAllocatorsAndLists[i][1].CreateCommandListAndAllocator(QUEUE_TYPE_COPY);
		gAllocatorsAndLists[i][2].CreateCommandListAndAllocator(QUEUE_TYPE_COMPUTE);
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
#pragma endregion

#pragma region CreateFenceAndEventHandle
void CreateFenceAndEventHandle()
{
	gCommandQueues[QUEUE_TYPE_DIRECT].CreateFenceAndEventHandle();
	gCommandQueues[QUEUE_TYPE_COPY].CreateFenceAndEventHandle();
	gCommandQueues[QUEUE_TYPE_COMPUTE].CreateFenceAndEventHandle();
}
#pragma endregion

#pragma region CreateRenderTargets
void CreateRenderTargets()
{
	//Create descriptor heap for render target views.
	D3D12_DESCRIPTOR_HEAP_DESC dhd	= {};
	dhd.NumDescriptors				= NUM_SWAP_BUFFERS;
	dhd.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	HRESULT hr = gDevice5->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&gRenderTargetsHeap));
	
	//Create resources for the render targets.
	gRenderTargetDescriptorSize = gDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = gRenderTargetsHeap->GetCPUDescriptorHandleForHeapStart();
	
	//One RTV for each frame.
	for(UINT n = 0; n < NUM_SWAP_BUFFERS; n++)
	{
		hr = gSwapChain4->GetBuffer(n, IID_PPV_ARGS(&gRenderTargets[n]));
		gDevice5->CreateRenderTargetView(gRenderTargets[n], nullptr, cdh);
		cdh.ptr += gRenderTargetDescriptorSize;
	}
}
#pragma endregion

#pragma region CreateViewportAndScissorRect
void CreateViewportAndScissorRect()
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
	gScissorRect.bottom	= (long)SCREEN_HEIGHT;
}
#pragma endregion

#pragma region CreateConstantBufferResources
void CreateConstantBufferResources()
{
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
		heapDescriptorDesc.NumDescriptors = 1;
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
	resourceDesc.Width = cbSizeAligned;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//Create a resource heap, descriptor heap, and pointer to cbv for each frame
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		gDevice5->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&gConstantBufferResource[i])
		);

		gConstantBufferResource[i]->SetName(L"cb heap");

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = gConstantBufferResource[i]->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = cbSizeAligned;
		gDevice5->CreateConstantBufferView(&cbvDesc, gDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());
	}
}
#pragma endregion

#pragma region CreateRootSignature
void CreateRootSignature()
{
	//define descriptor range(s)
	D3D12_DESCRIPTOR_RANGE  dtRanges[1];
	dtRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	dtRanges[0].NumDescriptors = 1; //only one CB in this example
	dtRanges[0].BaseShaderRegister = 0; //register b0
	dtRanges[0].RegisterSpace = 0; //register(b0,space0);
	dtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE dt;
	dt.NumDescriptorRanges = ARRAYSIZE(dtRanges);
	dt.pDescriptorRanges = dtRanges;

	//create root parameter
	D3D12_ROOT_PARAMETER rootParam[2];

	// constant translate 
	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].Constants = { 0, 0, 4 }; // 4 constants in b0 first register space
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// constant color
	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].Constants = { 1, 0, 4 }; // 4 constants in b0 first register space
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rsDesc.NumParameters = ARRAYSIZE(rootParam);
	rsDesc.pParameters = rootParam;
	rsDesc.NumStaticSamplers = 0;
	rsDesc.pStaticSamplers = nullptr;

	ID3DBlob* sBlob;
	D3D12SerializeRootSignature(
		&rsDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&sBlob,
		nullptr);

	gDevice5->CreateRootSignature(
		0,
		sBlob->GetBufferPointer(),
		sBlob->GetBufferSize(),
		IID_PPV_ARGS(&gRootSignature));
}
#pragma endregion

#pragma region CreateShadersAndPipelineState
void CreateShadersAndPiplelineState()
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

	////// Input Layout //////
	//D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "COLOR"	, 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	//};	
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
	inputLayoutDesc.pInputElementDescs = inputElementDesc;
	inputLayoutDesc.NumElements = ARRAYSIZE(inputElementDesc);

	////// Pipline State //////
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsd = {};

	//Specify pipeline stages:
	gpsd.pRootSignature = gRootSignature;
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

	gDevice5->CreateGraphicsPipelineState(&gpsd, IID_PPV_ARGS(&gPipeLineState));
}
#pragma endregion

#pragma region CreateTriangleData
void CreateTriangleData()
{
	Vertex triangleVertices[3] = 
	{ 
		{ 0.0f,  0.05f, 0.0f, 1.0f },
		{ 0.05f, -0.05f, 0.0f, 1.0f },
		{ -0.05f, -0.05f, 0.0f, 1.0f } 
	};

	//Vertex triangleVertices[3] =
	//{
	//	0.0f, 0.5f, 0.0f,	//v0 pos
	//	//1.0f, 0.0f, 0.0f,	//v0 color

	//	0.5f, -0.5f, 0.0f,	//v1
	//	//0.0f, 1.0f, 0.0f,	//v1 color

	//	-0.5f, -0.5f, 0.0f, //v2
	//	//0.0f, 0.0f, 1.0f	//v2 color
	//};

	//Note: using upload heaps to transfer static data like vert buffers is not 
    //recommended. Every time the GPU needs it, the upload heap will be marshalled 
    //over. Please read up on Default Heap usage. An upload heap is used here for 
    //code simplicity and because there are very few vertices to actually transfer.
	D3D12_HEAP_PROPERTIES hp	= {};
	hp.Type						= D3D12_HEAP_TYPE_UPLOAD;
	hp.CreationNodeMask			= 1;
	hp.VisibleNodeMask			= 1;

	D3D12_RESOURCE_DESC rd	= {};
	rd.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width				= sizeof(triangleVertices);
	rd.Height				= 1;
	rd.DepthOrArraySize		= 1;
	rd.MipLevels			= 1;
	rd.SampleDesc.Count		= 1;
	rd.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

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
	gVertexBufferView.BufferLocation	= gVertexBufferResource->GetGPUVirtualAddress();
	gVertexBufferView.StrideInBytes		= sizeof(Vertex);
	gVertexBufferView.SizeInBytes		= sizeof(triangleVertices);
}
#pragma endregion

#pragma region CreateMeshes
void CreateMeshes()
{
	// create the translation arrays used for moving the triangles around on the screen
	float degToRad = (float)M_PI / 180.0f;
	float scale = (float)TOTAL_PLACES / 359.9f;
	for (int a = 0; a < TOTAL_PLACES; a++)
	{
		xt[a] = 0.8f * cosf(degToRad * ((float)a / scale) * 3.0f);
		yt[a] = 0.8f * sinf(degToRad * ((float)a / scale) * 2.0f);
	};

	float fade = 1.0f / TOTAL_TRIS;
	for (int i = 0; i < TOTAL_TRIS; ++i) {
		TriangleObject m;

		// initialize meshes with greyscale colors
		float c = 1.0f - fade * i;
		m.color = { c, c, c, 1.0 };

		writeState.meshes.push_back(m);
	}
}
#pragma endregion

#pragma region FPSCounter
void CountFPS()
{
	// FPS counter
	static auto lastTime = std::chrono::high_resolution_clock::now();
	static int frames = 0;
	auto currentTime = std::chrono::high_resolution_clock::now();
	frames++;

	std::chrono::duration<double, std::milli> delta = currentTime - lastTime;
	if (delta >= std::chrono::duration<double, std::milli>(250)) {
		lastTime = std::chrono::high_resolution_clock::now();
		int fps = frames * 4;
		std::string windowText = "FPS " + std::to_string(fps);
		SetWindowTextA(wndHandle, windowText.c_str());
		frames = 0;
	}
}
#pragma endregion

#pragma region Update
void Update()
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	while (isRunning) {
		int meshInd = 0;
		static long long shift = 0;
		static double dshift = 0.0;
		static double delta = 0.0;


		for (auto &m : writeState.meshes) {

			//Update color values in constant buffer
			for (int i = 0; i < 3; i++)
			{
				//gConstantBufferCPU.colorChannel[i] += 0.0001f * (i + 1);
				m.color.values[i] += delta/10000.0f * (i + 1);
				if (m.color.values[i] > 1)
				{
					m.color.values[i] = 0;
				}
			}

			//Update positions of each mesh
			m.translate = ConstantBuffer{
				xt[(int)(float)(meshInd * 10 + dshift) % (TOTAL_PLACES)],
				yt[(int)(float)(meshInd * 10 + dshift) % (TOTAL_PLACES)],
				meshInd * (-1.0f / TOTAL_PLACES),
				0.0f
			};

			meshInd++;
		}
		shift += max((long long)(TOTAL_TRIS / 1000.0), (long long)(TOTAL_TRIS / 100.0));
		delta = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count();
		dshift += delta*50.0;
		startTime = std::chrono::high_resolution_clock::now();

		//copy updated gamestate to bufferstate
		bufferTransferLock.lock();
		bufferState = writeState;
		bufferTransferLock.unlock();
	}
}
#pragma endregion

#pragma region Render
void Render()
{
	static size_t lastThreadIndex = 0;
	CountFPS();

	if (isRunning) {
		thread_local int index;

		//get the thread index
		threadIDIndexLock.lock();
		index = lastThreadIndex;
		lastThreadIndex = (++lastThreadIndex) % MAX_PREPARED_FRAMES;
		threadIDIndexLock.unlock();

		UINT backBufferIndex = gSwapChain4->GetCurrentBackBufferIndex();
		//Command list allocators can only be reset when the associated command lists have
		//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
		ID3D12CommandAllocator* directAllocator = gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mAllocator;
#ifdef VETTIG_DATOR
		ID3D12GraphicsCommandList4*	commandList = gAllocatorsAndLists[QUEUE_TYPE_DIRECT].mCommandList;
#else
		ID3D12GraphicsCommandList3*	directList = gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mCommandList;
#endif
		ID3D12Fence1* fence = gCommandQueues[QUEUE_TYPE_DIRECT].mFence;
		HANDLE eventHandle = gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mEventHandle;


		////! since WaitForGPU is called just before the commandlist is executed in the previous frame this does 
		////! not need to be done here since the command allocator is already guaranteed to have finished executing
		//// wait for previous usage of this command allocator to be done executing before it is reset
		//UINT64 prevFence = gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mLastFrameWithThisAllocatorFenceValue;
		//if (fence->GetCompletedValue() < prevFence) {
		//	fence->SetEventOnCompletion(prevFence, eventHandle);
		//	WaitForSingleObject(eventHandle, INFINITE);
		//}

		directAllocator->Reset();
		directList->Reset(directAllocator, gPipeLineState);


		//Set root signature
		directList->SetGraphicsRootSignature(gRootSignature);

		//Set necessary states.
		directList->RSSetViewports(1, &gViewport);
		directList->RSSetScissorRects(1, &gScissorRect);

		//Indicate that the back buffer will be used as render target.
		SetResourceTransitionBarrier(directList,
			gRenderTargets[backBufferIndex],
			D3D12_RESOURCE_STATE_PRESENT,		//state before
			D3D12_RESOURCE_STATE_RENDER_TARGET	//state after
		);

		//Record commands.
		//Get the handle for the current render target used as back buffer.
		D3D12_CPU_DESCRIPTOR_HANDLE cdh = gRenderTargetsHeap->GetCPUDescriptorHandleForHeapStart();
		cdh.ptr += gRenderTargetDescriptorSize * backBufferIndex;

		directList->OMSetRenderTargets(1, &cdh, true, nullptr);

		float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		directList->ClearRenderTargetView(cdh, clearColor, 0, nullptr);

		directList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		directList->IASetVertexBuffers(0, 1, &gVertexBufferView);

		//get the current game state
		bufferTransferLock.lock();
		readOnlyState[index] = bufferState;
		bufferTransferLock.unlock();

		//Update constant buffers and draw triangles
		//for (int i = 0; i < 100; ++i) {
		for (auto &m : readOnlyState[index].meshes) {
			directList->SetGraphicsRoot32BitConstants(0, 4, &m.translate, 0);
			directList->SetGraphicsRoot32BitConstants(1, 4, &m.color, 0);

			directList->DrawInstanced(3, 1, 0, 0);
		}
		//}

		//Indicate that the back buffer will now be used to present.
		SetResourceTransitionBarrier(directList,
			gRenderTargets[backBufferIndex],
			D3D12_RESOURCE_STATE_RENDER_TARGET,	//state before
			D3D12_RESOURCE_STATE_PRESENT		//state after
		);

		//Close the list to prepare it for execution.
		directList->Close();



		//wait for current frame to finish rendering before pushing the next one to GPU
		gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

		// set the just executed command allocator and list to inactive
		//gAllocatorsAndLists[(index + MAX_THREAD_COUNT - 1) % MAX_THREAD_COUNT][QUEUE_TYPE_DIRECT].isActive = false;

		//Execute the command list.
		ID3D12CommandList* listsToExecute[] = { directList };
		gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);

		//Present the frame.
		DXGI_PRESENT_PARAMETERS pp = {};
		gSwapChain4->Present1(0, 0, &pp);


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

		// create new render thread
		std::thread(Render).detach();
	}
}
#pragma endregion