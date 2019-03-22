#pragma once

#include <d3d12.h>
#include "ConstantsAndGlobals.hpp"
#include <DirectXMath.h>
#include <d3dtypes.h>
#include <string>

#pragma region HelperFunctions

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
	static UINT64 frames = 0;
	auto currentTime = std::chrono::high_resolution_clock::now();
	InterlockedIncrement(&frames);

	std::chrono::duration<double, std::milli> delta = currentTime - lastTime;
	if (delta >= std::chrono::duration<double, std::milli>(250)) {
		lastTime = std::chrono::high_resolution_clock::now();
		UINT64 fps = frames * 4;
		std::string windowText = "FPS " + std::to_string(fps);
		SetWindowTextA(wndHandle, windowText.c_str());
		frames = 0;
	}
}

#pragma endregion


#pragma region Enums
enum QueueType : size_t {
	QT_DIR = 0,
	QT_COMP = 1
};

enum InFrameStage : size_t {
	GEOMETRY_STAGE = 0,
	FXAA_STAGE = 1,
	PRESENT_STAGE = 2
};

#pragma endregion

//todo move some of these to their classes
#pragma region HelperStructs

typedef union {
	struct { float x, y, z, w; };
	struct { float r, g, b, a; };
} float4;

struct alignas(256) CONSTANT_BUFFER_DATA {
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 viewProj;
	float4 color;
};

const unsigned long long gConstBufferStructSize = sizeof(CONSTANT_BUFFER_DATA);

struct CommandAllocatorAndList
{
	//ID3D12CommandAllocator*		mAllocator = nullptr;
	//D3D12GraphicsCommandListPtr mCommandList = nullptr;
	/*D3D12GraphicsCommandListPtr mListArray[LISTS_PER_FRAME] = { nullptr };
	ID3D12CommandAllocator*		mListAllocators[LISTS_PER_FRAME] = { nullptr };*/

	D3D12GraphicsCommandListPtr* mCommandLists;
	ID3D12CommandAllocator**     mAllocators;
	int mListSize = 1;

	void CreateCommandListAndAllocator(QueueType type, D3D12DevPtr dev, int amount = 1)
	{
		HRESULT hr      = S_OK;
		mCommandLists   = new D3D12GraphicsCommandListPtr[amount];
		mAllocators     = new ID3D12CommandAllocator*[amount];
		mListSize       = amount;

		D3D12_COMMAND_LIST_TYPE listType = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (type == QT_COMP) { listType = D3D12_COMMAND_LIST_TYPE_COMPUTE; }

		/*HRESULT hr = dev->CreateCommandAllocator(listType, IID_PPV_ARGS(&mAllocator));*/



		//Create command list.
	/*	hr = dev->CreateCommandList(
			0,
			listType,
			mAllocator,
			nullptr,
			IID_PPV_ARGS(&mCommandList));*/

		//Command lists are created in the recording state. Since there is nothing to
		//record right now and the main loop expects it to be closed, we close it.
		//mCommandList->Close();

		for (int i = 0; i < mListSize; ++i) {
			hr = dev->CreateCommandAllocator(listType, IID_PPV_ARGS(&mAllocators[i]));

			hr = dev->CreateCommandList(
				0,
				listType,
				mAllocators[i],
				nullptr,
				IID_PPV_ARGS(&mCommandLists[i]));

			mCommandLists[i]->Close();
		}


#ifdef _DEBUG
		//HRESULT hr = S_OK;
		LPCWSTR name[] = {(type == QT_COMP) ? L"Compute Allocator" : L"Direct Allocator"};
		for (int i = 0; i < mListSize; ++i) {
			hr = mAllocators[i]->SetName(*name);
		}
#endif

	}

	// todo release new allocators and lists
	void Release()
	{
		//SafeRelease(&mAllocator);
		//SafeRelease(&mCommandList);

		for (int i = 0; i < mListSize; ++i) {
			SafeRelease(&mAllocators[i]);
			SafeRelease(&mCommandLists[i]);
		}

		delete[] mCommandLists;
		mCommandLists= NULL;

		delete[] mAllocators;
		mAllocators = NULL;
	}
};


struct PerFrame
{
	CommandAllocatorAndList mAllocatorsAndLists[NUM_STAGES_IN_FRAME];
	ID3D12Resource1*		gIntermediateRenderTarget = nullptr;
	ID3D12Resource1*		gUAVResource = nullptr;


	void Release()
	{
		for (int i = 0; i < NUM_STAGES_IN_FRAME; ++i) {
			mAllocatorsAndLists[i].Release();
		}
		gIntermediateRenderTarget->Release();
		gUAVResource->Release();
	}

	void CreateCommandListsAndAllocators(D3D12DevPtr dev)
	{
		mAllocatorsAndLists[GEOMETRY_STAGE].CreateCommandListAndAllocator(QT_DIR, dev, LISTS_PER_FRAME);
		mAllocatorsAndLists[FXAA_STAGE].CreateCommandListAndAllocator(QT_COMP, dev);
		mAllocatorsAndLists[PRESENT_STAGE].CreateCommandListAndAllocator(QT_DIR, dev);
	}
};
#pragma endregion
