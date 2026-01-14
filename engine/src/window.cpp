#include "window.h"
#include "imgui\imgui.h"
#include <imgui\imgui_impl_glfw.h>
#include <imgui\imgui_impl_opengl3.h>
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
        if (self->resizeCallback_) self->resizeCallback_(width, height);
    }
}

SpxWindow::SpxWindow(const WindowConfig& config)
    : config_(config)
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
    glfwWindowHint(GLFW_RESIZABLE, config_.resizable ? GLFW_TRUE : GLFW_FALSE);

    window = glfwCreateWindow(config_.width, config_.height, config_.title, nullptr, nullptr);
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
    SetVSync(config_.vsync);
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

    // set the fonts
    fontconfig.GlyphOffset = ImVec2(0.0f, 1.0f);
    io.Fonts->AddFontFromFileTTF(FONT_PATH_MAIN, FONT_SIZE); // comic sans font type
    io.Fonts->AddFontFromFileTTF(ROBOTO_REG_PATH, FONT_SIZE); // sandard font type
    io.Fonts->AddFontFromFileTTF(FA_SOLID_PATH, FONT_SIZE, &fontconfig, ranges);
}
void SpxWindow::NewImguiFrame(GLFWwindow* window)
{
    // New Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
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
    config_.vsync = enabled;
}

void SpxWindow::SetResizeCallback(ResizeCallback cb) {
    resizeCallback_ = cb;
}

void* SpxWindow::GetNativeWindow() const {
    return reinterpret_cast<void*>(window);
}




//#include "window.h"
//#include <GLFW/glfw3.h>
//#include <iostream>
//#include <cassert>
//#include "log.h"
//
//struct SpxWindow::Impl {
//    GLFWwindow* window = nullptr;
//    int width = 1280;
//    int height = 720;
//    WindowConfig config;
//    SpxWindow::ResizeCallback resizeCallback = nullptr;
//    static int s_glfwRefCount;
//};
//
//int SpxWindow::Impl::s_glfwRefCount = 0;
//
//static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
//    void* up = glfwGetWindowUserPointer(window);
//    if (!up) return;
//    SpxWindow::Impl* impl = reinterpret_cast<SpxWindow::Impl*>(up);
//    impl->width = width;
//    impl->height = height;
//    // update GL viewport here (host or engine will call glViewport when appropriate)
//    if (impl->resizeCallback) impl->resizeCallback(width, height);
//}
//
//SpxWindow::SpxWindow(const WindowConfig& config)
//    : impl_(new Impl())
//{
//    impl_->config = config;
//    impl_->width = config.width;
//    impl_->height = config.height;
//
//    if (Impl::s_glfwRefCount == 0) {
//        if (!glfwInit()) {
//            //std::cerr << "Failed to initialize GLFW\n";
//			LOG_DEBUG("Failed to initialize GLFW");
//            delete impl_;
//            impl_ = nullptr;
//            return;
//        }
//    }
//    ++Impl::s_glfwRefCount;
//
//    // Window hints (use OpenGL 3.3 core as default)
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//#ifdef _WIN32
//    // On Windows, set this to ensure compatibility with old compilers
//    // (no action needed normally)
//#endif
//    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
//
//    impl_->window = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);
//    if (!impl_->window) {
//        //std::cerr << "Failed to create GLFW window\n";
//		LOG_DEBUG("Failed to create GLFW window");
//        // decrement refcount and maybe terminate
//        --Impl::s_glfwRefCount;
//        if (Impl::s_glfwRefCount == 0) {
//            glfwTerminate();
//        }
//        delete impl_;
//        impl_ = nullptr;
//        return;
//    }
//
//    // Store pointer so callbacks can access Impl
//    glfwSetWindowUserPointer(impl_->window, impl_);
//    glfwSetFramebufferSizeCallback(impl_->window, FramebufferSizeCallback);
//
//    // Make context current here if you want to init GL immediately from here,
//    // otherwise the caller (engine) will call gladLoadGLLoader after making context current.
//    glfwMakeContextCurrent(impl_->window);
//    SetVSync(config.vsync);
//}
//
//SpxWindow::~SpxWindow() {
//    if (impl_) {
//        if (impl_->window) {
//            glfwDestroyWindow(impl_->window);
//            impl_->window = nullptr;
//        }
//        --Impl::s_glfwRefCount;
//        if (Impl::s_glfwRefCount == 0) {
//            glfwTerminate();
//        }
//        delete impl_;
//        impl_ = nullptr;
//    }
//}
//
//bool SpxWindow::IsValid() const {
//    return impl_ && impl_->window;
//}
//
//bool SpxWindow::ShouldClose() const {
//    return !IsValid() || glfwWindowShouldClose(impl_->window);
//}
//
//void SpxWindow::PollEvents() {
//    glfwPollEvents();
//}
//
//void SpxWindow::SwapBuffers() {
//    if (IsValid())
//        glfwSwapBuffers(impl_->window);
//}
//
//int SpxWindow::GetWidth() const {
//    return impl_ ? impl_->width : 0;
//}
//
//int SpxWindow::GetHeight() const {
//    return impl_ ? impl_->height : 0;
//}
//
//void SpxWindow::SetVSync(bool enabled) {
//    if (!IsValid()) return;
//    glfwMakeContextCurrent(impl_->window);
//    glfwSwapInterval(enabled ? 1 : 0);
//    impl_->config.vsync = enabled;
//}
//
//void SpxWindow::SetResizeCallback(ResizeCallback cb) {
//    if (impl_) impl_->resizeCallback = cb;
//}
//
//void* SpxWindow::GetNativeWindow() const {
//    return impl_ ? reinterpret_cast<void*>(impl_->window) : nullptr;
//}



