#pragma once

#include <windows.h>

HWND				InitWindow(HINSTANCE hInstance);	//1. Create Window
LRESULT CALLBACK	WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
