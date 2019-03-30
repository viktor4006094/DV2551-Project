#include "Project.hpp"

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <windows.h>


#pragma region ForwardDeclerations
HWND				InitWindow(HINSTANCE hInstance);	//1. Create Window
LRESULT CALLBACK	WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#pragma endregion

#pragma region wwinMain
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	HWND gWndHandle;
	Project* project = new Project();

	MSG msg		= {0};
	gWndHandle	= InitWindow(hInstance);			//1. Create Window
	if(gWndHandle) {
		project->Init(gWndHandle);
		project->Start();

		ShowWindow(gWndHandle, nCmdShow);
		while(WM_QUIT != msg.message) {
			if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		// Stop the application and wait for GPU execution to finish
		project->Stop();
	}
	// Release all interfaces
	project->Shutdown();

	delete project;
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtDumpMemoryLeaks();
	
	return (int)msg.wParam;
}
#pragma endregion

#pragma region InitWindow
HWND InitWindow(HINSTANCE hInstance)//1. Create Window
{
	WNDCLASSEX wcex		= {0};
	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.lpfnWndProc	= WndProc;
	wcex.hInstance		= hInstance;
	wcex.lpszClassName	= L"DV2551_Project";
	if (!RegisterClassEx(&wcex))
	{
		return false;
	}

	RECT rc = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, false, WS_EX_OVERLAPPEDWINDOW);

	return CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,
		L"DV2551_Project",
		L"DV2551 FXAA Project",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);
}
#pragma endregion

#pragma region WndProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) 
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;		
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
#pragma endregion
