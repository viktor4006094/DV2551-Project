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

static float gXT[TOTAL_PLACES];
static float gYT[TOTAL_PLACES];

const unsigned int SCREEN_WIDTH = 640; //Width of application.
const unsigned int SCREEN_HEIGHT = 480;	//Height of application.

const unsigned int MAX_PREPARED_FRAMES = 3; // number of frames that can be queued
const unsigned int NUM_SWAP_BUFFERS = 3; //Number of buffers


static std::mutex gThreadIDIndexLock;
static std::mutex gBufferTransferLock;


#ifdef VETTIG_DATOR
typedef ID3D12Device5* D3D12DevPtr;
typedef ID3D12GraphicsCommandList4* D3D12GraphicsCommandListPtr;
#else
typedef ID3D12Device4* D3D12DevPtr;
typedef ID3D12GraphicsCommandList3* D3D12GraphicsCommandListPtr;
#endif


