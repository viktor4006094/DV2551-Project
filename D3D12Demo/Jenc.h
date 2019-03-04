//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef __jenc_h__
#define __jenc_h__

#include "JEncCommon.h"

//removed dll stuff
// moved .cpp function from JEncMain.cpp to here

//extern "C"
//{
	class JEnc
	{
	public:
		virtual ~JEnc() {}

		virtual JEncResult Encode(JEncRGBDataDesc rgbDataDesc, int quality) = 0;

		virtual JEncResult Encode(JEncD3DDataDesc d3dDataDesc, int quality) = 0;
	};

	JEnc* CreateJpegEncoderInstance(JENC_TYPE encoderType, JENC_CHROMA_SUBSAMPLE subsampleType,
		struct ID3D11Device* d3dDevice, struct ID3D11DeviceContext* d3dContext);
//}
#endif