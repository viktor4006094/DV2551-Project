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
		if (i == 0||i==1||i==3) {
			cbData[i].color = float4{ 0.5f,0.48f,0.44f,1.0f };
		}
		else {
			cbData[i].color = float4{ 0.82f,0.2f,0.16f, 1.0f };
		}
		DirectX::XMStoreFloat4x4(
			&cbData[i].world,
			DirectX::XMMatrixTranspose(
				DirectX::XMMatrixScaling(2, 2, 2)* DirectX::XMMatrixTranslation((i / 2) * 30 - 15, (i % 2) * 24 - 12, 0)
			)
		);
		//cbData[i].world = DirectX::XMFLOAT4X4{ 0,0,0,0,0,0,0,0,0,0,0,0,i % 100,0,i / 100,1 };//DirectX::XMMatrixTranslation((i % 100), 0, (i / 40));
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
				//cbData[m].color.data[i] += static_cast<float>(delta / 10000.0f * (i + 1));
				//if (cbData[m].color.data[i] > 1)
				//{
				//	cbData[m].color.data[i] = 0;
				//}
			//	cbData[m].color.data[i] = m /(float)TOTAL_TRIS;
			}

			//Update positions of each mesh
			//cbData[m].position = float4{
			//	gXT[(int)(float)(meshInd * 100 + dshift) % (TOTAL_PLACES)],
			//	gYT[(int)(float)(meshInd * 100 + dshift) % (TOTAL_PLACES)],
			//	meshInd * (-1.0f / TOTAL_PLACES),
			//	0.0f
			//};

			
			//DirectX::XMStoreFloat4x4(&cbData[m].world, DirectX::XMMatrixTranslation(//m/10, m%10, 0));
			//	//gXT[(int)(float)(meshInd * 100 + dshift) % (TOTAL_PLACES)],
			//	//gYT[(int)(float)(meshInd * 100 + dshift) % (TOTAL_PLACES)],
			//	//meshInd * (-1.0f / TOTAL_PLACES)));
			DirectX::XMMATRIX temp = DirectX::XMLoadFloat4x4(&cbData[m].world)*DirectX::XMMatrixRotationY(-delta/5000.0f);

			DirectX::XMStoreFloat4x4(&cbData[m].world, temp);
			DirectX::XMStoreFloat4x4(&cbData[m].viewProj, DirectX::XMMatrixTranspose(viewMat*projMat));;

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
