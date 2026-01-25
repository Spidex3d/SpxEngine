#include "EditorInput.h"
#include "../Camera/Camera.h"

#include "imgui\imgui.h"
#include <GLFW/glfw3.h>
#include <iostream>

EditorInput::EditorInput(GLFWwindow* window)
    : m_window(window), m_camera(nullptr)
{
    if (m_window) {
        double x, y;
        glfwGetCursorPos(m_window, &x, &y);
        m_lastX = x;
        m_lastY = y;
    }
}

EditorInput::~EditorInput() = default;

void EditorInput::SetSceneHovered(bool hovered) {
    m_sceneHovered = hovered;
    // reset mouse tracking when focus changes to avoid large jumps
    if (m_sceneHovered && m_window) {
        double x, y;
        glfwGetCursorPos(m_window, &x, &y);
        m_lastX = x;
        m_lastY = y;
        m_firstMouse = true;
    }
}

void EditorInput::SetCamera(Camera* cam) {
    m_camera = cam;
}

void EditorInput::Update(float dt) {
    if (!m_camera || !m_window) return;

    // Respect ImGui capture flags: if UI wants the mouse/keyboard, skip camera control
    ImGuiIO& io = ImGui::GetIO();
    if (!m_sceneHovered || io.WantCaptureMouse || io.WantCaptureKeyboard) {
        // still update last mouse pos to avoid jump when re-entering
        double cx, cy;
        glfwGetCursorPos(m_window, &cx, &cy);
        m_lastX = cx;
        m_lastY = cy;
        m_firstMouse = true;
        return;
    }

    // Keyboard movement (WASD + Z/X up/down). Use glfw polling for responsiveness.
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)  m_camera->ProcessKeyboard(FORWARD, dt);
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)  m_camera->ProcessKeyboard(BACKWARD, dt);
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)  m_camera->ProcessKeyboard(LEFT, dt);
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)  m_camera->ProcessKeyboard(RIGHT, dt);
    if (glfwGetKey(m_window, GLFW_KEY_Z) == GLFW_PRESS)  m_camera->ProcessKeyboard(UP, dt);
    if (glfwGetKey(m_window, GLFW_KEY_X) == GLFW_PRESS)  m_camera->ProcessKeyboard(UP, -dt);

    // Mouse: only when right mouse button is held (simple "look" control)
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(m_window, &xpos, &ypos);

        if (m_firstMouse) {
            m_lastX = xpos;
            m_lastY = ypos;
            m_firstMouse = false;
        }

        float xoffset = static_cast<float>(xpos - m_lastX);
        float yoffset = static_cast<float>(m_lastY - ypos); // reversed Y

        m_lastX = xpos;
        m_lastY = ypos;

        // Apply mouse movement to camera rotation
        m_camera->ProcessMouseMovement(xoffset, yoffset, true);
    }
    else {
        // release tracking so we don't get a huge jump when re-entering
        m_firstMouse = true;
    }

    // Scroll wheel via ImGui IO (works even without callbacks)
    float wheel = io.MouseWheel;
    if (wheel != 0.0f) {
        m_camera->ProcessMouseScroll(wheel);
    }
}