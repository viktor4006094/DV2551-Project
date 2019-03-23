// todo rename swap buffer to frame buffer

#pragma once
#include "ConstantsAndGlobals.hpp"
#include "StructsAndEnums.hpp"
#include "GameStateHandler.hpp"
#include "GPUStage.hpp"

#include <d3d12.h>
#include <dxgi1_6.h> //Only used for initialization of the device and swap chain.
#include <d3dcompiler.h>
#include <vector>
#include <chrono>

#include "..\extLib\ctpl_stl.h"
#include "D3D12Timer.h"

//for debug
#include <guiddef.h>
#include <dxgidebug.h>

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")
#pragma comment (lib, "dxguid.lib")
#pragma comment (lib, "d3dcompiler.lib")


// alignment for the constant buffer
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

	void CreateDirect3DDevice(HWND wndHandle);
	void CreateCommandInterfacesAndSwapChain(HWND wndHandle);
	void CreateFencesAndEventHandles();
	void CreateRenderTargets();
	void CreateViewportAndScissorRect();
	void CreateShadersAndPipelineStates();
	void UploadMeshData();
	void CreateRootSignature();
	void CreateComputeShaderResources();
	void CreateConstantBufferResources();

	void CopyComputeOutputToBackBuffer(UINT64 frameIndex, int swapBufferIndex, int threadIndex);
	void Render(int id);

	// ensure 256 bit alignment for the constant buffer
	void* operator new(size_t i)   { return _mm_malloc(i, 256); }
	void  operator delete(void* p) { _mm_free(p); }


	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	//Enable the D3D12 debug layer.
	ID3D12Debug* debugController = nullptr;
#endif

	/// Global
	ctpl::thread_pool* gThreadPool = nullptr;
	GameStateHandler mGameStateHandler;

	HWND mWndHandle;
	
	/// Global D3D
	D3D12DevPtr				gDevice5 = nullptr; // ID3D12Device

	ID3D12CommandQueue*		gCommandQueues[2];			// One Direct and one Compute queue
	ID3D12RootSignature*	gRootSignature = nullptr;	// only one root signature needed

	IDXGISwapChain4*		gSwapChain4	             = nullptr;
	HANDLE					gSwapChainWaitableObject = nullptr;
	
	D3D12_VIEWPORT			gViewport    = {};
	D3D12_RECT				gScissorRect = {};

	// Constant buffers
	ID3D12DescriptorHeap*	gConstantBufferDescriptorHeap = nullptr;
	ID3D12Resource1*		gConstantBufferResource       = nullptr;

	// Depth Stencil
	ID3D12Resource*			depthStencilBuffer = nullptr;
	ID3D12DescriptorHeap*	dsDescriptorHeap   = nullptr;



	/// Per swap buffer
	ID3D12DescriptorHeap*	gIntermediateRenderTargetsDescHeap   = nullptr; // Descriptor heap for the RTVs used by the geometry stage
	ID3D12DescriptorHeap*	gComputeDescriptorHeap				 = nullptr; // Descriptor heap for the UAVs and SRVs used by the geometry stage

	// Each swap buffer has its own commandlists, allocators, RTV/SRV, and UAV
	PerFrame				gPerFrameAllocatorsListsAndResources[NUM_SWAP_BUFFERS];

	// render targets used as the final backbuffer of the frame
	ID3D12DescriptorHeap*	gSwapChainRenderTargetsDescHeap = nullptr;
	ID3D12Resource1*		gSwapChainRenderTargets[NUM_SWAP_BUFFERS] = {};
	UINT					gRenderTargetDescriptorSize = 0;


	/// Mesh data
	ID3D12Resource1*			gVertexBufferResource = nullptr;
	ID3D12Resource1*			gVertexBufferNormalResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	gVertexBufferViews[2] = {};
	ID3D12Resource1*			gVertexStagingBufferResource = nullptr;
	ID3D12Resource1*			gNormalStagingBufferResource = nullptr;
	

	/// Fences

	// ensures that frames are presented in the correct order
	ID3D12Fence1* gBackBufferFence = nullptr;
	HANDLE gBackBufferFenceEvent[NUM_SWAP_BUFFERS] = { nullptr };

	// Fences used within frames for synchronization between the direct and compute queue
	ID3D12Fence1* gSwapBufferFences[NUM_SWAP_BUFFERS] = { nullptr };
	UINT64 gSwapBufferFenceValues[NUM_SWAP_BUFFERS]   = { 0 };

	bool isRunning = true;

#ifdef RECORD_TIME
	D3D12::D3D12Timer gpuTimer[3];

	struct CPUTimeStampPair
	{
		LARGE_INTEGER Start;
		LARGE_INTEGER Stop;
	};

	struct ClockCalibration
	{
		UINT64 gpuTimeStamp;
		UINT64 cpuTimeStamp;
	} mClockCalibration;

	CPUTimeStampPair mCPUTimeStamps[NUM_TIMESTAMP_PAIRS][3];
#endif


private:
	GPUStage* GPUStages[2];

};