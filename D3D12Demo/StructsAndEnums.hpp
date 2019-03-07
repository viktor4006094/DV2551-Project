#pragma once

#include <d3d12.h>
#include "ConstantsAndGlobals.hpp"

#include <string>

template<class Interface>
inline void SafeRelease(Interface **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL) {
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

//Helper function for resource transitions
inline void SetResourceTransitionBarrier(
	ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES StateBefore,
	D3D12_RESOURCE_STATES StateAfter)
{
	D3D12_RESOURCE_BARRIER barrierDesc = {};

	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = resource;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = StateBefore;
	barrierDesc.Transition.StateAfter = StateAfter;

	commandList->ResourceBarrier(1, &barrierDesc);
}


inline void CountFPS(HWND wndHandle)
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



#pragma region Enums
enum QueueType : size_t {
	QUEUE_TYPE_DIRECT = 0,
	QUEUE_TYPE_COPY = 1,
	QUEUE_TYPE_COMPUTE = 2
};
#pragma endregion

//todo move some of these to their classes
#pragma region HelperStructs

struct Vertex
{
	float x, y, z, w; // Position
	//float r,g,b; // Color
};

struct float4
{
	float data[4];
};


struct alignas(256) CONSTANT_BUFFER_DATA {
	float4 position;
	float4 color;
};


struct CommandQueueAndFence
{
	ID3D12CommandQueue* mQueue = nullptr;
	ID3D12Fence1*		mFence = nullptr;
	HANDLE				mEventHandle = nullptr;
	UINT64				mFenceValue = 0;

	void CreateFenceAndEventHandle(D3D12DevPtr dev)
	{
		dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
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
	D3D12GraphicsCommandListPtr mCommandList = nullptr;

	UINT64 mLastFrameWithThisAllocatorFenceValue = 0;
	HANDLE				mEventHandle = nullptr;

	void CreateCommandListAndAllocator(QueueType type, D3D12DevPtr dev)
	{
		D3D12_COMMAND_LIST_TYPE listType = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (type == QUEUE_TYPE_COPY) { listType = D3D12_COMMAND_LIST_TYPE_COPY; }
		if (type == QUEUE_TYPE_COMPUTE) { listType = D3D12_COMMAND_LIST_TYPE_COMPUTE; }

		dev->CreateCommandAllocator(
			listType,
			IID_PPV_ARGS(&mAllocator));

		//Create command list.
		dev->CreateCommandList(
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
