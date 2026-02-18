#pragma once
#include <string>
#include <glad/glad.h>

GLuint LoadIconFromFile(const std::string& filename, int& out_width, int& out_height);







// Example usage in Engine::Initialize or another init place:
        //std::string buttonPlayPath = GetAssetPath(ICON_PLAY_BUTTON);
        //m_playIconTex = LoadIconFromFile(buttonPlayPath, iconW, iconH); // m_playIconTex is GLuint member on Engine or SpxWindow

        //if (m_playIconTex == 0) LOG_WARNING("Failed to load icon: " << buttonPlayPath);

        // ############### ImGui::ImageButton ##################
        //if (m_playIconTex != 0) {
        //    ImGui::PushID(m_playIconTex);
        //    // Use UV (0,1)-(1,0) for OpenGL texture flip if needed
        //    //new signature: bool ImageButton(const char* str_id, ImTextureID tex_id, ImVec2 size, ImVec2 uv0 = ImVec2(0, 0), ImVec2 uv1 = ImVec2(1, 1), ImVec4 bg_col = ImVec4(0, 0, 0, 0), ImVec4 tint_col = ImVec4(1, 1, 1, 1));

        //    if (ImGui::ImageButton((ImTextureID)(intptr_t)m_playIconTex, ImVec2(30, 30), ImVec2(0, 1), ImVec2(1, 0), 0)) {
        //        // Replace the problematic ImGui::ImageButton call with the correct overload
        //       
        //        if (m_actionCallback) m_actionCallback("AddCube");
        //    }
        //    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play the scene");
        //    ImGui::PopID();

        //    
        //}
        //else {
        //    // fallback to text button if icon missing
        //    if (ImGui::Button("Play", ImVec2(60, 0))) {
        //        if (m_actionCallback) m_actionCallback("Play");
        //    }
        //}
        //ImGui::SameLine();
        //if (m_playIconTex != 0) {
        //    ImGui::PushID("play_icon");
        //    // Use UV (0,1)-(1,0) for OpenGL texture flip if needed
        //    //new signature: bool ImageButton(const char* str_id, ImTextureID tex_id, ImVec2 size, ImVec2 uv0 = ImVec2(0, 0), ImVec2 uv1 = ImVec2(1, 1), ImVec4 bg_col = ImVec4(0, 0, 0, 0), ImVec4 tint_col = ImVec4(1, 1, 1, 1));

        //    if (ImGui::ImageButton((ImTextureID)(intptr_t)m_playIconTex, ImVec2(30, 30), ImVec2(0, 1), ImVec2(1, 0), 0)) {
        //        // Replace the problematic ImGui::ImageButton call with the correct overload

        //        if (m_actionCallback) m_actionCallback("AddCube");
        //    }
        //    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add Cube");
        //    ImGui::PopID();


        //}
        //else {
        //    // fallback to text button if icon missing
        //    if (ImGui::Button("Play", ImVec2(60, 0))) {
        //        if (m_actionCallback) m_actionCallback("Play");
        //    }
        //}

      //  ImGui::TextColored(COLOR_LIGHTBLUE, ICON_FA_IMAGE " Available options");

        /*if (ImGui::Button(ICON_FA_AD "##", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("AddCube");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a Cube to the scene");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_AD "##", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("AddPlane");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a Plane to the scene");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_AD "##", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("AddFloor");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a Floor to the scene");*/







