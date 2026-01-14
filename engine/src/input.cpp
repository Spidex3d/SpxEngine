// input.cpp
#include "input.h"
#include <cassert>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#endif

// Constructor: allocate buffers sized to cover 0..GLFW_KEY_LAST inclusive
Input::Input(GLFWwindow* window)
    : window(window)
{
    int size = GLFW_KEY_LAST + 1;
    currFrame.assign(size, 0);
    prevFrame.assign(size, 0);
}


bool Input::HasKeyboardAttached() const {
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

void Input::Update() {
    // Defensive: if window is null, nothing to do
    if (!window) {
        return;
    }

    // Let GLFW process events once per frame
    glfwPollEvents();

    // copy current -> prev
    prevFrame = currFrame;

    // Sample key states safely for the whole range
    int last = GLFW_KEY_LAST;
    if ((int)currFrame.size() < last + 1) {
        currFrame.assign(last + 1, 0);
        prevFrame.assign(last + 1, 0);
    }

    for (int k = 0; k <= last; ++k) {
        int state = glfwGetKey(window, k);
        currFrame[k] = (state == GLFW_PRESS) ? 1 : 0;
    }
}

bool Input::IsKeyDown(int glfwKey) const {
    if (glfwKey < 0 || glfwKey >= (int)currFrame.size()) return false;
    return currFrame[glfwKey];
}

bool Input::IsKeyPressed(int glfwKey) const {
    if (glfwKey < 0 || glfwKey >= (int)currFrame.size()) return false;
    return currFrame[glfwKey] && !prevFrame[glfwKey];
}

bool Input::IsKeyReleased(int glfwKey) const {
    if (glfwKey < 0 || glfwKey >= (int)currFrame.size()) return false;
    return !currFrame[glfwKey] && prevFrame[glfwKey];
}


//#include "input.h"
//
//Input::Input(GLFWwindow* window)
//    : window(window)
//{
//    // GLFW defines keys up to GLFW_KEY_LAST; size vectors accordingly
//    currFrame.assign(GLFW_KEY_LAST, 0);
//    prevFrame.assign(GLFW_KEY_LAST, 0);
//}
//
//void Input::Update() {
//    // Let GLFW update its internal states and callbacks
//    glfwPollEvents();
//
//    // copy current -> prev, then sample new current states
//    prevFrame = currFrame;
//    for (int k = 0; k < GLFW_KEY_LAST; ++k) {
//        int state = glfwGetKey(window, k);
//        currFrame[k] = (state == GLFW_PRESS);
//    }
//}
//
//bool Input::IsKeyDown(int glfwKey) const {
//    if (glfwKey < 0 || glfwKey >= GLFW_KEY_LAST) return false;
//    return currFrame[glfwKey];
//}
//
//bool Input::IsKeyPressed(int glfwKey) const {
//    if (glfwKey < 0 || glfwKey >= GLFW_KEY_LAST) return false;
//    return currFrame[glfwKey] && !prevFrame[glfwKey];
//}
//
//bool Input::IsKeyReleased(int glfwKey) const {
//    if (glfwKey < 0 || glfwKey >= GLFW_KEY_LAST) return false;
//    return !currFrame[glfwKey] && prevFrame[glfwKey];
//}