#pragma once
#include <memory>


#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

class Camera;
#include <vector>

class EditorInput {
public:
    explicit EditorInput(GLFWwindow* window);
    ~EditorInput();


    bool HasKeyboardAttached() const;
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
    double m_lastX;
    double m_lastY;
    bool m_firstMouse = true;
	bool m_mouse = true;
};
