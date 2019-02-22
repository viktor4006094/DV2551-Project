#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h> //Only used for initialization of the device and swap chain.
#include <d3dcompiler.h>

#include <vector>

#define _USE_MATH_DEFINES
#include <math.h>

#include <chrono>
#include <ctime>
#include <string>

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")
#pragma comment (lib, "d3dcompiler.lib")



#pragma region Enums
enum QueueType : size_t {
	QUEUE_TYPE_DIRECT = 0,
	QUEUE_TYPE_COPY = 1,
	QUEUE_TYPE_COMPUTE = 2
};
#pragma endregion


template<class Interface>
inline void SafeRelease(Interface **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL) {
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

const unsigned int SCREEN_WIDTH = 640; //Width of application.
const unsigned int SCREEN_HEIGHT = 480;	//Height of application.

const unsigned int MAX_PREPARED_FRAMES = 3; // number of frames that can be queued
const unsigned int NUM_SWAP_BUFFERS = 3; //Number of buffers


#pragma region Forward Declarations

HWND				InitWindow(HINSTANCE hInstance);	//1. Create Window
LRESULT CALLBACK	WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
void CreateShadersAndPiplelineState();						//7. Set up the pipeline state
void CreateTriangleData();									//8. Create vertexdata
void CreateRootSignature();
void CreateConstantBufferResources();
void CreateMeshes();

void CountFPS();
void Update();
void Render();


struct CommandQueueAndFence;
struct CommandAllocatorAndList;

#pragma endregion


#pragma region Globals
HWND wndHandle;




#ifdef VETTIG_DATOR
ID3D12Device5*				gDevice5 = nullptr;
#else
ID3D12Device4*				gDevice5 = nullptr;
#endif



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



CommandQueueAndFence gCommandQueues[3];
CommandAllocatorAndList gAllocatorsAndLists[MAX_PREPARED_FRAMES][3];


//ID3D12CommandAllocator*		gCommandAllocator					= nullptr;
IDXGISwapChain4*			gSwapChain4 = nullptr;

ID3D12DescriptorHeap*		gRenderTargetsHeap = nullptr;
ID3D12Resource1*			gRenderTargets[NUM_SWAP_BUFFERS] = {};
UINT						gRenderTargetDescriptorSize = 0;
//UINT						gFrameIndex							= 0;

D3D12_VIEWPORT				gViewport = {};
D3D12_RECT					gScissorRect = {};

ID3D12RootSignature*		gRootSignature = nullptr;
ID3D12PipelineState*		gPipeLineState = nullptr;

ID3D12Resource1*			gVertexBufferResource = nullptr;
D3D12_VERTEX_BUFFER_VIEW	gVertexBufferView = {};

#pragma endregion

#pragma region ConstantBufferGlobals
struct ConstantBuffer
{
	float values[4];
};

ID3D12DescriptorHeap*	gDescriptorHeap[NUM_SWAP_BUFFERS] = {};
ID3D12Resource1*		gConstantBufferResource[NUM_SWAP_BUFFERS] = {};
//ConstantBuffer			gConstantBufferCPU								= {};
#pragma endregion


#pragma region GameState

// TOTAL_TRIS pretty much decides how many drawcalls in a brute force approach.
constexpr int TOTAL_TRIS = 4000;
// this has to do with how the triangles are spread in the screen, not important.
constexpr int TOTAL_PLACES = 2 * TOTAL_TRIS * 20;
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