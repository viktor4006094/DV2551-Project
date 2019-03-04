//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
// being rewritten fro 444 encoding

#pragma once

#include "JpegEncoderBase.h"

#include "ComputeShader.h"

//todo rename to something more fitting since the GPU stuff is 
//todo mostly done in JPEGStage
class JpegEncoderGPU : public JpegEncoderBase
{
	friend class JPEGStage;
protected:

	int mEntropyBlockSize;

	struct ImageData
	{
		float	ImageWidth;
		float	ImageHeight;

		UINT	NumBlocksX;
		UINT	NumBlocksY;

		int		EntropyBlockSize;
	};

	//D3D
	//ID3D11Device*				mD3DDevice;
	//ID3D11DeviceContext*		mD3DDeviceContext;

	//Constant buffers holding image and compute information
	//ID3D11Buffer*				mCB_ImageData_Y;
	//ID3D11Buffer*				mCB_ImageData_CbCr;

	//Compute shaders for each ycbcr component
	//ComputeWrap*				mComputeSys;
	//ComputeShader*				mShader_Y_Component;
	//ComputeShader*				mShader_Cb_Component;
	//ComputeShader*				mShader_Cr_Component;

	//Output UAV
	//ComputeBuffer*				mCB_EntropyResult;

	//Huffman tables, structured buffers
	//ComputeBuffer*				mCB_Huff_Y_AC;
	//ComputeBuffer*				mCB_Huff_CbCr_AC;

	//DCT and Quantization data, one for Y and one for CbCr, structured buffers
	//ComputeBuffer*				mCB_DCT_Matrix;
	//ComputeBuffer*				mCB_DCT_Matrix_Transpose;
	//ComputeBuffer*				mCB_Y_Quantization_Table;
	//ComputeBuffer*				mCB_CbCr_Quantization_Table;

	//sampler state used to repeat border pixels
	//ID3D11SamplerState*			mCB_SamplerState_PointClamp;

	//Texture used if RGB data sent for encoding
	//ComputeTexture*				mCT_RGBA;

	//TCHAR						mComputeShaderFile[4096];

	bool						mDoCreateBuffers;

private:
	//HRESULT CreateBuffers();
	void ComputationDimensionsChanged();

	void QuantizationTablesChanged();

	float Y_Quantization_Table_Float[64];
	float CbCr_Quantization_Table_Float[64];

	int CalculateBufferSize(int quality);


public:
	JpegEncoderGPU();
	virtual ~JpegEncoderGPU();

	// done in JPEGStage
	//bool Init();

protected:
	void ReleaseBuffers();
	void ReleaseQuantizationBuffers();
	void ReleaseShaders();

	void DoHuffmanEncoding(int* DU, short& prevDC, BitString* HTDC);
	
	//! Moved to JPEGStage
	void WriteImageData(JEncRGBDataDesc rgbDataDesc) {}
	void WriteImageData(JEncD3DDataDesc d3dDataDesc) {}

	//void DoQuantization(ID3D11ShaderResourceView* pSRV);

	//void Dispatch();

	void DoEntropyEncode();

	void FinalizeData();
};