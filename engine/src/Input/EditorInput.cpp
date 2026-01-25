#include "EditorInput.h"
#include "../Camera/Camera.h"

#include "imgui\imgui.h"
#include <GLFW/glfw3.h>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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

bool EditorInput::HasKeyboardAttached() const
{
#ifdef _WIN32
    // Query raw input device list
    UINT numDevices = 0;
    if (GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST)) != 0) {
        return false;
    }
    if (numDevices == 0) return false;

    std::vector<RAWINPUTDEVICELIST> devs(numDevices);
    if (GetRawInputDeviceList(devs.data(), &numDevices, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        return false;
    }
    for (UINT i = 0; i < numDevices; ++i) {
        if (devs[i].dwType == RIM_TYPEKEYBOARD) {
            return true;
        }
    }
    return false;
#else
    // On non-Windows platforms assume keyboard is present (alternatively implement platform check)
    return true;
#endif
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
        //return;
    }

    // Keyboard movement (WASD + Z/X up/down). Use glfw polling for responsiveness.
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)  m_camera->ProcessKeyboard(FORWARD, dt);
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)  m_camera->ProcessKeyboard(BACKWARD, dt);
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)  m_camera->ProcessKeyboard(LEFT, dt);
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)  m_camera->ProcessKeyboard(RIGHT, dt);
    if (glfwGetKey(m_window, GLFW_KEY_Z) == GLFW_PRESS)  m_camera->ProcessKeyboard(UP, dt);
    if (glfwGetKey(m_window, GLFW_KEY_X) == GLFW_PRESS)  m_camera->ProcessKeyboard(UP, -dt);

    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(m_window, true);
    }
	if (glfwGetKey(m_window, GLFW_KEY_O) == GLFW_PRESS) {
		glfwSetWindowOpacity(m_window, 0.5f);
	}
	if (glfwGetKey(m_window, GLFW_KEY_P) == GLFW_PRESS) {
		glfwSetWindowOpacity(m_window, 1.0f);
	}

	// Mouse movement via ImGui IO (works even without callbacks)  
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float xoffset = io.MouseDelta.x;
        float yoffset = -io.MouseDelta.y; // invert if your ProcessMouseMovement expects reversed Y
        if (xoffset != 0.0f || yoffset != 0.0f) {
            m_camera->ProcessMouseMovement(xoffset, yoffset, true);
            // std::cout << "Mouse delta (ImGui IO): " << xoffset << ", " << yoffset << std::endl;
        }
    }
    

    // Scroll wheel via ImGui IO (works even without callbacks)
    float wheel = io.MouseWheel;
    if (wheel != 0.0f) {
        m_camera->ProcessMouseScroll(wheel);
    }
}


