#define STB_IMAGE_IMPLEMENTATION
#include "window.h"
#include "imgui\imgui.h"
#include <imgui\imgui_impl_glfw.h>
#include <imgui\imgui_impl_opengl3.h>
#include "stb/stb_image.h"
#include "../include/asset_path.h" // for GetAssetPath
#include "../include/engine.h"
#include "log.h"
#include <iostream>

// initialize static refcount
int SpxWindow::s_glfwRefCount = 0;
// Framebuffer size callback to handle window resizing events
void SpxWindow::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    void* up = glfwGetWindowUserPointer(window);
    if (!up) return;
    SpxWindow* self = reinterpret_cast<SpxWindow*>(up);
    if (self) {
        // update stored sizes (we don't expose setters here, but we can rely on GLFW queries)
        if (self->m_resizeCallback) self->m_resizeCallback(width, height);
    }
}

SpxWindow::SpxWindow(const WindowConfig& config)
    : m_config(config)
{
    if (s_glfwRefCount == 0) {
        if (!glfwInit()) {
            //std::cerr << "SpxWindow: Failed to initialize GLFW\n";
			LOG_DEBUG("SpxWindow: Failed to initialize GLFW");
            return;
        }
    }
    ++s_glfwRefCount;

    // Default to OpenGL 3.3 core; adjust if you need different version/profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, m_config.resizable ? GLFW_TRUE : GLFW_FALSE);

    window = glfwCreateWindow(m_config.width, m_config.height, m_config.title, nullptr, nullptr);
    if (!window) {
        //std::cerr << "SpxWindow: Failed to create GLFW window\n";
		LOG_DEBUG("SpxWindow: Failed to create GLFW window");
        // decrement refcount and terminate if this was the only user
        --s_glfwRefCount;
        if (s_glfwRefCount == 0) glfwTerminate();
        return;
    }

    // store pointer to this for callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // Make context current here so a caller can initialize GL loader (glad) immediately
    glfwMakeContextCurrent(window);

    // Set vsync as requested
    SetVSync(m_config.vsync);
}

void SpxWindow::SetIcon(GLFWwindow* window)
{
    std::string iconPath = GetAssetPath(ICON_PATH);
    GLFWimage images[1];
    images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4); // rgba = png
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(images[0].pixels);
}
// ############################################# ImGui Set up #############################################
void SpxWindow::SetUpImGui(GLFWwindow* window) {
    //ImGui set up
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    //io.KeyMap[ImGuiKey_H] = GLFW_KEY_HOME;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
  //  ImGui_ImplOpenGL3_Init(SHADER_VERSION);
    const char* glsl_version = "#version 460 core";
    ImGui_ImplOpenGL3_Init(glsl_version);


    // Make it possible to use Icons From FontAwesome5
    ImFontConfig fontconfig;
    fontconfig.MergeMode = true;
    fontconfig.PixelSnapH = true;
    static const ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    fontconfig.GlyphOffset = ImVec2(0.0f, 1.0f);
    std::string fontPath = GetAssetPath(FONT_PATH_MAIN_REL);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), FONT_SIZE);

    // set the fonts
    //io.Fonts->AddFontFromFileTTF(FONT_PATH_MAIN_REL, FONT_SIZE); // comic sans font type
    //io.Fonts->AddFontFromFileTTF(FONT_PATH_MAIN, FONT_SIZE); // comic sans font type
    //io.Fonts->AddFontFromFileTTF(ROBOTO_REG_PATH, FONT_SIZE); // sandard font type
   // io.Fonts->AddFontFromFileTTF(FA_SOLID_PATH, FONT_SIZE, &fontconfig, ranges);
}
void SpxWindow::NewImguiFrame(GLFWwindow* window)
{
    // New Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void SpxWindow::SetEnableDocking(bool enabled)
{
    m_enableDocking = enabled;
}

bool SpxWindow::GetEnableDocking() const
{
    return m_enableDocking;
}

void SpxWindow::MainDockSpace(bool* p_open)  
{
   
    if (m_enableDocking) { // Docking on or off
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;// I changed this so my scean shows up on start up

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); // you can add a bit of padding  
        ImGui::Begin("DockSpace Demo", p_open, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);


        // Submit the DockSpace to the ini file
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        ImGui::End();
    }
}

void SpxWindow::RenderImGui(GLFWwindow* window)
{
	ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle multiple viewports / platform windows
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}
void SpxWindow::ImGuiShutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
// ############################################# End ImGui Set up #############################################

SpxWindow::~SpxWindow() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    --s_glfwRefCount;
    if (s_glfwRefCount == 0) {
        glfwTerminate();
    }
}

bool SpxWindow::IsValid() const {
    return window != nullptr;
}

bool SpxWindow::ShouldClose() const {
    return !IsValid() || glfwWindowShouldClose(window);
}

void SpxWindow::PollEvents() {
    // Delegate to GLFW (safe to call even if other code also polls)
    glfwPollEvents();
}

void SpxWindow::SwapBuffers() {
    if (window) glfwSwapBuffers(window);
}

int SpxWindow::GetWidth() const {
    if (!window) return 0;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return w;
}

int SpxWindow::GetHeight() const {
    if (!window) return 0;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return h;
}

void SpxWindow::SetVSync(bool enabled) {
    if (!window) return;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(enabled ? 1 : 0);
    m_config.vsync = enabled;
}

void SpxWindow::SetResizeCallback(ResizeCallback cb) {
    m_resizeCallback = cb;
}

void* SpxWindow::GetNativeWindow() const {
    return reinterpret_cast<void*>(window);
}



