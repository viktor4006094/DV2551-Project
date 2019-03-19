#pragma once

//#include "..\extLib\ctpl_stl.h"
#include <d3d12.h>

#include <thread>
#include <mutex>
#include <vector>

//#define VETTIG_DATOR
#define RECORD_TIME

const unsigned int NUM_THREADS = 10;

const unsigned int MAX_FRAME_LATENCY = 3;

// TOTAL_TRIS pretty much decides how many drawcalls in a brute force approach.
constexpr int TOTAL_TRIS = 4; //todo rename
// this has to do with how the triangles are spread in the screen, not important.
constexpr int TOTAL_PLACES = 2 * 400 * 20;

static float gXT[TOTAL_PLACES];
static float gYT[TOTAL_PLACES];

static const double gMovementSpeed = 0.1;

const int SCREEN_WIDTH = 2*640; //Width of application.
const int SCREEN_HEIGHT = 2*480;	//Height of application.


const FLOAT gClearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };

const unsigned int MAX_PREPARED_FRAMES = 3; // number of frames that can be queued
const unsigned int NUM_SWAP_BUFFERS = 3; //Number of buffers


static std::mutex gThreadIDIndexLock;
static std::mutex gBufferTransferLock;

static std::mutex gPresentLock;

const unsigned int NUM_TIMESTAMP_PAIRS = 100;

#ifdef VETTIG_DATOR
typedef ID3D12Device5* D3D12DevPtr;
typedef ID3D12GraphicsCommandList4* D3D12GraphicsCommandListPtr;
#else
typedef ID3D12Device4* D3D12DevPtr;
typedef ID3D12GraphicsCommandList3* D3D12GraphicsCommandListPtr;
#endif


