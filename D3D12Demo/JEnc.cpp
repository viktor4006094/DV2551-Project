
//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "Jenc.h"

#include "JpegEncoderGPU_444.h"
//#include "Encoder\JpegEncoderGPU_422.h"
//#include "Encoder\JpegEncoderGPU_420.h"

#include "stdafx.h"


JEnc* CreateJpegEncoderInstance(JENC_TYPE encoderType, JENC_CHROMA_SUBSAMPLE subsampleType,
	struct ID3D11Device* d3dDevice, struct ID3D11DeviceContext* d3dContext)
{
	JpegEncoderBase* enc = NULL;

	if (encoderType == GPU_ENCODER)
	{
		if (subsampleType == JENC_CHROMA_SUBSAMPLE_4_4_4)
			enc = myNew JpegEncoderGPU_444(d3dDevice, d3dContext);
		//else if (subsampleType == JENC_CHROMA_SUBSAMPLE_4_2_2)
		//	enc = myNew JpegEncoderGPU_422(d3dDevice, d3dContext);
		//else if (subsampleType == JENC_CHROMA_SUBSAMPLE_4_2_0)
		//	enc = myNew JpegEncoderGPU_420(d3dDevice, d3dContext);
	}

	if (enc)
	{
		if (!enc->Init())
		{
			delete enc;
			enc = NULL;
		}
	}

	return enc;
}