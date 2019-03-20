#pragma once

#include "ConstantsAndGlobals.hpp"
#include "StructsAndEnums.hpp"
#include <vector>

class GameStateHandler
{
public:
	GameStateHandler();
	~GameStateHandler();

	void CreateMeshes();
	void ShutDown();
	void Update(int id, UINT* currentFrameIndex);

	//void writeNewestGameStateToReadOnlyAtIndex(int index);
	//GameState* getReadOnlyStateAtIndex(int index);

	bool isRunning = true;
	//DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovLH(3.0f*(3.14f / 180.0f), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 2000.0f);
	DirectX::XMMATRIX projMat = DirectX::XMMatrixOrthographicLH(SCREEN_WIDTH / 20.0f, SCREEN_HEIGHT / 20.0f, 0.1f, 100.0f);
	DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtLH({ 0, 10, -50 }, { 0, 10, 0 }, { 0,1,0 });
	CONSTANT_BUFFER_DATA cbData[TOTAL_TRIS];
	CONSTANT_BUFFER_DATA *pMappedCB[NUM_SWAP_BUFFERS];
private:

	/*
		CPU updates writeState
		Once CPU update is done writeState is copied to bufferState
		Before each new GPU frame the bufferState is copied to readOnlyState which is used by the GPU
	*/
	//GameState writeState;
	//GameState bufferState;
	//GameState readOnlyState[MAX_PREPARED_FRAMES]; // might not be needed for correctness

};