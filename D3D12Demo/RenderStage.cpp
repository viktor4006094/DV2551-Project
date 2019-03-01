#include "RenderStage.hpp"
#include "Project.hpp"

RenderStage::RenderStage()
{

}


RenderStage::~RenderStage()
{

}

void RenderStage::Run(int index, Project* p)
{
	static size_t lastRenderIterationIndex = 0;

	if (isRunning) {

		UINT backBufferIndex = p->gSwapChain4->GetCurrentBackBufferIndex();
		//Command list allocators can only be reset when the associated command lists have
		//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
		ID3D12CommandAllocator* directAllocator = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mAllocator;
		D3D12GraphicsCommandListPtr	directList = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mCommandList;

		ID3D12Fence1* fence = p->gCommandQueues[QUEUE_TYPE_DIRECT].mFence;
		HANDLE eventHandle = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mEventHandle;


		////! since WaitForGPU is called just before the commandlist is executed in the previous frame this does 
		////! not need to be done here since the command allocator is already guaranteed to have finished executing
		//// wait for previous usage of this command allocator to be done executing before it is reset
		//UINT64 prevFence = gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mLastFrameWithThisAllocatorFenceValue;
		//if (fence->GetCompletedValue() < prevFence) {
		//	fence->SetEventOnCompletion(prevFence, eventHandle);
		//	WaitForSingleObject(eventHandle, INFINITE);
		//}

		directAllocator->Reset();
		directList->Reset(directAllocator, p->gRenderPipeLineState);


		//Set root signature
		directList->SetGraphicsRootSignature(p->gRootSignature);

		//Set necessary states.
		directList->RSSetViewports(1, &p->gViewport);
		directList->RSSetScissorRects(1, &p->gScissorRect);

		//Indicate that the back buffer will be used as render target.
		SetResourceTransitionBarrier(directList,
			p->gRenderTargets[backBufferIndex],
			D3D12_RESOURCE_STATE_PRESENT,		//state before
			D3D12_RESOURCE_STATE_RENDER_TARGET	//state after
		);

		//Record commands.
		//Get the handle for the current render target used as back buffer.
		D3D12_CPU_DESCRIPTOR_HANDLE cdh = p->gRenderTargetsHeap->GetCPUDescriptorHandleForHeapStart();
		cdh.ptr += p->gRenderTargetDescriptorSize * backBufferIndex;

		directList->OMSetRenderTargets(1, &cdh, true, nullptr);

		float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		directList->ClearRenderTargetView(cdh, clearColor, 0, nullptr);

		directList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		directList->IASetVertexBuffers(0, 1, &p->gVertexBufferView);

		//get the current game state
	
		p->mGameStateHandler.writeNewestGameStateToReadOnlyAtIndex(index);
		/*gBufferTransferLock.lock();
		readOnlyState[index] = bufferState;
		gBufferTransferLock.unlock();*/

		//Update constant buffers and draw triangles
		//for (int i = 0; i < 100; ++i) {


		for (auto &m : p->mGameStateHandler.getReadOnlyStateAtIndex(index)->meshes) {
			directList->SetGraphicsRoot32BitConstants(0, 4, &m.translate, 0);
			directList->SetGraphicsRoot32BitConstants(1, 4, &m.color, 0);

			directList->DrawInstanced(3, 1, 0, 0);
		}
		//}

		//Indicate that the back buffer will now be used to present.
		//SetResourceTransitionBarrier(directList,
		//	gRenderTargets[backBufferIndex],
		//	D3D12_RESOURCE_STATE_RENDER_TARGET,	//state before
		//	D3D12_RESOURCE_STATE_PRESENT		//state after
		//);


		//Close the list to prepare it for execution.
		directList->Close();

		//wait for current frame to finish rendering before pushing the next one to GPU
		p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

		// set the just executed command allocator and list to inactive
		//gAllocatorsAndLists[(index + MAX_THREAD_COUNT - 1) % MAX_THREAD_COUNT][QUEUE_TYPE_DIRECT].isActive = false;

		//Execute the command list.
		ID3D12CommandList* listsToExecute[] = { directList };
		p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);


		//// Execute Compute shader
		//ComputePass(index);


		//// Passthrough
		//PassthroughPass(index);


		////Present the frame.
		//DXGI_PRESENT_PARAMETERS pp = {};
		//gSwapChain4->Present1(0, 0, &pp);


		//threadIDIndexLock.lock();
		//// Signal that the execution of this list is done
		////! SIGNAL MUST HAPPEN AFTER ExecuteCommandLists
		//{
		//	const UINT64 fenceVal = gCommandQueues[QUEUE_TYPE_DIRECT].mFenceValue;
		//	gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->Signal(fence, fenceVal);
		//	gCommandQueues[QUEUE_TYPE_DIRECT].mFenceValue++;
		//	gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mLastFrameWithThisAllocatorFenceValue = fenceVal;
		//}
		//threadIDIndexLock.unlock();
		//gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();
		//Wait for GPU to finish.
		//NOT BEST PRACTICE, only used as such for simplicity.


		//todo wait for previous render call with this render target index to finish the JPEG part

		// create new render thread
		//gThreadPool.push(Render);
		//gThreadPool->push([this](int id) {Render(id); });
		//std::thread(Render).detach();

	}



}