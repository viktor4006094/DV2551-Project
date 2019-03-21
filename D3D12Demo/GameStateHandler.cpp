#include "GameStateHandler.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

GameStateHandler::GameStateHandler()
{

}

GameStateHandler::~GameStateHandler()
{

}

void GameStateHandler::CreatePerMeshData()
{
	for (int i = 0; i < TOTAL_DRAGONS; ++i) {
		switch (i) {
		case 0:
			cbData[i].color = float4{ 0.2f, 0.2f, 1.0f, 1.0f };
			break;
		case 1:
			cbData[i].color = float4{ 1.0f, 1.0f, 1.0f, 1.0f };
			break;
		case 2:
		case 3:
			cbData[i].color = float4{ 1.0f, 0.2f, 0.2f, 1.0f };
			break;
		default:
			cbData[i].color = float4{ 1.0f, 1.0f, 1.0f, 1.0f };
			break;
		}

		DirectX::XMStoreFloat4x4(
			&cbData[i].world,
			DirectX::XMMatrixTranspose(
				DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f)* DirectX::XMMatrixTranslation((i / 2) * 30.0f - 15.0f, (i % 2) * 24.0f - 12.0f, 0.0f)
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

// todo remove currentFrameIndex, use only one constantbuffer on the gpu
void GameStateHandler::Update(int id, UINT* currentFrameIndex)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	static double tickRate = 512.0;

	while (isRunning) {
		double t = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count();
		if (t >= 1000.0 / tickRate) {
			int meshInd = 0;
			static long long shift = 0;
			static double dshift = 0.0;
			static double delta = 0.0;

			for (int m = 0; m < TOTAL_DRAGONS; m++) {
				//Update world matrixes of each mesh
				DirectX::XMMATRIX temp = DirectX::XMLoadFloat4x4(&cbData[m].world)*DirectX::XMMatrixRotationY(-1.0 / (tickRate * 5.0));
				//DirectX::XMMATRIX temp = DirectX::XMLoadFloat4x4(&cbData[m].world)*DirectX::XMMatrixRotationY(-delta / 5000.0);

				DirectX::XMStoreFloat4x4(&cbData[m].world, temp);
				DirectX::XMStoreFloat4x4(&cbData[m].viewProj, DirectX::XMMatrixTranspose(viewMat*projMat));;

				meshInd++;
			}

			shift += max((long long)(TOTAL_DRAGONS / 1000.0), (long long)(TOTAL_DRAGONS / 100.0));
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
