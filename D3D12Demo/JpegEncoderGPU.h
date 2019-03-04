//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "JpegEncoderBase.h"

#include "ComputeShader.h"

class JpegEncoderGPU : public JpegEncoderBase
{
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
	ID3D11Device*				mD3DDevice;
	ID3D11DeviceContext*		mD3DDeviceContext;

	//Constant buffers holding image and compute information
	ID3D11Buffer*				mCB_ImageData_Y;
	ID3D11Buffer*				mCB_ImageData_CbCr;

	//Compute shaders for each ycbcr component
	ComputeWrap*				mComputeSys;
	ComputeShader*				mShader_Y_Component;
	ComputeShader*				mShader_Cb_Component;
	ComputeShader*				mShader_Cr_Component;

	//Output UAV
	ComputeBuffer*				mCB_EntropyResult;

	//Huffman tables, structured buffers
	ComputeBuffer*				mCB_Huff_Y_AC;
	ComputeBuffer*				mCB_Huff_CbCr_AC;

	//DCT and Quantization data, one for Y and one for CbCr, structured buffers
	ComputeBuffer*				mCB_DCT_Matrix;
	ComputeBuffer*				mCB_DCT_Matrix_Transpose;
	ComputeBuffer*				mCB_Y_Quantization_Table;
	ComputeBuffer*				mCB_CbCr_Quantization_Table;

	//sampler state used to repeat border pixels
	ID3D11SamplerState*			mCB_SamplerState_PointClamp;

	//Texture used if RGB data sent for encoding
	ComputeTexture*				mCT_RGBA;

	TCHAR						mComputeShaderFile[4096];

	bool						mDoCreateBuffers;

private:
	HRESULT CreateBuffers();
	virtual void ComputationDimensionsChanged();

	virtual void QuantizationTablesChanged();

	float Y_Quantization_Table_Float[64];
	float CbCr_Quantization_Table_Float[64];

	int CalculateBufferSize(int quality);


public:
	JpegEncoderGPU(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext);
	virtual ~JpegEncoderGPU();

	virtual bool Init() = 0;

protected:
	void ReleaseBuffers();
	void ReleaseQuantizationBuffers();
	void ReleaseShaders();

	void DoHuffmanEncoding(int* DU, short& prevDC, BitString* HTDC);

	virtual void WriteImageData(JEncRGBDataDesc rgbDataDesc);
	virtual void WriteImageData(JEncD3DDataDesc d3dDataDesc);

	void DoQuantization(ID3D11ShaderResourceView* pSRV);

	virtual void Dispatch();

	virtual void DoEntropyEncode() = 0;

	void FinalizeData();
};