Texture2D<float4>   inputTex  : register(t0);
RWTexture2D<float4> outputTex : register(u0);


//Used for syntax highlighting
#if defined(__INTELLISENSE__)
//#define TEXTURE_WIDTH	1.0f
//#define TEXTURE_HEIGHT	1.0f
#endif


[numthreads(40, 20, 1)]
void CS_main(
	uint3	dispaThreadID	: SV_DispatchThreadID,	// Global position
	uint3	groupThreadID : SV_GroupThreadID,		// Group position
	uint	groupInDRx : SV_GroupIndex,
	uint3   groupID : SV_GroupID)
{
	//float texWidth;
	//float texHeight;
	//float2 orig_uv = (dispaThreadID.xy + float2(0.5, 0.5)) * inverseScreenSize;

	float2 screen_pos = dispaThreadID.xy;

	//GroupMemoryBarrierWithGroupSync();

	//float3 colorCenter = inputTex.SampleLevel(samp, orig_uv, 0.0).rgb;
	float3 testCenter = inputTex[screen_pos].rgb;

	//Flat color
	//float4 result = float4(1.0f, 0.0f, 0.0f, 1.0f);

	//Copy image
	//float4 result = inputTex[screen_pos];
	//float4 result = image_square[groupThreadID.x][groupThreadID.y];

	//GroupID
	//result = float4((groupID % 5) /4.0, 1.0);

	//Thread ID
	float4 result = float4((screen_pos.x / 640.0f), (screen_pos.y / 480.0f), 0.0f, 1.0f);

	//outputTex[screen_pos] = result;
	outputTex[screen_pos] = float4(testCenter,1.0);
	//outputTex[screen_pos] = float4(1.0,0.0,0.0,1.0);

}