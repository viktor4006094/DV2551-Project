struct VSOut
{
	float4 pos		: SV_POSITION;
	float4 color	: COLOR;
};

cbuffer Color : register(b1)
{
	float4 color;
}

float4 PS_main( VSOut input ) : SV_TARGET0
{
	return color;
}