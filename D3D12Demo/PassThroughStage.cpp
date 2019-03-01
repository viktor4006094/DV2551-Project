#include "PassThroughStage.hpp"
#include "Project.hpp"


PassThroughStage::PassThroughStage()
{

}

PassThroughStage::~PassThroughStage()
{

}


void PassThroughStage::Run(int index, Project* p)
{
	UINT backBufferIndex = p->gSwapChain4->GetCurrentBackBufferIndex();
	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	ID3D12CommandAllocator* directAllocator = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mAllocator;
	D3D12GraphicsCommandListPtr	directList = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mCommandList;


	//// Passthrough ////

	p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	directAllocator->Reset();
	directList->Reset(directAllocator, p->gPassthroughPipeLineState);

	//Set root signature
	directList->SetGraphicsRootSignature(p->gRootSignature);

	//Set necessary states.
	directList->RSSetViewports(1, &p->gViewport);
	directList->RSSetScissorRects(1, &p->gScissorRect);

	////Indicate that the back buffer will be used as render target.
	//SetResourceTransitionBarrier(directList,
	//	gRenderTargets[backBufferIndex],
	//	D3D12_RESOURCE_STATE_PRESENT,		//state before
	//	D3D12_RESOURCE_STATE_RENDER_TARGET	//state after
	//);

	//Record commands.
	//Get the handle for the current render target used as back buffer.
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = p->gRenderTargetsHeap->GetCPUDescriptorHandleForHeapStart();
	cdh.ptr += p->gRenderTargetDescriptorSize * backBufferIndex;

	directList->OMSetRenderTargets(1, &cdh, true, nullptr);

	float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	directList->ClearRenderTargetView(cdh, clearColor, 0, nullptr);

	directList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* dheap[] = { p->gComputeDescriptorHeap };
	directList->SetDescriptorHeaps(_countof(dheap), dheap);
	directList->SetGraphicsRootDescriptorTable(2, p->gComputeDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	directList->DrawInstanced(3, 1, 0, 0);


	//Indicate that the back buffer will now be used to present.
	SetResourceTransitionBarrier(directList,
		p->gRenderTargets[backBufferIndex],
		D3D12_RESOURCE_STATE_RENDER_TARGET,	//state before
		D3D12_RESOURCE_STATE_PRESENT		//state after
	);

	SetResourceTransitionBarrier(directList,
		p->gUAVResource,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,		//state before
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS		//state after
	);

	//Close the list to prepare it for execution.
	directList->Close();

	//wait for current frame to finish rendering before pushing the next one to GPU
	p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	// set the just executed command allocator and list to inactive
	//gAllocatorsAndLists[(index + MAX_THREAD_COUNT - 1) % MAX_THREAD_COUNT][QUEUE_TYPE_DIRECT].isActive = false;

	//Execute the command list.
	ID3D12CommandList* listsToExecute3[] = { directList };
	p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute3), listsToExecute3);
}