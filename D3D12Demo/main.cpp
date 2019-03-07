//--------------------------------------------------------------------------------------
// BTH - 2018
// 170123: Mikael Olofsson, initial version
// 170131: Stefan Petersson, added constant buffer and some cleanups/updates
// 180129: Stefan Petersson, cleanups/updates DXGI 1.5
// 181009: Stefan Petersson, cleanups/updates DXGI 1.6
//--------------------------------------------------------------------------------------

/* 
//! Per-frame order of execution

1. Render scene to a RTV texture that's not in the swapchain

2. Use that texture as a SRV in the FXAA compute shader and write to a UAV

3. Copy resource from the UAV to the backbuffer, no passthrough-pass needed
   See line 890 in the Ray-Tracing example for how this is done
*/



#include "main.hpp"

#include "Project.hpp"

#pragma region wwinMain
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	
	HWND gWndHandle;
	Project* project = new Project();

	MSG msg		= {0};
	gWndHandle	= InitWindow(hInstance);			//1. Create Window
	if(gWndHandle)
	{
		project->Init(gWndHandle);

		project->Start();

		ShowWindow(gWndHandle, nCmdShow);
		while(WM_QUIT != msg.message)
		{
			if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		project->Stop();
	}
	
	// sleep main thread for half a second so that worker threads have time to finish executing
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	
	//Wait for GPU execution to be done and then release all interfaces.
	project->Shutdown();
	
	delete project;

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
		L"DV2551 JPEG Project",
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
