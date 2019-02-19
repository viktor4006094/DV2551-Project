struct VSIn
{
	float4 pos		: POSITION;
	//float3 color	: COLOR;
};

struct VSOut
{
	float4 pos		: SV_POSITION;
	//float4 color	: COLOR;
};

cbuffer Translation : register(b0)
{
	float4 translate;
};

VSOut VS_main( VSIn input, uint index : SV_VertexID )
{
	VSOut output = (VSOut)0;
	output.pos = input.pos +translate;
	output.pos.z += 0.55; // move them backwards so that they're in front of the camera
	return output;
}