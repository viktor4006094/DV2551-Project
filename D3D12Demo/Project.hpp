#pragma once
#include "ConstantsAndGlobals.hpp"
#include "StructsAndEnums.hpp"

#include "GameStateHandler.hpp"

#include <d3d12.h>
#include <dxgi1_6.h> //Only used for initialization of the device and swap chain.
#include <d3dcompiler.h>

#include <vector>

#include "..\extLib\ctpl_stl.h"

#include "GPUStage.hpp"


#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")
#pragma comment (lib, "d3dcompiler.lib")


__declspec(align(256)) class Project
{
public:
	friend class GPUStage;

	Project();
	~Project();

	void Init(HWND wndHandle);
	void Start();
	void Stop();
	void Shutdown();

	//Helper function for syncronization of GPU/CPU
	void WaitForGpu(QueueType type);
	void CreateDirect3DDevice(HWND wndHandle);					//2. Create Device
	void CreateCommandInterfacesAndSwapChain(HWND wndHandle);	//3. Create CommandQueue and SwapChain
	void CreateFenceAndEventHandle();							//4. Create Fence and Event handle
	void CreateRenderTargets();									//5. Create render targets for backbuffer
	void CreateViewportAndScissorRect();						//6. Create viewport and rect

	void CreateShadersAndPipelineStates();						//7. Set up the pipeline states

	void CreateTriangleData();									//8. Create vertexdata
	void CreateRootSignature();
	void CreateComputeShaderResources();
	void CreateConstantBufferResources();

	void CopyComputeOutputToBackBuffer(int index);
	void Render(int id);

	// ensure 256 bit alignment for the constant buffer
	void* operator new(size_t i)
	{
		return _mm_malloc(i, 256);
	}

	void operator delete(void* p)
	{
		_mm_free(p);
	}



	CommandQueueAndFence gCommandQueues[3];
	CommandAllocatorAndList gAllocatorsAndLists[MAX_PREPARED_FRAMES][3];

	D3D12DevPtr gDevice5 = nullptr; // ID3D12Device

	//ID3D12CommandAllocator*	gCommandAllocator					= nullptr;
	IDXGISwapChain4*		gSwapChain4 = nullptr;

	// Render targets used by the render stage
	ID3D12DescriptorHeap*	gIntermediateRenderTargetsDescHeap = nullptr;
	ID3D12Resource1*		gIntermediateRenderTargets[NUM_SWAP_BUFFERS] = {};
	
	// render targets used as the final backbuffer of the frame
	ID3D12DescriptorHeap*	gSwapChainRenderTargetsDescHeap = nullptr;
	ID3D12Resource1*		gSwapChainRenderTargets[NUM_SWAP_BUFFERS] = {};
	UINT					gRenderTargetDescriptorSize = 0;
	//UINT					gFrameIndex							= 0;


	//Compute shader UAV and SRV
	ID3D12Resource1*			gSRVofBackBuffer = nullptr;
	ID3D12DescriptorHeap*		gComputeDescriptorHeap = nullptr;
	ID3D12Resource1*			gUAVResource[NUM_SWAP_BUFFERS] = {};

	D3D12_VIEWPORT				gViewport = {};
	D3D12_RECT					gScissorRect = {};

	ID3D12RootSignature*		gRootSignature = nullptr;

	ID3D12Resource1*			gVertexBufferResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	gVertexBufferView = {};


	// Constant buffers
	ID3D12DescriptorHeap*	gDescriptorHeap[NUM_SWAP_BUFFERS] = {};
	//ID3D12Heap1*			gConstantBufferHeap[NUM_SWAP_BUFFERS] = {};
	ID3D12Resource1*		gConstantBufferResource[NUM_SWAP_BUFFERS]= {};

	/*struct alignas(256) CONSTANT_BUFFER_DATA {
		float position[4];
		float color[4];
	}cbData[TOTAL_TRIS], *pMappedCB[3];*/


	UINT mLatestBackBufferIndex = 0;

	// Thread pool
	ctpl::thread_pool* gThreadPool = nullptr; //1 CPU update, 9 render

	GameStateHandler mGameStateHandler;

	HWND mWndHandle;

	bool isRunning = true;
private:

	GPUStage* GPUStages[2];

};