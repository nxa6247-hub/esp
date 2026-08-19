#pragma once
#include ".../../External/imgui/imgui.h"
#include <Windows.h>
#include <cstdint>

namespace d3d12 {
	inline HMODULE mainModule;
	inline HWND mainWindow = NULL;
	inline int uninjectKey;
	inline int openMenuKey;
}

namespace visuals {


	inline bool bEnable = true;
	
	inline bool bSnaplines;
	inline ImVec4 SnaplineRGB = { 1.f, 1.f, 1.f, 1.f };

	inline bool bBox = true;
	inline ImVec4 BoxRGB = { 1.f, 1.f, 1.f, 1.f };

	inline int iBoxype = 0;
	inline const char* cBoxType[3] = { "3D Box", "2D Box", "2D Cornered" };

	inline bool bTeamCheck = true;
	inline bool bDeadCheck = true;


}

namespace weapon
{
	inline bool bNoSpread = false;
	inline constexpr std::uint64_t k_spread_rva = 0x110382E0;
}


