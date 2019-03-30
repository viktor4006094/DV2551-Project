#define AMBIENT 0.2

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

float4 PS_main( VSOut input ) : SV_TARGET0
{
    float4 output  = mul(float4(input.normal.xyz, 0.0), mul(worldMat, viewprojMat));
	float3 normOut = normalize(output.xyz);

    float3 strongLightDir = float3(-250.0,  450.0, -450.0);
	float3 weakLightDir   = float3(   0.0, -500.0,    0.0);
    float strongLightBrightness = 0.8;
    float weakLightBrightness   = 0.2;

    float diff_coef = max(dot(normalize(strongLightDir), normOut), 0.0);
    float3 diff_comp = (1.0 - AMBIENT) * diff_coef * strongLightBrightness * input.color.rgb;

    diff_coef = max(dot(normalize(weakLightDir), normOut), 0.0);
    diff_comp += (1.0 - AMBIENT) * diff_coef * weakLightBrightness * input.color.rgb;

    float3 amb_comp = AMBIENT * input.color.rgb;

    float3 finalColor = saturate(diff_comp + amb_comp);

	return float4(finalColor, 1.0);
}