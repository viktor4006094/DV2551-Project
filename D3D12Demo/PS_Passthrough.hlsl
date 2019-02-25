//Texture2D InputTexture : register(t0);

struct PS_IN
{
	float4 PositionCS	: SV_Position;
};

float4 PS_main(PS_IN input) : SV_TARGET
{
	//return float4(InputTexture.Load(int3(input.PositionCS.xy, 0)));
	return float4(1.0,0.0,0.0,1.0);
}