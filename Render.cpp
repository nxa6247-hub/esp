#include "Render.hpp"

#define IM_PI 3.14159265358979323846

auto Render::text_outline(const ImVec2& Position, ImU32 Black, ImU32 Color, const char* Text) -> void
{
	ImGui::GetBackgroundDrawList()->AddText({ Position.x - 1, Position.y }, Black, Text);
	ImGui::GetBackgroundDrawList()->AddText({ Position.x + 1, Position.y }, Black, Text);
	ImGui::GetBackgroundDrawList()->AddText({ Position.x, Position.y - 1 }, Black, Text);
	ImGui::GetBackgroundDrawList()->AddText({ Position.x, Position.y + 1 }, Black, Text);

	ImGui::GetBackgroundDrawList()->AddText(Position, Color, Text);
}

inline void Render::DrawImage(ImTextureID ptexture, float_t x, float_t y, float_t w, float_t h, ImColor color)
{
    ImGui::GetBackgroundDrawList()->AddImage(ptexture, ImVec2(x, y), ImVec2(x + w, y + h), ImVec2(0, 0), ImVec2(1, 1), color);
}

void Render::DrawPlayerTags(const ubiVector2& Position, float Health, float MaxHealth, float Distance, const char* UserName /* const char* Operator */, ImColor color, ImColor distanceColor)
{
    float healthPercentage = Health / MaxHealth;
    if (healthPercentage >= 1) healthPercentage = 1.f;

    std::string Full = std::to_string((int)std::round(Distance)) + ("m ") + UserName;
    std::string DistanceStr = std::to_string((int)std::round(Distance)) + "m ";

    const ImVec2 text_dimension = ImGui::CalcTextSize(Full.c_str());
    const float text_width = text_dimension.x + 5.f;
    const float offsetX = -10.0f;
    const float mid_width = Position.x - (text_width / 2.f) + offsetX;

    ImVec2 distance_dimension = ImGui::CalcTextSize(DistanceStr.c_str());
    ImVec2 username_dimension = ImGui::CalcTextSize(UserName);

    ImVec2 tagTopLeft = ImVec2(mid_width + 2.f, Position.y - 25.f);
    ImVec2 tagBottomRight = ImVec2(mid_width + distance_dimension.x + username_dimension.x + 25.f, Position.y - 25.f + text_dimension.y + 1.f);

    ImGui::GetBackgroundDrawList()->AddRectFilled(tagTopLeft, ImVec2(mid_width + distance_dimension.x + 5.f + 2.f, tagBottomRight.y), distanceColor);
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(mid_width + distance_dimension.x + 5.f + 2.f, tagTopLeft.y), tagBottomRight, color);


    float extraSpacing = 14.0f;
    ImVec2 distancePos = ImVec2(mid_width + 6.f, (Position.y - 26.5f) + 2.f);
    ImVec2 usernamePos = ImVec2(mid_width + distance_dimension.x + extraSpacing, (Position.y - 26.5f) + 2.f);

    Render::text_outline(distancePos, IM_COL32_BLACK, IM_COL32_WHITE, DistanceStr.c_str());
    Render::text_outline(usernamePos, IM_COL32_BLACK, IM_COL32_WHITE, UserName);


    ImVec2 healthBarPos = ImVec2(mid_width + 2.f, Position.y - 27.f + text_dimension.y + 3.f);
    ImVec2 healthBarSize = ImVec2(text_width + 18.7f, 2);

    ImU32 healthColor = IM_COL32(
        (1.f - healthPercentage) * 255,
        255 * healthPercentage * 0.9f,
        0, 255);

    ImGui::GetBackgroundDrawList()->AddRectFilled(
        healthBarPos,
        { healthBarPos.x + healthBarSize.x, healthBarPos.y + healthBarSize.y },
        ImColor(0, 0, 0, 255));

    ImGui::GetBackgroundDrawList()->AddRectFilled(
        healthBarPos,
        { healthBarPos.x + healthBarSize.x * healthPercentage, healthBarPos.y + healthBarSize.y },
        healthColor);


    /* if (auto icon = GetIconByName(Operator); icon)
        Render::DrawImage(icon, mid_width - 23.f, Position.y - 28.3f, 24.f, 24.f, ImColor(255, 255, 255, 255)); */
}
