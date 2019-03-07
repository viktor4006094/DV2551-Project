struct VSOut
{
	float4 pos		: SV_POSITION;
	float4 color	: COLOR;
};

cbuffer Translation : register(b0)
{
	float4 translate;
	float4 color;
};

float4 PS_main( VSOut input ) : SV_TARGET0
{
	return color;
}