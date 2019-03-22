#pragma once

//#include "..\extLib\ctpl_stl.h"
#include <d3d12.h>

#include <thread>
#include <mutex>
#include <vector>

//#define VETTIG_DATOR
//#define RECORD_TIME

const unsigned int NUM_THREADS = 10;

const unsigned int MAX_FRAME_LATENCY = 3;


// TOTAL_TRIS pretty much decides how many drawcalls in a brute force approach.
constexpr int TOTAL_DRAGONS = 104;
constexpr unsigned int LISTS_PER_FRAME = 5;
const unsigned int DRAGONS_PER_LIST = (TOTAL_DRAGONS / LISTS_PER_FRAME);

static const double gMovementSpeed = 0.1;

const int SCREEN_WIDTH  = 3*400; //Width of application.
const int SCREEN_HEIGHT = 3*300; //Height of application.


//const FLOAT gClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
const FLOAT gClearColor[] = { 0.0f, 0.16f, 0.21f, 1.0f };

const unsigned int NUM_SWAP_BUFFERS = MAX_FRAME_LATENCY + 1; //Number of buffers

const unsigned int NUM_STAGES_IN_FRAME = 3;

static std::mutex gThreadIDIndexLock;
static std::mutex gBufferTransferLock;

static std::mutex gPresentLock;

const unsigned int FIRST_TIMESTAMPED_FRAME = 1;
const unsigned int NUM_TIMESTAMP_PAIRS = 20;

#ifdef VETTIG_DATOR
typedef ID3D12Device5* D3D12DevPtr;
typedef ID3D12GraphicsCommandList4* D3D12GraphicsCommandListPtr;
#else
typedef ID3D12Device4* D3D12DevPtr;
typedef ID3D12GraphicsCommandList3* D3D12GraphicsCommandListPtr;
#endif


