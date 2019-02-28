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
	void Update(int id);

	void writeNewestGameStateToReadOnlyAtIndex(int index);
	GameState* getReadOnlyStateAtIndex(int index);

	bool isRunning = true;
private:

	

	/*
		CPU updates writeState
		Once CPU update is done writeState is copied to bufferState
		Before each new GPU frame the bufferState is copied to readOnlyState which is used by the GPU
	*/
	GameState writeState;
	GameState bufferState;
	GameState readOnlyState[MAX_PREPARED_FRAMES]; // might not be needed for correctness

};