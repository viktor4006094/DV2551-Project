struct VSIn
{
	float4 pos	: POSITION;
	float4 nor	: NOR;
};

struct VSOut
{
	float4 pos		: SV_POSITION;
	float4 normal	: GNORMAL;
	float4 color	: COLOR;
};

cbuffer Translation : register(b0)
{
	float4x4 worldMat;
	float4x4 viewprojMat;
	float4 color;
};

VSOut VS_main( VSIn input, uint index : SV_VertexID )
{
	VSOut output  = (VSOut)0;

    output.pos    = mul(input.pos, mul(worldMat, viewprojMat));
	output.normal = input.nor;
	output.color  = color;

	return output;
}