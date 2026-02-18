#pragma once
#include <memory>


#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"   // for ImVec2, ImGuiIO
#include "../include/entity.h"    // GameObj


class Engine; // forward declaration

class Camera; // forward declaration
#include <vector>

class EditorInput {
public:
    explicit EditorInput(GLFWwindow* window);
    ~EditorInput();


    bool HasKeyboardAttached() const;
    // Must be called every frame (dt in seconds)
    void Update(float dt);   
    // ##################################################### Picking ########################################################
   // Picking: returns index into entities vector, or -1 if none.
    int TryPick(const std::vector<std::unique_ptr<GameObj>>& entities, const ImVec2& viewportPos, const ImVec2& viewportSize);
    
    int ProcessViewportPick(const std::vector<std::unique_ptr<GameObj>>& entities, const ImVec2& viewportPos,
    // ##################################################### Picking ########################################################
        const ImVec2& viewportSize, bool sceneHovered, float dt);
   
    // Tell the input system whether the scene viewport is currently hovered by the mouse.
    // When false, input will not affect the camera (so UI interactions work).
    void SetSceneHovered(bool hovered);

    // Provide the camera instance to drive
    void SetCamera(Camera* cam);

private:
    GLFWwindow* m_window = nullptr;
    Camera* m_camera = nullptr;
	

    bool m_sceneHovered = false;
        
    // simple mouse tracking
    double m_lastX;
    double m_lastY;
    bool m_firstMouse = true;
	bool m_mouse = true;

    
};


