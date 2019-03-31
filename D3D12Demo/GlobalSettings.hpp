#pragma once

#include <d3d12.h>
#include <thread>
#include <mutex>


//#define VETTIG_DATOR
//#define RECORD_TIME

constexpr int TOTAL_DRAGONS = 104;
static const double gMovementSpeed = 0.1;

const int SCREEN_WIDTH  = 1200; //Width of application.
const int SCREEN_HEIGHT = 900;  //Height of application.
const FLOAT gClearColor[] = { 0.0f, 0.16f, 0.21f, 1.0f };

const unsigned int NUM_THREADS = 12;
const unsigned int MAX_FRAME_LATENCY = 2;

constexpr unsigned int LISTS_PER_FRAME     = 5;
constexpr unsigned int DRAGONS_PER_LIST    = (TOTAL_DRAGONS / LISTS_PER_FRAME);
constexpr unsigned int NUM_SWAP_BUFFERS    = MAX_FRAME_LATENCY + 1; //Number of buffers
constexpr unsigned int NUM_STAGES_IN_FRAME = 3;

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


