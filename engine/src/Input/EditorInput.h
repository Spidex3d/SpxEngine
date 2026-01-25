#pragma once
#include <memory>


#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

class Camera;

class EditorInput {
public:
    explicit EditorInput(GLFWwindow* window);
    ~EditorInput();

    // Must be called every frame (dt in seconds)
    void Update(float dt);

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
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    bool m_firstMouse = true;
};
