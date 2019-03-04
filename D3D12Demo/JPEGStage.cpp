#include "JPEGStage.hpp"
#include "Project.hpp"
#include "JpegEncoderGPU.h"

JPEGStage::JPEGStage() : mJPEGEncoder(new JpegEncoderGPU())
{

}

JPEGStage::~JPEGStage()
{
	SAFE_DELETE(mJPEGEncoder);
}

void JPEGStage::Init(D3D12DevPtr dev, ID3D12RootSignature* rootSig)
{
	////// Shader Defines //////
	D3D_SHADER_MACRO shaderDefines_Y[] = {
		{ "C_4_4_4", "1" },
		{ "COMPONENT_Y", "1" },
		{ NULL, NULL}
	};

	D3D_SHADER_MACRO shaderDefines_Cb[] = {
		{ "C_4_4_4", "1" },
		{ "COMPONENT_CB", "1" },
		{ NULL, NULL}
	};

	D3D_SHADER_MACRO shaderDefines_Cr[] = {
		{ "C_4_4_4", "1" },
		{ "COMPONENT_CR", "1" },
		{ NULL, NULL}
	};

	////// Shader Compiles //////
	ID3DBlob* pShaderBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	//// Y channel ////
	{
		HRESULT hr = D3DCompileFromFile(
			L"Jpeg_CS.hlsl",// filename
			shaderDefines_Y,// optional macros
			nullptr,		// optional include files
			"ComputeJPEG",	// entry point
			"cs_5_0",		// shader model (target)
			0,				// shader compile options			// here DEBUGGING OPTIONS
			0,				// effect compile options
			&pShaderBlob,	// double pointer to ID3DBlob		
			&pErrorBlob		// pointer for Error Blob messages.
							// how to use the Error blob, see here
							// https://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
		);

		if (pErrorBlob) {
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		}

		if (SUCCEEDED(hr)) {
			////// Pipline State //////
			D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};

			//Specify pipeline stages:
			cpsd.pRootSignature = rootSig;
			cpsd.CS.pShaderBytecode = reinterpret_cast<void*>(pShaderBlob->GetBufferPointer());
			cpsd.CS.BytecodeLength = pShaderBlob->GetBufferSize();

			dev->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&mPipelineState));
		}

		SAFE_RELEASE(pErrorBlob);
		SAFE_RELEASE(pShaderBlob);
	}

	//// Cb channel ////
	{
		HRESULT hr = D3DCompileFromFile(
			L"Jpeg_CS.hlsl",// filename
			shaderDefines_Cb,// optional macros
			nullptr,		// optional include files
			"ComputeJPEG",	// entry point
			"cs_5_0",		// shader model (target)
			0,				// shader compile options			// here DEBUGGING OPTIONS
			0,				// effect compile options
			&pShaderBlob,	// double pointer to ID3DBlob		
			&pErrorBlob		// pointer for Error Blob messages.
							// how to use the Error blob, see here
							// https://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
		);

		if (pErrorBlob) {
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		}

		if (SUCCEEDED(hr)) {
			////// Pipline State //////
			D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};

			//Specify pipeline stages:
			cpsd.pRootSignature = rootSig;
			cpsd.CS.pShaderBytecode = reinterpret_cast<void*>(pShaderBlob->GetBufferPointer());
			cpsd.CS.BytecodeLength = pShaderBlob->GetBufferSize();

			dev->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&mCbPipelineState));
		}

		SAFE_RELEASE(pErrorBlob);
		SAFE_RELEASE(pShaderBlob);
	}

	//// Cr channel ////
	{
		HRESULT hr = D3DCompileFromFile(
			L"Jpeg_CS.hlsl",// filename
			shaderDefines_Cr,// optional macros
			nullptr,		// optional include files
			"ComputeJPEG",	// entry point
			"cs_5_0",		// shader model (target)
			0,				// shader compile options			// here DEBUGGING OPTIONS
			0,				// effect compile options
			&pShaderBlob,	// double pointer to ID3DBlob		
			&pErrorBlob		// pointer for Error Blob messages.
							// how to use the Error blob, see here
							// https://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
		);

		if (pErrorBlob) {
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		}

		if (SUCCEEDED(hr)) {
			////// Pipline State //////
			D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};

			//Specify pipeline stages:
			cpsd.pRootSignature = rootSig;
			cpsd.CS.pShaderBytecode = reinterpret_cast<void*>(pShaderBlob->GetBufferPointer());
			cpsd.CS.BytecodeLength = pShaderBlob->GetBufferSize();

			dev->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&mCrPipelineState));
		}

		SAFE_RELEASE(pErrorBlob);
		SAFE_RELEASE(pShaderBlob);
	}
}

void JPEGStage::Run(int index, Project* p)
{
	// DX11 version
	{
		//ID3D11UnorderedAccessView* aUAVViews[] = { mCB_EntropyResult->GetUnorderedAccessView() };
		//mD3DDeviceContext->CSSetUnorderedAccessViews(0, sizeof(aUAVViews) / sizeof(aUAVViews[0]), aUAVViews, NULL);


		//ID3D11ShaderResourceView* srv_Huffman_Y[] = { mCB_Y_Quantization_Table->GetResourceView(), mCB_Huff_Y_AC->GetResourceView() };
		//mD3DDeviceContext->CSSetShaderResources(3, 2, srv_Huffman_Y);

		//mD3DDeviceContext->CSSetConstantBuffers(0, 1, &mCB_ImageData_Y);

		//mShader_Y_Component->Set();
		//mD3DDeviceContext->Dispatch(mNumComputationBlocks_Y[0], mNumComputationBlocks_Y[1], 1);
		//mShader_Y_Component->Unset();



		//ID3D11ShaderResourceView* srv_Huffman_CbCr[] = { mCB_CbCr_Quantization_Table->GetResourceView(), mCB_Huff_CbCr_AC->GetResourceView() };
		//mD3DDeviceContext->CSSetShaderResources(3, 2, srv_Huffman_CbCr);

		//mD3DDeviceContext->CSSetConstantBuffers(0, 1, &mCB_ImageData_CbCr);

		//mShader_Cb_Component->Set();
		//mD3DDeviceContext->Dispatch(mNumComputationBlocks_CbCr[0], mNumComputationBlocks_CbCr[1], 1);
		//mShader_Cb_Component->Unset();

		//mShader_Cr_Component->Set();
		//mD3DDeviceContext->Dispatch(mNumComputationBlocks_CbCr[0], mNumComputationBlocks_CbCr[1], 1);
		//mShader_Cr_Component->Unset();
	}


	if (mJPEGEncoder->mDoCreateBuffers)
	{
		//mJPEGEncoder->CreateBuffers();
		mJPEGEncoder->mDoCreateBuffers = false;
	}

	//DoQuantization(d3dDataDesc.ResourceView);





	// todo get the buffer index in a way that's independant of the swapchain index since this doesn't write to the backbuffer
	UINT backBufferIndex = p->gSwapChain4->GetCurrentBackBufferIndex();
	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	// todo use Compute allocators
	ID3D12CommandAllocator* directAllocator = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mAllocator;
	D3D12GraphicsCommandListPtr directList = p->gAllocatorsAndLists[index][QUEUE_TYPE_DIRECT].mCommandList;


	// wait for the previous frame to be complete
	p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu();

	directAllocator->Reset();

	////// Y part //////
	{
		// set Y PSO
		directList->Reset(directAllocator, mPipelineState);

		//Set root signature
		directList->SetComputeRootSignature(p->gRootSignature);

		// todo read from a texture that isn't in the swapchain
		SetResourceTransitionBarrier(directList,
			p->gRenderTargets[backBufferIndex],
			D3D12_RESOURCE_STATE_RENDER_TARGET,				//state before
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE	//state after
		);


		//todo set correct descriptor heap
		//ID3D12DescriptorHeap* dheap1[] = { p->gComputeDescriptorHeap };
		// todo set UAV
		// todo set SRV HuffmanY
		// todo set Y Constant Buffers

		// Dispatch Y
		directList->Dispatch(mJPEGEncoder->mNumComputationBlocks_Y[0], mJPEGEncoder->mNumComputationBlocks_Y[1], 1);

		// Close the list and excecute the Y part
		directList->Close();
		p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu(); //todo use compute queue instead
		ID3D12CommandList* listsToExecute2[] = { directList };
		p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);
	}

	////// Cb Part //////
	{
		// set Cb PSO
		directList->Reset(directAllocator, mCbPipelineState);

		//Set root signature
		directList->SetComputeRootSignature(p->gRootSignature);

		// todo set SRV Huffman CbCr (same SRV for both it seems)
		// todo set CbCr Constant Buffers

		// Dispatch Cb
		directList->Dispatch(mJPEGEncoder->mNumComputationBlocks_CbCr[0], mJPEGEncoder->mNumComputationBlocks_CbCr[1], 1);

		// Close the list and excecute the Cb part
		directList->Close();
		p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu(); //todo use compute queue instead
		ID3D12CommandList* listsToExecute2[] = { directList };
		p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);
	}

	////// Cr Part //////
	{
		// set Cr PSO
		directList->Reset(directAllocator, mCrPipelineState);

		//Set root signature
		directList->SetComputeRootSignature(p->gRootSignature);

		// todo set SRV Huffman CbCr (same SRV for both it seems)
		// todo set CbCr Constant Buffers

		// Dispatch Cr
		directList->Dispatch(mJPEGEncoder->mNumComputationBlocks_CbCr[0], mJPEGEncoder->mNumComputationBlocks_CbCr[1], 1);

		// Transition the texture to a pixel shader resource so that it can 
		SetResourceTransitionBarrier(directList,
			p->gRenderTargets[backBufferIndex],
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,	//state before
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE		//state after
		);


		// Close the list and excecute the Cr part
		directList->Close();
		p->gCommandQueues[QUEUE_TYPE_DIRECT].WaitForGpu(); //todo use compute queue instead
		ID3D12CommandList* listsToExecute2[] = { directList };
		p->gCommandQueues[QUEUE_TYPE_DIRECT].mQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute2), listsToExecute2);
	}


	// todo Copy UAV result to RAM

	// Call CPU functions to finalize JPEG encoding
	mJPEGEncoder->DoEntropyEncode();

	mJPEGEncoder->FinalizeData();
}