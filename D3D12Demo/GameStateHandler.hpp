#pragma once

#include "GlobalSettings.hpp"
#include "StructsAndEnums.hpp"

#include <vector>
#include <chrono>

class GameStateHandler
{
public:
	GameStateHandler();
	~GameStateHandler();

	void CreatePerMeshData();
	void ShutDown();
	void Update(int id);

	void PrepareRender(float alpha);

	std::vector<PER_DRAGON_DATA> tempDragonData;
	std::vector<PER_DRAGON_DATA> currentDragonData;
	std::vector<PER_DRAGON_DATA> previousDragonData;

	std::mutex dragonDataLock;

	DRAGON_START_MATRIX startMatrixes[TOTAL_DRAGONS];

	CONSTANT_BUFFER_DATA cbData[TOTAL_DRAGONS];
	CONSTANT_BUFFER_DATA *pMappedCB = nullptr;

	std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::high_resolution_clock::now();
private:
	DirectX::XMMATRIX projMat = DirectX::XMMatrixOrthographicLH((48.0f*SCREEN_WIDTH) / SCREEN_HEIGHT, 48.0f, 0.1f, 100.0f);
	DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtLH({ 0, 10, -50 }, { 0, 10, 0 }, { 0,1,0 });
	
	bool isRunning = true;
};