struct VSIn
{
	float4 pos		: POSITION;
	float4 normal	:NORMAL;
	//float3 color	: COLOR;
};

struct VSOut
{
	float4 pos		: SV_POSITION;
	float4 normal	: GNORMAL;
	float4 color	: COLOR; //for stress testing
};

cbuffer Translation : register(b0)
{
	float4x4 worldMat;
	float4x4 viewprojMat;
	//float4 translate;
	float4 color;
};

VSOut VS_main( VSIn input, uint index : SV_VertexID )
{
	VSOut output = (VSOut)0;

	float4 test = { 0.5, 0.5, 0.5, 0.5 };

	//// for stress testing ////
	//! increasing i by too much will hang the GPU, try to avoid doing that
	//for (int i = 0; i < 4000000; i++) {
	//	test.x = sin(i);
	//	test.y = test.x + cos(i*i);
	//}
	//// End for stress testing ////

	output.color = color;// test;

	output.pos = mul(input.pos,mul(worldMat,viewprojMat));
	output.normal = input.normal;
	//output.pos = input.pos +translate;
	//output.pos.z += 0.55; // move them backwards so that they're in front of the camera
	return output;
}