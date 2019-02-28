#pragma once
#include "Globals.hpp"

#include <d3d12.h>
#include <dxgi1_6.h> //Only used for initialization of the device and swap chain.
#include <d3dcompiler.h>

#include <vector>

#include "..\extLib\ctpl_stl.h"


#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")
#pragma comment (lib, "d3dcompiler.lib")


template<class Interface>
inline void SafeRelease(Interface **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL) {
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

#pragma region Enums
enum QueueType : size_t {
	QUEUE_TYPE_DIRECT = 0,
	QUEUE_TYPE_COPY = 1,
	QUEUE_TYPE_COMPUTE = 2
};
#pragma endregion

#pragma region HelperStructs
struct ConstantBuffer
{
	float values[4];
};

struct Vertex
{
	float x, y, z, w; // Position
	//float r,g,b; // Color
};

struct CommandQueueAndFence
{
	ID3D12CommandQueue* mQueue = nullptr;
	ID3D12Fence1*		mFence = nullptr;
	HANDLE				mEventHandle = nullptr;
	UINT64				mFenceValue = 0;

	void CreateFenceAndEventHandle()
	{
		gDevice5->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
		mFenceValue = 1;
		//Create an event handle to use for GPU synchronization.
		mEventHandle = CreateEvent(0, false, false, 0);
	}

	void Release()
	{
		CloseHandle(mEventHandle);
		SafeRelease(&mQueue);
		SafeRelease(&mFence);
	}

	void WaitForGpu() {
		//WAITING FOR EACH FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
		//This is code implemented as such for simplicity. The cpu could for example be used
		//for other tasks to prepare the next frame while the current one is being rendered.

		//Signal and increment the fence value.
		const UINT64 fence = mFenceValue;

		mQueue->Signal(mFence, fence);

		mFenceValue++;

		//Wait until command queue is done.
		if (mFence->GetCompletedValue() < fence)
		{
			mFence->SetEventOnCompletion(fence, mEventHandle);
			WaitForSingleObject(mEventHandle, INFINITE);
		}
	}
};

struct CommandAllocatorAndList
{
	ID3D12CommandAllocator*		mAllocator = nullptr;
#ifdef VETTIG_DATOR
	ID3D12GraphicsCommandList4*	mCommandList = nullptr;
#else
	ID3D12GraphicsCommandList3*	mCommandList = nullptr;
#endif
	UINT64 mLastFrameWithThisAllocatorFenceValue = 0;
	HANDLE				mEventHandle = nullptr;

	void CreateCommandListAndAllocator(QueueType type)
	{
		D3D12_COMMAND_LIST_TYPE listType = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (type == QUEUE_TYPE_COPY) { listType = D3D12_COMMAND_LIST_TYPE_COPY; }
		if (type == QUEUE_TYPE_COMPUTE) { listType = D3D12_COMMAND_LIST_TYPE_COMPUTE; }

		gDevice5->CreateCommandAllocator(
			listType,
			IID_PPV_ARGS(&mAllocator));

		//Create command list.
		gDevice5->CreateCommandList(
			0,
			listType,
			mAllocator,
			nullptr,
			IID_PPV_ARGS(&mCommandList));

		//Command lists are created in the recording state. Since there is nothing to
		//record right now and the main loop expects it to be closed, we close it.
		mCommandList->Close();

		mEventHandle = CreateEvent(0, false, false, 0);

	}

	void Release()
	{
		CloseHandle(mEventHandle);
		SafeRelease(&mAllocator);
		SafeRelease(&mCommandList);
	}
};
#pragma endregion



// Thread pool
ctpl::thread_pool gThreadPool(10); //1 CPU update, 9 render


class Project
{
public:
	Project();
	~Project();

	void Init(HWND wndHandle);
	void Start();
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
	void CreateMeshes();

	void CountFPS();
	void Update(int id);
	void Render(int id);
	void ComputePass(int index);
	void PassthroughPass(int index);



	CommandQueueAndFence gCommandQueues[3];
	CommandAllocatorAndList gAllocatorsAndLists[MAX_PREPARED_FRAMES][3];


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


	
private:

#pragma region GameState

	
	float xt[TOTAL_PLACES], yt[TOTAL_PLACES];

	struct TriangleObject
	{
		//Vertex triangle[3]; // all meshes use the same vertices
		ConstantBuffer translate;
		ConstantBuffer color;
	};

	struct GameState
	{
		std::vector<TriangleObject> meshes;
	};

	/*
		CPU updates writeState
		Once CPU update is done writeState is copied to bufferState
		Before each new GPU frame the bufferState is copied to readOnlyState which is used by the GPU
	*/
	GameState writeState;
	GameState bufferState;
	GameState readOnlyState[MAX_PREPARED_FRAMES]; // might not be needed for correctness



#pragma endregion



};