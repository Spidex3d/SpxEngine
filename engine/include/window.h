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

//const char* SHADER_VERSION = "#version 460";

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

                                         
constexpr const char* FONT_PATH_MAIN =  "fonts/comic.ttf";
constexpr const char* ROBOTO_REG_PATH = "fonts/Roboto-Regular.ttf";
constexpr const char* FA_REG_PATH =     "fonts/FA-Regular-400.otf";
constexpr const char* FA_SOLID_PATH =   "fonts/FA-Solid-900.otf";


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

    explicit SpxWindow(const WindowConfig& config);
    ~SpxWindow();

    // Non-copyable
    SpxWindow(const SpxWindow&) = delete;
    SpxWindow& operator=(const SpxWindow&) = delete;

	// #### ImGui integration requires access to the GLFWwindow* ####
    void SetUpImGui(GLFWwindow* window);
    void NewImguiFrame(GLFWwindow* window);
	void RenderImGui(GLFWwindow* window);
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
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* window = nullptr;
    WindowConfig config_;
    ResizeCallback resizeCallback_ = nullptr;

    // Simple refcount so multiple SpxWindow instances don't re-init/terminate GLFW
    static int s_glfwRefCount;
};
