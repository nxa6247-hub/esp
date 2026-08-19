#pragma once
#include <Windows.h>

namespace inputhook {
	extern void Init(HWND hWindow);
	extern void Remove(HWND hWindow);
	static LRESULT __stdcall WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
}