#include "GameStateHandler.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

GameStateHandler::GameStateHandler()
{

}

GameStateHandler::~GameStateHandler()
{

}

void GameStateHandler::CreateMeshes()
{
	// create the translation arrays used for moving the triangles around on the screen
	float degToRad = (float)M_PI / 180.0f;
	float scale = (float)TOTAL_PLACES / 359.9f;
	for (int a = 0; a < TOTAL_PLACES; a++)
	{
		gXT[a] = 0.8f * cosf(degToRad * ((float)a / scale) * 3.0f);
		gYT[a] = 0.8f * sinf(degToRad * ((float)a / scale) * 2.0f);
	};

	float fade = 1.0f / TOTAL_TRIS;
	for (int i = 0; i < TOTAL_TRIS; ++i) {
		//TriangleObject m;

		// initialize meshes with greyscale colors
		float c = 1.0f - fade * i;
		cbData[i].color = float4{ c, c, c, 1.0 };

		//writeState.meshes.push_back(m);
	}

}

void GameStateHandler::ShutDown()
{
	isRunning = false;
}

void GameStateHandler::Update(int id, UINT* currentFrameIndex)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	while (isRunning) {
		int meshInd = 0;
		static long long shift = 0;
		static double dshift = 0.0;
		static double delta = 0.0;

		for (int m = 0; m < TOTAL_TRIS; m++) {
		//for (auto &m : writeState.meshes) {

			//Update color values in constant buffer
			for (int i = 0; i < 3; i++)
			{
				//gConstantBufferCPU.colorChannel[i] += 0.0001f * (i + 1);
				//m.color.values[i] += delta / 10000.0f * (i + 1);
				cbData[m].color.data[i] += delta / 10000.0f * (i + 1);
				if (cbData[m].color.data[i] > 1)
				{
					cbData[m].color.data[i] = 0;
				}
			}

			//Update positions of each mesh
			cbData[m].position = float4{
				gXT[(int)(float)(meshInd * 100 + dshift) % (TOTAL_PLACES)],
				gYT[(int)(float)(meshInd * 100 + dshift) % (TOTAL_PLACES)],
				meshInd * (-1.0f / TOTAL_PLACES),
				0.0f
			};

			meshInd++;
		}
		shift += max((long long)(TOTAL_TRIS / 1000.0), (long long)(TOTAL_TRIS / 100.0));
		delta = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count();
		dshift += delta * gMovementSpeed;
		startTime = std::chrono::high_resolution_clock::now();

		//copy updated gamestate to bufferstate
		//gBufferTransferLock.lock();

		//todo? copy it to the next frames buffer, 
		memcpy((void*)pMappedCB[*currentFrameIndex], cbData, sizeof(cbData));

		//bufferState = writeState;
		//gBufferTransferLock.unlock();
	}
}

//void GameStateHandler::writeNewestGameStateToReadOnlyAtIndex(int index)
//{
//	//gBufferTransferLock.lock();
//	////readOnlyState[index] = bufferState;
//	//gBufferTransferLock.unlock();
//}
//
//GameState* GameStateHandler::getReadOnlyStateAtIndex(int index)
//{
//	return &readOnlyState[index];
//}
