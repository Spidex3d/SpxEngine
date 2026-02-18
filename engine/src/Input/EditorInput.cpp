#include "EditorInput.h"
#include <engine.h>
#include "../Camera/Camera.h"

#include <GLFW/glfw3.h>
#include "imgui\imgui.h"
#include <iostream>
// ##################################################### Picking ########################################################
#include <cfloat>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "../entity.h"
// ##################################################### End Picking #####################################################
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


// ######################################################################################################################
// ##################################################### Try Picking ########################################################
// ######################################################################################################################
static bool RayIntersectsAABB(const glm::vec3& orig, const glm::vec3& dir,
    const glm::vec3& minB, const glm::vec3& maxB,
    float& tNearOut)
{
    float tmin = -FLT_MAX;
    float tmax = FLT_MAX;

    for (int i = 0; i < 3; ++i) {
        float o = orig[i];
        float d = dir[i];
        float minv = minB[i];
        float maxv = maxB[i];

        if (fabs(d) < 1e-8f) {
            if (o < minv || o > maxv) return false;
        }
        else {
            float invD = 1.0f / d;
            float t1 = (minv - o) * invD;
            float t2 = (maxv - o) * invD;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
            if (tmax < 0.0f) return false; // AABB is behind
        }
    }
    tNearOut = (tmin >= 0.0f) ? tmin : tmax;
    return true;
}

// Convert mouse + viewport -> world ray using glm::unProject
static void ScreenPosToWorldRay(const Camera* camera,
    const ImVec2& mousePosScreen,
    const ImVec2& viewportPosScreen,
    const ImVec2& viewportSizeScreen,
    const ImVec2& fbScale,
    glm::vec3& out_origin,
    glm::vec3& out_dir)
{
    // Map mouse pos into viewport pixels
    float mx = (mousePosScreen.x - viewportPosScreen.x) * fbScale.x;
    float my = (mousePosScreen.y - viewportPosScreen.y) * fbScale.y;

    float fbW = viewportSizeScreen.x * fbScale.x;
    float fbH = viewportSizeScreen.y * fbScale.y;
    if (fbW <= 0.0f || fbH <= 0.0f) {
        out_origin = glm::vec3(0.0f);
        out_dir = glm::vec3(0.0f, 0.0f, -1.0f);
        return;
    }

    // OpenGL window coordinates: origin at bottom-left
    float winX = mx;
    float winY = fbH - my;
    glm::vec4 viewportGL(0.0f, 0.0f, fbW, fbH);

    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 proj = camera->GetProjectionMatrix(fbW / fbH);

    glm::vec3 nearP = glm::unProject(glm::vec3(winX, winY, 0.0f), view, proj, viewportGL);
    glm::vec3 farP = glm::unProject(glm::vec3(winX, winY, 1.0f), view, proj, viewportGL);

    out_origin = nearP;
    out_dir = glm::normalize(farP - nearP);
}

// EditorInput::TryPick implementation
// ############################################# Piking ############################################

// EditorInput::TryPick implementation (use canonical local AABB)
int EditorInput::TryPick(const std::vector<std::unique_ptr<GameObj>>& entities, const ImVec2& viewportPos, const ImVec2& viewportSize)
{
    if (!m_camera) return -1;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePosScreen = ImGui::GetMousePos();
    ImVec2 fbScale(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

    glm::vec3 rayOrigin, rayDir;
    ScreenPosToWorldRay(m_camera, mousePosScreen, viewportPos, viewportSize, fbScale, rayOrigin, rayDir);

    float bestDist = FLT_MAX;
    int bestIndex = -1;

    // canonical local AABB for your cube/plane primitives
    glm::vec3 aabbMinLocal(-0.5f, -0.5f, -0.5f);
    glm::vec3 aabbMaxLocal(0.5f, 0.5f, 0.5f);

    for (int i = 0; i < (int)entities.size(); ++i) {
        GameObj* obj = entities[i].get();
        if (!obj) continue;
        if (!obj->isVisible) continue;

        // transform ray into object's local space
        glm::mat4 invModel = glm::inverse(obj->modelMatrix);
        glm::vec3 localOrig = glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f));
        glm::vec3 localFar = glm::vec3(invModel * glm::vec4(rayOrigin + rayDir, 1.0f));
        glm::vec3 localDir = glm::normalize(localFar - localOrig);

        float tLocal;
        if (RayIntersectsAABB(localOrig, localDir, aabbMinLocal, aabbMaxLocal, tLocal)) {
            glm::vec3 localHit = localOrig + localDir * tLocal;
            glm::vec3 worldHit = glm::vec3(obj->modelMatrix * glm::vec4(localHit, 1.0f));
            float dist = glm::length(worldHit - rayOrigin);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = i;
            }
        }
    }

    return bestIndex;
}
// new
int EditorInput::ProcessViewportPick(const std::vector<std::unique_ptr<GameObj>>& entities, const ImVec2& viewportPos, const ImVec2& viewportSize, bool sceneHovered, float dt)
{
    // Keep EditorInput internal hovered state in sync and run the per-frame update.
    SetSceneHovered(sceneHovered);
    // Note: Update uses m_camera etc. Pass dt so camera movement still works while picking.
    Update(dt);

    // Early-out if no camera or no entities
    if (!m_camera) return -1;
    if (entities.empty()) return -1;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = ImGui::GetMousePos();

    bool mouseInsideViewport =
        (mousePos.x >= viewportPos.x && mousePos.x <= viewportPos.x + viewportSize.x &&
            mousePos.y >= viewportPos.y && mousePos.y <= viewportPos.y + viewportSize.y);

    // Trigger pick when left button is held and cursor is inside viewport,
    // and no ImGui widget is actively using the mouse (avoids interfering with UI).
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && mouseInsideViewport && ImGui::GetActiveID() == 0) {
        // Call existing TryPick helper which does ray-unproject + AABB tests
        int picked = TryPick(entities, viewportPos, viewportSize);
        return picked;
    }

    return -1;
}



// ########################################## End Piking ############################################






