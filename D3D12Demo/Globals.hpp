#pragma once

//#include "..\extLib\ctpl_stl.h"
#include <d3d12.h>

#include <thread>
#include <mutex>
#include <vector>


// TOTAL_TRIS pretty much decides how many drawcalls in a brute force approach.
constexpr int TOTAL_TRIS = 4000;
// this has to do with how the triangles are spread in the screen, not important.
constexpr int TOTAL_PLACES = 2 * TOTAL_TRIS * 20;

const unsigned int SCREEN_WIDTH = 640; //Width of application.
const unsigned int SCREEN_HEIGHT = 480;	//Height of application.

const unsigned int MAX_PREPARED_FRAMES = 3; // number of frames that can be queued
const unsigned int NUM_SWAP_BUFFERS = 3; //Number of buffers

bool gIsRunning = true;
std::mutex gThreadIDIndexLock;
std::mutex gBufferTransferLock;


//// Thread pool
//ctpl::thread_pool gThreadPool(10); //1 CPU update, 9 render

HWND gWndHandle;

//todo move to Project class
#ifdef VETTIG_DATOR
ID3D12Device5*				gDevice5 = nullptr;
#else
ID3D12Device4*				gDevice5 = nullptr;
#endif