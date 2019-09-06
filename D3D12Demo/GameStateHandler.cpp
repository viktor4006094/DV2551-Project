#include "GameStateHandler.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <ctime>
#include <cmath>

GameStateHandler::GameStateHandler()
{
	tempDragonData  = std::vector<PER_DRAGON_DATA>(TOTAL_DRAGONS);
	currentDragonData  = std::vector<PER_DRAGON_DATA>(TOTAL_DRAGONS);
	previousDragonData = std::vector<PER_DRAGON_DATA>(TOTAL_DRAGONS);
}

GameStateHandler::~GameStateHandler()
{

}

void GameStateHandler::CreatePerMeshData()
{
	for (int i = 0; i < 4; ++i) {
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
				DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) * DirectX::XMMatrixTranslation((i / 2) * 30.0f - 15.0f, (i % 2) * 24.0f - 12.0f, 0.0f)
			)
		);

		currentDragonData[i].position = { (i / 2) * 30.0f - 15.0f, (i % 2) * 24.0f - 12.0f, 0.0f };
		currentDragonData[i].angle = 0.0f;
		currentDragonData[i].scale = { 2.0f, 2.0f, 2.0f };

	}

	srand(static_cast<unsigned int>(std::time(0)));
	
	for (int i = 4; i < TOTAL_DRAGONS; i++) {
		float a, b, c;
		a = ((float)rand()) / (float)RAND_MAX;
		b = ((float)rand()) / (float)RAND_MAX;
		c = ((float)rand()) / (float)RAND_MAX;
		cbData[i].color = float4{ a, b, c, 1.0f };
		int j = i - 4;
		DirectX::XMStoreFloat4x4(
			&cbData[i].world,
			DirectX::XMMatrixTranspose(
				DirectX::XMMatrixScaling(0.2f, 0.2f, 0.2f)* DirectX::XMMatrixTranslation((j / 10) * 6.0f - 28.0f, (j %10) * 4.8f - 13.0f, 20.0f)
			)
		);

		currentDragonData[i].position = { (j / 10) * 6.0f - 28.0f, (j % 10) * 4.8f - 13.0f, 20.0f };
		currentDragonData[i].angle = 0.0f;
		currentDragonData[i].scale = { 0.2f, 0.2f, 0.2f };

	}
	previousDragonData = currentDragonData;
}

void GameStateHandler::ShutDown()
{
	isRunning = false;
}


void GameStateHandler::Update(int id)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	//static double tickRate = 512.0;
	static double tickRate = 10.0;

	while (isRunning) {
		double t = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count();
		if (t >= 1000.0 / tickRate) {
			int meshInd = 0;
			static long long shift = 0;
			static double dshift   = 0.0;
			static double delta    = 0.0;


			for (int m = 0; m < TOTAL_DRAGONS; m++) {
				if (m < 4) {
					//Update world matrixes of each mesh
					/*DirectX::XMMATRIX temp = DirectX::XMLoadFloat4x4(&cbData[m].world)*DirectX::XMMatrixRotationY(-1.0f / static_cast<float>(tickRate * 5.0f));
					DirectX::XMStoreFloat4x4(&cbData[m].world, temp);
					DirectX::XMStoreFloat4x4(&cbData[m].viewProj, DirectX::XMMatrixTranspose(viewMat*projMat));;*/


					//currentDragonData[m].position = { (j / 10) * 6.0f - 28.0f, (j % 10) * 4.8f - 13.0f, 20.0f };
					currentDragonData[m].angle = (-1.0f / static_cast<float>(tickRate * 5.0f));
					//currentDragonData[m].scale = { 0.2f, 0.2f, 0.2f };
				}
				else {
					int colInd = (m - 4) / 10;
					int dir=1;
					if (colInd % 3 < 1) {
						dir = -1;
					}
					float speed = ((colInd % 4) + 1) * 1.0f;


					/*DirectX::XMMATRIX temp = DirectX::XMLoadFloat4x4(&cbData[m].world)*DirectX::XMMatrixRotationY(dir / static_cast<float>(tickRate * speed));
					DirectX::XMStoreFloat4x4(&cbData[m].world, temp);
					DirectX::XMStoreFloat4x4(&cbData[m].viewProj, DirectX::XMMatrixTranspose(viewMat*projMat));*/

					//currentDragonData[i].position = { (j / 10) * 6.0f - 28.0f, (j % 10) * 4.8f - 13.0f, 20.0f };
					currentDragonData[m].angle = (dir / static_cast<float>(tickRate * speed));
					//currentDragonData[i].scale = { 0.2f, 0.2f, 0.2f };
				}

				meshInd++;
			}

			shift += max((long long)(TOTAL_DRAGONS / 1000.0), (long long)(TOTAL_DRAGONS / 100.0));
			delta = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count();
			dshift += delta * gMovementSpeed;
			startTime = std::chrono::high_resolution_clock::now();

			////copy updated gamestate to mapped memory so that the GPU can access it when needed
			//memcpy((void*)pMappedCB, cbData, sizeof(cbData));


			dragonDataLock.lock();
			previousDragonData = currentDragonData;
			currentDragonData = tempDragonData;
			dragonDataLock.unlock();


			// FOR TESTING
			// no interpolation
			PrepareRender(0.0f);
		}
	}
}


// alpha = [0,1) linear interpolation between previous and current Dragon data.
void GameStateHandler::PrepareRender(float alpha)
{
	dragonDataLock.lock();
	for (int m = 0; m < TOTAL_DRAGONS; m++) {
		//Update world matrixes of each mesh

		float angle = alpha * currentDragonData[m].angle + (1.0f - alpha) * previousDragonData[m].angle;
		DirectX::XMMATRIX temp = DirectX::XMLoadFloat4x4(&cbData[m].world)*DirectX::XMMatrixRotationY(angle);
		DirectX::XMStoreFloat4x4(&cbData[m].world, temp);
		DirectX::XMStoreFloat4x4(&cbData[m].viewProj, DirectX::XMMatrixTranspose(viewMat*projMat));;
	}


	//copy updated gamestate to mapped memory so that the GPU can access it when needed
	memcpy((void*)pMappedCB, cbData, sizeof(cbData));

	dragonDataLock.unlock();
}