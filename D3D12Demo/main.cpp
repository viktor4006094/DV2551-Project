//--------------------------------------------------------------------------------------
// BTH - 2018
// 170123: Mikael Olofsson, initial version
// 170131: Stefan Petersson, added constant buffer and some cleanups/updates
// 180129: Stefan Petersson, cleanups/updates DXGI 1.5
// 181009: Stefan Petersson, cleanups/updates DXGI 1.6
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h> //Only used for initialization of the device and swap chain.
#include <d3dcompiler.h>

#include <vector>

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")
#pragma comment (lib, "d3dcompiler.lib")

#pragma region Forward Declarations

HWND				InitWindow(HINSTANCE hInstance);	//1. Create Window
LRESULT CALLBACK	WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//Helper function for syncronization of GPU/CPU
void WaitForGpu();
//Helper function for resource transitions

void CreateDirect3DDevice(HWND wndHandle);				//2. Create Device
void CreateCommandInterfacesAndSwapChain(HWND wndHandle);	//3. Create CommandQueue and SwapChain
void CreateFenceAndEventHandle();							//4. Create Fence and Event handle
void CreateRenderTargets();									//5. Create render targets for backbuffer
void CreateViewportAndScissorRect();						//6. Create viewport and rect
void CreateShadersAndPiplelineState();						//7. Set up the pipeline state
void CreateTriangleData();									//8. Create vertexdata
void CreateRootSignature();
void CreateConstantBufferResources();
void CreateMeshes();


void	Update(int backBufferIndex);
void	Render(int backBufferIndex);

#pragma endregion

template<class Interface>
inline void SafeRelease(
	Interface **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = NULL;
	}
}

const unsigned int SCREEN_WIDTH = 640; //Width of application.
const unsigned int SCREEN_HEIGHT = 480;	//Height of application.

const unsigned int NUM_SWAP_BUFFERS = 2; //Number of buffers

struct Vertex
{
	float x,y,z; // Position
	float r,g,b; // Color
};

#pragma region Globals
#ifdef VETTIG_DATOR
ID3D12Device5*				gDevice5							= nullptr;
ID3D12GraphicsCommandList4*	gCommandList4 = nullptr;
#else
ID3D12Device4*				gDevice5 = nullptr;
ID3D12GraphicsCommandList3*	gCommandList4 = nullptr;
#endif
ID3D12CommandQueue*			gCommandQueue						= nullptr;
ID3D12CommandAllocator*		gCommandAllocator					= nullptr;
IDXGISwapChain4*			gSwapChain4							= nullptr;

ID3D12Fence1*				gFence								= nullptr;
HANDLE						gEventHandle						= nullptr;
UINT64						gFenceValue							= 0;

ID3D12DescriptorHeap*		gRenderTargetsHeap					= nullptr;
ID3D12Resource1*			gRenderTargets[NUM_SWAP_BUFFERS]	= {};
UINT						gRenderTargetDescriptorSize			= 0;
//UINT						gFrameIndex							= 0;

D3D12_VIEWPORT				gViewport							= {};
D3D12_RECT					gScissorRect						= {};

ID3D12RootSignature*		gRootSignature						= nullptr;
ID3D12PipelineState*		gPipeLineState						= nullptr;

ID3D12Resource1*			gVertexBufferResource				= nullptr;
D3D12_VERTEX_BUFFER_VIEW	gVertexBufferView					= {};

#pragma endregion

#pragma region ConstantBufferGlobals
struct ConstantBuffer
{
	float values[4];
};

ID3D12DescriptorHeap*	gDescriptorHeap[NUM_SWAP_BUFFERS]				= {};
ID3D12Resource1*		gConstantBufferResource[NUM_SWAP_BUFFERS]		= {};
//ConstantBuffer			gConstantBufferCPU								= {};
#pragma endregion


#pragma region GameState

const int num_of_meshes = 10;

struct Mesh
{
	//Vertex triangle[3]; // all meshes use the same vertices
	ConstantBuffer translate;
	ConstantBuffer color;
};

struct GameState
{
	std::vector<Mesh> meshes;
};

/*
	CPU updates writeState
	Once CPU update is done writeState is copied to bufferState
	Before each new GPU frame the bufferState is copied to readOnlyState which is used by the GPU
*/
GameState writeState;
GameState bufferState;
GameState readOnlyState;

#pragma endregion


#pragma region wwinMain
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	MSG msg			= {0};
	HWND wndHandle	= InitWindow(hInstance);			//1. Create Window
	
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

		WaitForGpu();
		
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
				UINT backBufferIndex = gSwapChain4->GetCurrentBackBufferIndex();

				Update(backBufferIndex);
				Render(backBufferIndex);
			}
		}
	}
	
	//Wait for GPU execution to be done and then release all interfaces.
	WaitForGpu();
	CloseHandle(gEventHandle);
	SafeRelease(&gDevice5);		
	SafeRelease(&gCommandQueue);
	SafeRelease(&gCommandAllocator);
	SafeRelease(&gCommandList4);
	SafeRelease(&gSwapChain4);

	SafeRelease(&gFence);

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
void WaitForGpu()
{
	//WAITING FOR EACH FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	//This is code implemented as such for simplicity. The cpu could for example be used
	//for other tasks to prepare the next frame while the current one is being rendered.

	//Signal and increment the fence value.
	const UINT64 fence = gFenceValue;
	gCommandQueue->Signal(gFence, fence);
	gFenceValue++;
	
	//Wait until command queue is done.
	if(gFence->GetCompletedValue() < fence)
	{
		gFence->SetEventOnCompletion(fence, gEventHandle);
		WaitForSingleObject(gEventHandle, INFINITE);
	}
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
	//Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC cqd = {};
	gDevice5->CreateCommandQueue(&cqd, IID_PPV_ARGS(&gCommandQueue));

	//Create command allocator. The command allocator object corresponds
	//to the underlying allocations in which GPU commands are stored.
	gDevice5->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gCommandAllocator));
	
	//Create command list.
	gDevice5->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		gCommandAllocator,
		nullptr,
		IID_PPV_ARGS(&gCommandList4));

	//Command lists are created in the recording state. Since there is nothing to
	//record right now and the main loop expects it to be closed, we close it.
	gCommandList4->Close();

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
		gCommandQueue,
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
	gDevice5->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gFence));
	gFenceValue		= 1;
	//Create an event handle to use for GPU synchronization.
	gEventHandle	= CreateEvent(0, false, false, 0);
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
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR"	, 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
		0.0f, 0.5f, 0.0f,	//v0 pos
		1.0f, 0.0f, 0.0f,	//v0 color

		0.5f, -0.5f, 0.0f,	//v1
		0.0f, 1.0f, 0.0f,	//v1 color

		-0.5f, -0.5f, 0.0f, //v2
		0.0f, 0.0f, 1.0f	//v2 color
	};

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
	float fade = 1.0 / num_of_meshes;
	for (int i = 0; i < num_of_meshes; ++i) {
		Mesh m;

		// initialize meshes with greyscale colors
		float c = 1.0 - fade * i;
		m.color = { c, c, c, 1.0 };

		writeState.meshes.push_back(m);
	}
}
#pragma endregion

#pragma region Update
void Update(int backBufferIndex)
{
	//Update color values in constant buffer
	for (auto &m : writeState.meshes) {
		for (int i = 0; i < 3; i++)
		{
			//gConstantBufferCPU.colorChannel[i] += 0.0001f * (i + 1);
			m.color.values[i] += 0.0001f * (i + 1);
			if (m.color.values[i] > 1)
			{
				m.color.values[i] = 0;
			}
		}
	}

	//todo
	//Update translation values in constant buffer


	// todo mutex lock or something
	bufferState = writeState;


	////Update GPU memory
	//void* mappedMem = nullptr;
	//D3D12_RANGE readRange = { 0, 0 }; //We do not intend to read this resource on the CPU.
	//if (SUCCEEDED(gConstantBufferResource[backBufferIndex]->Map(0, &readRange, &mappedMem)))
	//{
	//	//memcpy(mappedMem, &gConstantBufferCPU, sizeof(ConstantBuffer));

	//	D3D12_RANGE writeRange = { 0, sizeof(ConstantBuffer) };
	//	gConstantBufferResource[backBufferIndex]->Unmap(0, &writeRange);
	//}
}
#pragma endregion

#pragma region Render
void Render(int backBufferIndex)
{
	// todo mutex lock or something
	readOnlyState = bufferState;
	
	
	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	gCommandAllocator->Reset();
	gCommandList4->Reset(gCommandAllocator, gPipeLineState);

	//Set constant buffer descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { gDescriptorHeap[backBufferIndex] };
	gCommandList4->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);
	
	//Set root signature
	gCommandList4->SetGraphicsRootSignature(gRootSignature);

	//Set root descriptor table to index 0 in previously set root signature
	gCommandList4->SetGraphicsRootDescriptorTable(0,
		gDescriptorHeap[backBufferIndex]->GetGPUDescriptorHandleForHeapStart());

	//Set necessary states.
	gCommandList4->RSSetViewports(1, &gViewport);
	gCommandList4->RSSetScissorRects(1, &gScissorRect);

	//Indicate that the back buffer will be used as render target.
	SetResourceTransitionBarrier(gCommandList4,
		gRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_PRESENT,		//state before
		D3D12_RESOURCE_STATE_RENDER_TARGET	//state after
	);
	
	//Record commands.
	//Get the handle for the current render target used as back buffer.
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = gRenderTargetsHeap->GetCPUDescriptorHandleForHeapStart();
	cdh.ptr += gRenderTargetDescriptorSize * backBufferIndex;

	gCommandList4->OMSetRenderTargets(1, &cdh, true, nullptr);
	
	float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	gCommandList4->ClearRenderTargetView(cdh, clearColor, 0, nullptr);
	
	gCommandList4->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gCommandList4->IASetVertexBuffers(0, 1, &gVertexBufferView);


	for (auto &m : readOnlyState.meshes) {
		const void* translateConstant = &m.translate;
		gCommandList4->SetGraphicsRoot32BitConstants(0, 4, translateConstant, 0);

		const void* colorConstant = &m.color;
		gCommandList4->SetGraphicsRoot32BitConstants(1, 4, colorConstant, 0);

		gCommandList4->DrawInstanced(3, 1, 0, 0);
	}



	
	//Indicate that the back buffer will now be used to present.
	SetResourceTransitionBarrier(gCommandList4,
		gRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_RENDER_TARGET,	//state before
		D3D12_RESOURCE_STATE_PRESENT		//state after
	);
	
	//Close the list to prepare it for execution.
	gCommandList4->Close();

	//Execute the command list.
	ID3D12CommandList* listsToExecute[] = { gCommandList4 };
	gCommandQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);

	//Present the frame.
	DXGI_PRESENT_PARAMETERS pp = {};
	gSwapChain4->Present1(0, 0, &pp);

	WaitForGpu(); //Wait for GPU to finish.
				  //NOT BEST PRACTICE, only used as such for simplicity.
}
#pragma endregion