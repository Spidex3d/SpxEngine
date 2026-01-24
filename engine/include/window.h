#pragma once
#include <functional>
#include <imgui\ImGuiAF.h>
#include <imgui\imgui.h>
#include <imgui\imgui_internal.h>

// Ensure GLFW doesn't include OpenGL headers (safe when glad is used to load GL)
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <glad/glad.h> // for GLuint & GL calls used by framebuffer helpers
//#include <functional>

// Make sure glEnable(GL_DEPTH_TEST) is set (you already do in engine) and glEnable(GL_CULL_FACE) if you want backface culling.

struct WindowConfig {
    int width = 1280;
    int height = 720;
    const char* title = "SPXEngine";
    bool resizable = true;
    bool vsync = true;
};

class SpxWindow {
public:
    using ResizeCallback = std::function<void(int width, int height)>;
    using RenderCallback = std::function<void()>; // called while FBO is bound so Engine can render into it

    explicit SpxWindow(const WindowConfig& config);
    ~SpxWindow();

    // Non-copyable
    SpxWindow(const SpxWindow&) = delete;
    SpxWindow& operator=(const SpxWindow&) = delete;

    void SetIcon(GLFWwindow* window); // set window icon from image file

    // #### ImGui integration requires access to the GLFWwindow* ####
    void SetUpImGui(GLFWwindow* window);
    void NewImguiFrame(GLFWwindow* window);

    // Docking control
    void SetEnableDocking(bool enabled);
    bool GetEnableDocking() const;
    void MainDockSpace(bool* p_open); // docking space

    // Main scene window + framebuffer helpers
    void MainSceneWindow(GLFWwindow* window); // drawing UI window that will display the FBO
    void Creat_FrameBuffer();                 // create or recreate the framebuffer using current size
    void Bind_Framebuffer();                  // bind the offscreen FBO for rendering
    void Unbinde_Frambuffer();                // unbind (return to default framebuffer)
    void Rescale_frambuffer(float width, float height); // recreate at given pixel size

    // Render callback management
    void SetRenderCallback(RenderCallback cb);
    // Framebuffer info accessors (pixel size and color texture id)
    int GetFramebufferWidth() const;
    int GetFramebufferHeight() const;
    GLuint GetFramebufferColorTexture() const; 

	void RenderImGui(GLFWwindow* window); // finish ImGui frame and render
    void ImGuiShutdown();

    bool IsValid() const;
    bool ShouldClose() const;
    void PollEvents();     // calls glfwPollEvents()
    void SwapBuffers();    // swaps this window's buffers

    int GetWidth() const;
    int GetHeight() const;

    void SetVSync(bool enabled);
    void SetResizeCallback(ResizeCallback cb);

    // Return opaque native window pointer (GLFWwindow*)
    void* GetNativeWindow() const;

private:
    // callback
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    // glfw window + config
    GLFWwindow* window = nullptr;
    WindowConfig m_config;
    ResizeCallback m_resizeCallback = nullptr;

    // Framebuffer resources for the main scene view
    GLuint m_fbo = 0;
	GLuint m_fboColor = 0; // color texture
    GLuint m_fboDepth = 0;
    int m_fbWidth = 0;
    int m_fbHeight = 0;

    // Render callback called while FBO is bound
    RenderCallback m_renderCallback = nullptr;

    // Simple refcount so multiple SpxWindow instances don't re-init/terminate GLFW
    static int s_glfwRefCount; // static member declaration

    bool m_enableDocking = true;
};

enum FontIndex : int {
    REG_FONT_INDEX = 0,
    SMALL_FONT_INDEX,
    TINY_FONT_INDEX,
};
#define ICON_MIN_FA 0xe005
#define ICON_MAX_16_FA 0xf8ff
#define ICON_MAX_FA 0xf8ff


const float FONT_SIZE = 28.0f; // comic
const float SMALL_FONT_SIZE = 13.0f;
const float TINY_FONT_SIZE = 10.0f;

constexpr const char* FONT_PATH_MAIN_REL = "assets/fonts/comic.ttf";

//constexpr const char* FONT_PATH_MAIN =  "C:/Users/marty/Desktop/SPXEngine/SPXEngine/SpxEngine/engine/assets/fonts/comic.ttf";
constexpr const char* ROBOTO_REG_PATH = "fonts/Roboto-Regular.ttf";
constexpr const char* FA_REG_PATH = "fonts/FA-Regular-400.otf";
constexpr const char* FA_SOLID_PATH = "fonts/FA-Solid-900.otf";

constexpr const char* ICON_PATH = "assets/textures/icons/icon.png";
