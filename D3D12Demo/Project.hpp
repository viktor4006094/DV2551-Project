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


class Project
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
	void CreateIntermediateRenderTargets();
	void CreateComputeShaderResources();
	void CreateConstantBufferResources();
	//void CreateMeshes();

	//void CountFPS();
	//void Update(int id);
	void Render(int id);



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

	// todo in the render pass write to buffers that aren't in the swapchain
	/*ID3D12DescriptorHeap*		gComputeInputTargetsHeap = nullptr;
	ID3D12Resource1*			gComputeInputTargets[NUM_SWAP_BUFFERS] = {};
	UINT						gComputeInputTargetDescriptorSize = 0;*/


	//Compute shader UAV and SRV
	ID3D12Resource1*			gSRVofBackBuffer = nullptr;
	ID3D12DescriptorHeap*		gComputeDescriptorHeap = nullptr;
	ID3D12Resource1*			gUAVResource = nullptr;

	D3D12_VIEWPORT				gViewport = {};
	D3D12_RECT					gScissorRect = {};

	ID3D12RootSignature*		gRootSignature = nullptr;
	//ID3D12PipelineState*		gRenderPipeLineState = nullptr;
	//ID3D12PipelineState*		gComputePipeLineState = nullptr;
	// only for testing
	//ID3D12PipelineState*		gPassthroughPipeLineState = nullptr;


	ID3D12Resource1*			gVertexBufferResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	gVertexBufferView = {};


	// Constant buffers
	ID3D12DescriptorHeap*	gDescriptorHeap[NUM_SWAP_BUFFERS] = {};
	ID3D12Resource1*		gConstantBufferResource[NUM_SWAP_BUFFERS] = {};

	// Thread pool
	ctpl::thread_pool* gThreadPool = nullptr; //1 CPU update, 9 render

	GameStateHandler mGameStateHandler;

	HWND mWndHandle;

	bool isRunning = true;
private:

	GPUStage* GPUStages[2];

#pragma region GameState


	

	



#pragma endregion



};