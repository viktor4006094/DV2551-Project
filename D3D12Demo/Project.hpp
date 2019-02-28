#pragma once
#include "ConstantsAndGlobals.hpp"
#include "StructsAndEnums.hpp"

#include "GameStateHandler.hpp"

#include <d3d12.h>
#include <dxgi1_6.h> //Only used for initialization of the device and swap chain.
#include <d3dcompiler.h>

#include <vector>

#include "..\extLib\ctpl_stl.h"


#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")
#pragma comment (lib, "d3dcompiler.lib")


class Project
{
public:
	Project();
	~Project();

	void Init(HWND wndHandle);
	void Start();
	void Stop();
	void Shutdown();

	//Helper function for syncronization of GPU/CPU
	void WaitForGpu(QueueType type);
	//Helper function for resource transitions
	void SetResourceTransitionBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
		D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter);
	void CreateDirect3DDevice(HWND wndHandle);					//2. Create Device
	void CreateCommandInterfacesAndSwapChain(HWND wndHandle);	//3. Create CommandQueue and SwapChain
	void CreateFenceAndEventHandle();							//4. Create Fence and Event handle
	void CreateRenderTargets();									//5. Create render targets for backbuffer
	void CreateViewportAndScissorRect();						//6. Create viewport and rect

	void CreateShadersAndPipelineStates();						//7. Set up the pipeline states
	void CreateRenderShadersAndPiplelineState();
	void CreateComputeShadersAndPiplelineState();
	void CreatePassthroughShadersAndPiplelineState();

	void CreateTriangleData();									//8. Create vertexdata
	void CreateRootSignature();
	void CreateComputeShaderResources();
	void CreateConstantBufferResources();
	//void CreateMeshes();

	void CountFPS();
	//void Update(int id);
	void Render(int id);
	void ComputePass(int index);
	void PassthroughPass(int index);



	CommandQueueAndFence gCommandQueues[3];
	CommandAllocatorAndList gAllocatorsAndLists[MAX_PREPARED_FRAMES][3];


	D3D12DevPtr gDevice5 = nullptr; // ID3D12Device

	//ID3D12CommandAllocator*		gCommandAllocator					= nullptr;
	IDXGISwapChain4*			gSwapChain4 = nullptr;

	ID3D12DescriptorHeap*		gRenderTargetsHeap = nullptr;
	ID3D12Resource1*			gRenderTargets[NUM_SWAP_BUFFERS] = {};
	UINT						gRenderTargetDescriptorSize = 0;
	//UINT						gFrameIndex							= 0;

	//Compute shader UAV and SRV
	ID3D12Resource1*			gSRVofBackBuffer = nullptr;
	ID3D12DescriptorHeap*		gComputeDescriptorHeap = nullptr;
	ID3D12Resource1*			gUAVResource = nullptr;

	D3D12_VIEWPORT				gViewport = {};
	D3D12_RECT					gScissorRect = {};

	ID3D12RootSignature*		gRootSignature = nullptr;
	ID3D12PipelineState*		gRenderPipeLineState = nullptr;
	ID3D12PipelineState*		gComputePipeLineState = nullptr;
	// only for testing
	ID3D12PipelineState*		gPassthroughPipeLineState = nullptr;


	ID3D12Resource1*			gVertexBufferResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	gVertexBufferView = {};



	ID3D12DescriptorHeap*	gDescriptorHeap[NUM_SWAP_BUFFERS] = {};
	ID3D12Resource1*		gConstantBufferResource[NUM_SWAP_BUFFERS] = {};

	// Thread pool
	ctpl::thread_pool* gThreadPool = nullptr; //1 CPU update, 9 render

	GameStateHandler mGameStateHandler;

	HWND mWndHandle;

	bool isRunning = true;
private:

#pragma region GameState


	

	



#pragma endregion



};