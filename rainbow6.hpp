#pragma once
#include "../config.hpp"
#include "../External/imgui/imgui.h"
#include "../scimitar/hooks/hooks.hpp"
#include "../directx/Render/Render.hpp"

namespace rainbow6 
{


	auto visuals_renderables(bool enable) -> void;
	auto weapon_nospread() -> void;



	inline auto Run() -> void
	{
		weapon_nospread();
		visuals_renderables(visuals::bEnable);
	};
}