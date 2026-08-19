#include "interface.hpp"


auto Render::user_interface() -> void
{
 
    ImGui::SetNextWindowSize(ImVec2(560, 660), ImGuiCond_Once);

    ImGui::Begin("Rainbow6 Internal Example", nullptr, ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::CollapsingHeader("Visuals", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable Visuals", &visuals::bEnable);
        ImGui::Checkbox("Team Check", &visuals::bTeamCheck);
        ImGui::Checkbox("Dead Check", &visuals::bDeadCheck);

        if (visuals::bEnable)
        {
            ImGui::Checkbox("Box", &visuals::bBox);
            ImGui::SameLine();
            ImGui::ColorEdit4("##BoxRGB", &visuals::BoxRGB.x, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            ImGui::Combo("##BoxType", &visuals::iBoxype, visuals::cBoxType, IM_ARRAYSIZE(visuals::cBoxType));

            ImGui::Checkbox("Snaplines", &visuals::bSnaplines);
            ImGui::SameLine();
            ImGui::ColorEdit4("##SnaplineRGB", &visuals::SnaplineRGB.x, ImGuiColorEditFlags_NoInputs);
        }
    }

    if (ImGui::CollapsingHeader("Weapon", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("No Spread", &weapon::bNoSpread);
    }

    ImGui::End();
}


