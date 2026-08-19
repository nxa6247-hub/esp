#pragma once
#include "../../External/imgui/imgui.h"
#include "../../scimitar/engine/havok_math.hpp"

namespace Render {

	auto text_outline(const ImVec2& Position, ImU32 Black, ImU32 Color, const char* Text) -> void;
	auto DrawImage(ImTextureID ptexture, float_t x, float_t y, float_t w, float_t h, ImColor color) -> void;
	auto DrawPlayerTags(const ubiVector2& m_Position, float m_health, float m_MaxHealth, float m_Distance, const char* m_UserName, ImColor m_Color_1, ImColor m_Color_2) -> void;

}