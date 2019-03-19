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
	//float4 translate;
	float4 color;
};

float4 PS_main( VSOut input ) : SV_TARGET0
{
	float4 output = mul(float4(input.normal.xyz,0.0f),mul(worldMat,viewprojMat));
	float3 normOut = normalize(output.xyz);
	float b = input.normal.z;
	//float4 output = float4((input.normal.xyz*input.normal.xyz*100),1.0f);
	//float4 output = float4(0.0,0.0,b,1.0f);
	//return color;
	float3 finalColor = color * float4(1.0f, 1.0f, 1.0f,1.0f);
	finalColor += saturate(dot(normalize(float3(250.0f, 450.0f, -450.0f)), normOut)*color.xyz);
	//return float4((input.pos.xy)/256.0,0.0, 1.0);
	//return float4(1.0, 1.0, 1.0, 1.0);
	//return float4(input.normal.xyz, 1.0f);
	return float4(finalColor, 1.0f);


}