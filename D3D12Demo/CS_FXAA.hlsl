/**
* Course: DV1542 - 3D-Programming
* Authors: Viktor Enfeldt, Peter Meunier
*
* File: FXAACompute.hlsl
*
* File summary:
*	A hlsl version of Simon Rodriguez' OpenGL FXAA algorithm.
*	http://blog.simonrodriguez.fr/articles/30-07-2016_implementing_fxaa.html
*/

Texture2D<float4>   inputTex	: register(t0);
RWTexture2D<float4> outputTex	: register(u0);
SamplerState samp				: register(s0);

//Used for syntax highlighting
#if defined(__INTELLISENSE__)
#define TEXTURE_WIDTH	1.0f
#define TEXTURE_HEIGHT	1.0f
#endif


#ifdef TEXTURE_WIDTH
#ifdef TEXTURE_HEIGHT
groupshared float2 inverseScreenSize = float2((1.0 / TEXTURE_WIDTH), (1.0 / TEXTURE_HEIGHT));
#else
groupshared float2 inverseScreenSize = float2(-1.0, -1.0);
#endif
#else
groupshared float2 inverseScreenSize = float2(-1.0, -1.0);
#endif

#define EDGE_THRESHOLD_MIN 0.0312
#define EDGE_THRESHOLD_MAX 0.125
#define ITERATIONS 7
#define SUBPIXEL_QUALITY 0.75

groupshared static float QUALITY[7] = { 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };



float rgb2luma(float3 rgb)
{
	return sqrt(dot(rgb, float3(0.299, 0.587, 0.114)));
}


[numthreads(40, 20, 1)]
void FXAA_main(
	uint3	dispaThreadID	: SV_DispatchThreadID,	// Global position
	uint3	groupThreadID : SV_GroupThreadID,		// Group position
	uint	groupInDRx : SV_GroupIndex,
	uint3   groupID : SV_GroupID)
{
	float texWidth;
	float texHeight;
	float2 orig_uv = (dispaThreadID.xy + float2(0.5, 0.5)) * inverseScreenSize;

	float2 screen_pos = dispaThreadID.xy;

	GroupMemoryBarrierWithGroupSync();

	float3 colorCenter = inputTex.SampleLevel(samp, orig_uv, 0.0).rgb;
	float3 testCenter = inputTex[screen_pos].rgb;

	// Luma at the current fragment
	float lumaC = rgb2luma(colorCenter);

	// Luma at the four direct neighbours of the current fragment
	float lumaD = rgb2luma(inputTex[screen_pos + float2(0.0, 1.0)].rgb);
	float lumaU = rgb2luma(inputTex[screen_pos + float2(0.0, -1.0)].rgb);
	float lumaL = rgb2luma(inputTex[screen_pos + float2(-1.0, 0.0)].rgb);
	float lumaR = rgb2luma(inputTex[screen_pos + float2(1.0, 0.0)].rgb);

	// Find the max and min luma around the current fragment
	float lumaMin = min(lumaC, min(min(lumaD, lumaU), min(lumaL, lumaR)));
	float lumaMax = max(lumaC, max(max(lumaD, lumaU), max(lumaL, lumaR)));

	// Compare the difference
	float lumaRange = lumaMax - lumaMin;

	float4 result = float4(1.0, 1.0, 1.0, 1.0);

	// Early exit if no edge is detected in the area or if the area is really dark, 
	// or if the TEXTURE_WIDHT/HEIGHT macros have not gotten to the shader
	if (lumaRange < max(EDGE_THRESHOLD_MIN, (lumaMax * EDGE_THRESHOLD_MAX)) || inverseScreenSize.x == -1.0) {
		result = float4(colorCenter, 1.0);
	} else {
		// The four remaining corners' lumas
		float lumaDR = rgb2luma(inputTex[screen_pos + float2(1.0, 1.0)].rgb);
		float lumaDL = rgb2luma(inputTex[screen_pos + float2(-1.0, 1.0)].rgb);
		float lumaUR = rgb2luma(inputTex[screen_pos + float2(1.0, -1.0)].rgb);
		float lumaUL = rgb2luma(inputTex[screen_pos + float2(-1.0, -1.0)].rgb);

		// Combine the four edges' lumas
		float lumaDU = lumaD + lumaU;
		float lumaLR = lumaL + lumaR;

		// The same is done for the corners
		float lumaRC = lumaUL + lumaDL; // Right Corner
		float lumaLC = lumaUR + lumaDR; // Left Corner
		float lumaDC = lumaDR + lumaDL; // Down Corner
		float lumaUC = lumaUR + lumaUL; // Up Corner

		// Compute an estimation of the gradient along the horizontal and vertical axiz
		float edgeHor = abs(-2.0 * lumaL + lumaLC) + abs(-2.0 * lumaC + lumaDU) * 2.0 + abs(-2.0 * lumaR + lumaRC);
		float edgeVer = abs(-2.0 * lumaU + lumaUC) + abs(-2.0 * lumaC + lumaLR) + 2.0 + abs(-2.0 * lumaD + lumaDC);

		bool isHorizontal = (edgeHor >= edgeVer);

		// Select the two neighbouring texels' lumas in the opposite direction of the local edge
		float luma1 = isHorizontal ? lumaD : lumaL;
		float luma2 = isHorizontal ? lumaU : lumaR;
		// Compute gradients in this direction
		float grad1 = luma1 - lumaC;
		float grad2 = luma2 - lumaC;

		// Which directinon is the steepest?
		bool is1Steepest = abs(grad1) >= abs(grad2);

		// Normalized gradient in the corresponding direction
		float gradScaled = 0.25 * max(abs(grad1), abs(grad2));

		// Choose the step size (one pixel) according to the edge direction
		float stepLength = isHorizontal ? inverseScreenSize.y : inverseScreenSize.x;

		// average luma in the correct direction
		float lumaLocalAverage = 0.0;

		if (is1Steepest) {
			// switch the direction
			stepLength = -stepLength;
			lumaLocalAverage = 0.5 * (luma1 + lumaC);
		} else {
			lumaLocalAverage = 0.5 * (luma2 + lumaC);
		}

		// Shift the texel coordinate by half a pixel. After this currentUV will be located between two pixels
		float2 currentUV = orig_uv;
		if (isHorizontal) {
			currentUV.y += stepLength * 0.5;
		} else {
			currentUV.x += stepLength * 0.5;
		}

		// ---------------------------- First iteration. One step in both directions --------------------------

		// Compute offset (for each iteration step) in the right directio
		float2 offset = isHorizontal ? float2(inverseScreenSize.x, 0.0) : float2(0.0, inverseScreenSize.y);
		// Compute UVs to explore each side of the edge.
		float2 uv1 = currentUV - offset;
		float2 uv2 = currentUV + offset;

		// Read the lumas at both current extremities of the exploration segment, and compute the delta to the local average luma
		float lumaEnd1 = rgb2luma(inputTex.SampleLevel(samp, uv1, 0.0).rgb);
		float lumaEnd2 = rgb2luma(inputTex.SampleLevel(samp, uv2, 0.0).rgb);
		lumaEnd1 -= lumaLocalAverage;
		lumaEnd2 -= lumaLocalAverage;

		// If the luma deltas at the current extremities are larger than the local gradient, then the side of the edge has been reached
		bool reached1 = abs(lumaEnd1) >= gradScaled;
		bool reached2 = abs(lumaEnd2) >= gradScaled;

		// if the side is not reached then we continue to itearate in that direction
		if (!reached1) {
			uv1 -= offset;
		}
		if (!reached2) {
			uv2 += offset;
		}

		// If both sides have not been reached 
		if (!(reached1 && reached2)) {
			for (int i = 2; i < ITERATIONS; i++) {
				// If needed, read luma in 1st direction and compute delta.
				if (!reached1) {
					lumaEnd1 = rgb2luma(inputTex.SampleLevel(samp, uv1, 0.0).rgb);
					lumaEnd1 -= lumaLocalAverage;
				}

				// If needed, read luma in 2nd direction and compute delta.
				if (!reached1) {
					lumaEnd2 = rgb2luma(inputTex.SampleLevel(samp, uv2, 0.0).rgb);
					lumaEnd2 -= lumaLocalAverage;
				}

				// If the luma deltas at the current extremities are larger than the local gradient, 
				// then the side of the edge has been reached
				reached1 = abs(lumaEnd1) >= gradScaled;
				reached2 = abs(lumaEnd2) >= gradScaled;

				if (!reached1) {
					uv1 -= offset * QUALITY[i];
				}
				if (!reached2)
				{
					uv2 += offset * QUALITY[i];
				}
				// Stop when the edge has been found in both directions
				if (reached1 && reached2) { break; }
			} // end for ITERATIONS
		} // end if !reachedboth

		// Compute the distances to each extremity of the edge.
		float dist1 = isHorizontal ? (orig_uv.x - uv1.x) : (orig_uv.y - uv1.y);
		float dist2 = isHorizontal ? (uv2.x - orig_uv.x) : (uv2.y - orig_uv.y);

		// In which direction is the extremity of the edge closer?
		bool isDir1 = dist1 < dist2;
		float finalDist = min(dist1, dist2);

		float edgeLength = dist1 + dist2;

		// UV offset: read in the direction of the closest side of the edge
		float pixelOffset = -finalDist / edgeLength + 0.5;

		// is the luma at center smaller than the local average?
		bool isLumaCSmaller = lumaC < lumaLocalAverage;

		// If the luma at center is smaller than at its neighbour, the delta luma at each end should be positive
		bool correctVariation = (((isDir1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCSmaller);

		// Do not offset if the luma variation is incorrect
		float finalOffset = correctVariation ? pixelOffset : 0.0;

		// Sub-pixel shifting
		// Full weighted average of the luma over the 3x3 neighborhood
		float lumaAvg = (1.0 / 12.0) * (2.0 * (lumaDU + lumaLR) + lumaLC + lumaRC);

		// Ratio of the delta between the global average and the center luma, over the luma range in the 3x3 neighbourhood
		float subPixelOffset1 = saturate(abs(lumaAvg - lumaC) / lumaRange);
		float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
		// Compute a sub-pixel offset based on this delta
		float finalSubPixelOffset = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;

		// Pick the biggest of the two offsets
		finalOffset = max(finalOffset, finalSubPixelOffset);

		float2 finalUV = orig_uv;
		// Compute the final UV coordinates
		if (isHorizontal) {
			finalUV.y += finalOffset * stepLength;
		} else {
			finalUV.x += finalOffset * stepLength;
		}

		result = inputTex.SampleLevel(samp, finalUV, 0.0);

		// FOR DEBUGGING. Highlights detected edges
		//result = float4(1.0, 0.0, 0.0, 0.0);
	}

	//Flat color
	//float4 result = float4(1.0f, 0.0f, 0.0f, 1.0f);

	//Copy image
	//result = inputTex[screen_pos];
	//float4 result = image_square[groupThreadID.x][groupThreadID.y];

	//GroupID
	//result = float4((groupID % 5) /4.0, 1.0);

	//Thread ID
	//float4 result = float4((screen_pos.x / 1920.0f), (screen_pos.y / 1080.0f), 0.0f, 1.0f);

	outputTex[screen_pos] = result;
}