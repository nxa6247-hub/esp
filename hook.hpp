#pragma once
#include <windows.h>
#include <cstdio>
#include <cstdint>

#include <dxgi.h>
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")


#include "../../directx/Hook/InputHook/input_hook.hpp"
#include "../../directx/d3d12hook.hpp"

#include "../../External/imgui/imgui.h"
#include "../../External/imgui/imgui_impl_dx12.h"
#include "../../External/imgui/imgui_impl_win32.h"
#include "../../External/kiero/kiero.hpp"


namespace hooks {
	extern void Init();
}
