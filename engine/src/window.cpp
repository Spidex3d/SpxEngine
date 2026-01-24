#define STB_IMAGE_IMPLEMENTATION
#include "window.h"
#include "imgui\imgui.h"
#include <imgui\imgui_impl_glfw.h>
#include <imgui\imgui_impl_opengl3.h>
#include "stb/stb_image.h"
#include "../include/asset_path.h" // for GetAssetPath
#include "../include/globalVar.h"
#include "../include/entity.h"
#include "log.h"
#include <iostream>

// initialize static refcount
int SpxWindow::s_glfwRefCount = 0;

// small helper to destroy existing framebuffer resources
static void DestroyFBO(GLuint& fbo, GLuint& color, GLuint& depth) {
    if (depth) { glDeleteRenderbuffers(1, &depth); depth = 0; }
    if (color) { glDeleteTextures(1, &color); color = 0; }
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
}

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
            LOG_DEBUG("SpxWindow: Failed to initialize GLFW");
            return;
        }
    }
    ++s_glfwRefCount;

    // Default to OpenGL 4.6 core; adjust if you need different version/profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, m_config.resizable ? GLFW_TRUE : GLFW_FALSE);

    window = glfwCreateWindow(m_config.width, m_config.height, m_config.title, nullptr, nullptr);
    if (!window) {
        LOG_DEBUG("SpxWindow: Failed to create GLFW window");
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
    // ImGui set up
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // enable viewports/docking depending on flag
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    if (m_enableDocking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char* glsl_version = "#version 460 core";
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Fonts
    ImFontConfig fontconfig;
    fontconfig.MergeMode = true;
    fontconfig.PixelSnapH = true;
    static const ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    fontconfig.GlyphOffset = ImVec2(0.0f, 1.0f);
    std::string fontPath = GetAssetPath(FONT_PATH_MAIN_REL);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), FONT_SIZE);
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
    if (m_enableDocking) {
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

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
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", p_open, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        ImGui::End();
    }
}

// ######### The main Imgui window for rendering the scene #########
void SpxWindow::MainSceneWindow(GLFWwindow* window)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::Begin("Main scene");

    // Available size in UI units
    const float window_width = ImGui::GetContentRegionAvail().x;
    const float window_height = ImGui::GetContentRegionAvail().y;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGuiIO& io = ImGui::GetIO();

    // Determine desired framebuffer pixel size (account for HiDPI scale)
    int desired_w = static_cast<int>(window_width * io.DisplayFramebufferScale.x);
    int desired_h = static_cast<int>(window_height * io.DisplayFramebufferScale.y);

    // Recreate framebuffer if size changed or not created yet
    if (desired_w > 0 && desired_h > 0) {
        if (desired_w != m_fbWidth || desired_h != m_fbHeight || m_fbo == 0) {
            Rescale_frambuffer((float)desired_w, (float)desired_h);
        }
    }

    // Static objects to persist between frames:
    static Entity s_entity;
    static std::vector<std::unique_ptr<GameObj>> s_entVector;
    static int s_currentIndex = 0;
    static int s_planeObjIdx = 0;

    // If we have an FBO, bind it, clear and render scene into it.
    if (m_fbo) {
        // Prepare a simple "centered" camera/projection (no complex camera yet).
        // Plane geometry is approximately within [-0.5, 0.5] in X/Y, so use a small orthographic view.
        glm::mat4 view = glm::mat4(1.0f);

        float aspect = (m_fbHeight > 0) ? (float)m_fbWidth / (float)m_fbHeight : 1.0f;
        float orthoHalfHeight = 1.0f; // adjust to zoom in/out
        float orthoHalfWidth = orthoHalfHeight * aspect;
        glm::mat4 projection = glm::ortho(-orthoHalfWidth, orthoHalfWidth, -orthoHalfHeight, orthoHalfHeight, -10.0f, 10.0f);

        // Track if a plane was added during the call so we can bump the index
        size_t prevSize = s_entVector.size();

        // Bind FBO, set viewport and clear
        Bind_Framebuffer();
        glClearColor(0.12f, 0.15f, 0.18f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Call into your entity renderer (it will create planes if ShouldAddPlane is true)
        s_entity.RenderPlane(view, projection, s_entVector, s_currentIndex, s_planeObjIdx);

        // Unbind and restore default framebuffer
        Unbinde_Frambuffer();

        // If a new entity was added, increment current index so IDs remain unique
        if (s_entVector.size() > prevSize) {
            ++s_currentIndex;
        }

        // Draw the resulting texture inside the ImGui window (flip vertical for GL)
        ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)m_fboColor,
            ImVec2(pos.x, pos.y),
            ImVec2(pos.x + window_width, pos.y + window_height),
            ImVec2(0, 1), ImVec2(1, 0));
    }
    else {
        // fallback: draw empty box or placeholder text
        ImGui::TextWrapped("Frame buffer not initialized.");
    }

    // Existing UI: detect right-click popup menu
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("RightClickMenu");
    }

    if (ImGui::BeginPopup("RightClickMenu"))
    {
        if (ImGui::BeginMenu("Add a new mesh")) {
            if (ImGui::MenuItem("Plane")) {
                ShouldAddPlane = true;
            }
            // other menu items...
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add a new Light")) {
            if (ImGui::MenuItem("Sun Light")) {}
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
    ImGui::PopStyleVar();
    //ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    //ImGui::Begin("Main scene");

    //// Available size in UI units
    //const float window_width = ImGui::GetContentRegionAvail().x;
    //const float window_height = ImGui::GetContentRegionAvail().y;

    //ImVec2 pos = ImGui::GetCursorScreenPos();
    //ImGuiIO& io = ImGui::GetIO();

    //// Determine desired framebuffer pixel size (account for HiDPI scale)
    //int desired_w = static_cast<int>(window_width * io.DisplayFramebufferScale.x);
    //int desired_h = static_cast<int>(window_height * io.DisplayFramebufferScale.y);

    //// Recreate framebuffer if size changed or not created yet
    //if (desired_w > 0 && desired_h > 0) {
    //    if (desired_w != m_fbWidth || desired_h != m_fbHeight || m_fbo == 0) {
    //        Rescale_frambuffer((float)desired_w, (float)desired_h);
    //    }
    //}

    //// If we have an FBO, bind it, clear and render scene into it.
    //if (m_fbo) {
    //    // Bind FBO and render your scene here
    //    Bind_Framebuffer();

    //    // Clear with a sensible default (you can use engine clear color instead)
    //    glClearColor(0.15f, 0.15f, 0.16f, 1.0f);
    //    glEnable(GL_DEPTH_TEST);
    //    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //    // TODO: call your renderer here. For example:
    //    // Engine::GetInstance()->RenderSceneToFBO(m_fbWidth, m_fbHeight);
    //    // or call your Entity::RenderPlane(...) with appropriate camera/view/projection.

    //    // Done rendering to FBO
    //    Unbinde_Frambuffer();

    //    // Draw the resulting texture inside the ImGui window.
    //    // Use AddImage so it fills the content region exactly.
    //    ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)m_fboColor,
    //        ImVec2(pos.x, pos.y),
    //        ImVec2(pos.x + window_width, pos.y + window_height),
    //        ImVec2(0, 1), ImVec2(1, 0)); // flip vertically for GL texture coords
    //}
    //else {
    //    // fallback: draw empty box or placeholder text
    //    ImGui::TextWrapped("Frame buffer not initialized.");
    //}

    //// Detect right-click for popup menu (existing UI code)
    //if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    //{
    //    ImGui::OpenPopup("RightClickMenu");
    //}

    //if (ImGui::BeginPopup("RightClickMenu"))
    //{
    //    if (ImGui::BeginMenu("Add a new mesh")) {
    //        if (ImGui::MenuItem("Plane")) {
    //            ShouldAddPlane = true;
    //        }
    //        // other menu items...
    //        ImGui::EndMenu();
    //    }

    //    if (ImGui::BeginMenu("Add a new Light")) {
    //        if (ImGui::MenuItem("Sun Light")) {}
    //        ImGui::EndMenu();
    //    }

    //    ImGui::EndPopup();
    //}

    //ImGui::End();
    //ImGui::PopStyleVar();
}

// Create or recreate the FBO using current window size if needed
void SpxWindow::Creat_FrameBuffer()
{
    int w = GetWidth();
    int h = GetHeight();
    if (w <= 0 || h <= 0) return;
    Rescale_frambuffer((float)w, (float)h);
}

// Bind offscreen FBO for rendering. Sets viewport to framebuffer size.
void SpxWindow::Bind_Framebuffer()
{
    if (!m_fbo) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
}

// Unbind FBO and restore default framebuffer / viewport to window size
void SpxWindow::Unbinde_Frambuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    int w = GetWidth();
    int h = GetHeight();
    if (w > 0 && h > 0) {
        glViewport(0, 0, w, h);
    }
}

// Recreate framebuffer at requested pixel size (width, height in pixels)
// This deletes previous attachments safely and creates a color texture + depth renderbuffer.
void SpxWindow::Rescale_frambuffer(float width, float height)
{
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);
    if (w <= 0 || h <= 0) return;

    // If same size, nothing to do
    if (m_fbo && m_fbWidth == w && m_fbHeight == h) return;

    // Destroy old attachments (if any)
    DestroyFBO(m_fbo, m_fboColor, m_fboDepth);

    // Create new color texture
    glGenTextures(1, &m_fboColor);
    glBindTexture(GL_TEXTURE_2D, m_fboColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Optional: clamp
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create depth+stencil renderbuffer
    glGenRenderbuffers(1, &m_fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create framebuffer and attach
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboColor, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_fboDepth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_WARNING("Failed to create framebuffer: status=0x%x", (unsigned)status);
        DestroyFBO(m_fbo, m_fboColor, m_fboDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_fbWidth = m_fbHeight = 0;
        return;
    }

    // success
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_fbWidth = w;
    m_fbHeight = h;
    LOG_INFO("Created FBO %u (color=%u depth=%u) size=%dx%d", (unsigned)m_fbo, (unsigned)m_fboColor, (unsigned)m_fboDepth, w, h);
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
    // destroy framebuffer resources when shutting down
    DestroyFBO(m_fbo, m_fboColor, m_fboDepth);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
// ############################################# End ImGui Set up #############################################

SpxWindow::~SpxWindow() {
    // destroy FBO if still present
    DestroyFBO(m_fbo, m_fboColor, m_fboDepth);

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





//#include "window.h"
//#include "imgui\imgui.h"
//#include <imgui\imgui_impl_glfw.h>
//#include <imgui\imgui_impl_opengl3.h>
//#include "stb/stb_image.h"
//#include "../include/asset_path.h" // for GetAssetPath
//#include "../include/globalVar.h"
//#include "../include/entity.h"
//#include "log.h"
//#include <iostream>
//
//// initialize static refcount
//int SpxWindow::s_glfwRefCount = 0;
//// Framebuffer size callback to handle window resizing events
//void SpxWindow::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
//    void* up = glfwGetWindowUserPointer(window);
//    if (!up) return;
//    SpxWindow* self = reinterpret_cast<SpxWindow*>(up);
//    if (self) {
//        // update stored sizes (we don't expose setters here, but we can rely on GLFW queries)
//        if (self->m_resizeCallback) self->m_resizeCallback(width, height);
//    }
//}
//
//SpxWindow::SpxWindow(const WindowConfig& config)
//    : m_config(config)
//{
//    if (s_glfwRefCount == 0) {
//        if (!glfwInit()) {
//            //std::cerr << "SpxWindow: Failed to initialize GLFW\n";
//			LOG_DEBUG("SpxWindow: Failed to initialize GLFW");
//            return;
//        }
//    }
//    ++s_glfwRefCount;
//
//    // Default to OpenGL 3.3 core; adjust if you need different version/profile
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//    glfwWindowHint(GLFW_RESIZABLE, m_config.resizable ? GLFW_TRUE : GLFW_FALSE);
//
//    window = glfwCreateWindow(m_config.width, m_config.height, m_config.title, nullptr, nullptr);
//    if (!window) {
//        //std::cerr << "SpxWindow: Failed to create GLFW window\n";
//		LOG_DEBUG("SpxWindow: Failed to create GLFW window");
//        // decrement refcount and terminate if this was the only user
//        --s_glfwRefCount;
//        if (s_glfwRefCount == 0) glfwTerminate();
//        return;
//    }
//
//    // store pointer to this for callbacks
//    glfwSetWindowUserPointer(window, this);
//    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
//
//    // Make context current here so a caller can initialize GL loader (glad) immediately
//    glfwMakeContextCurrent(window);
//
//    // Set vsync as requested
//    SetVSync(m_config.vsync);
//}
//
//void SpxWindow::SetIcon(GLFWwindow* window)
//{
//    std::string iconPath = GetAssetPath(ICON_PATH);
//    GLFWimage images[1];
//    images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4); // rgba = png
//    glfwSetWindowIcon(window, 1, images);
//    stbi_image_free(images[0].pixels);
//}
//// ############################################# ImGui Set up #############################################
//void SpxWindow::SetUpImGui(GLFWwindow* window) {
//    //ImGui set up
//    IMGUI_CHECKVERSION();
//    ImGui::CreateContext();
//    ImGuiIO& io = ImGui::GetIO(); (void)io;
//    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
//   // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
//    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
//    //io.KeyMap[ImGuiKey_H] = GLFW_KEY_HOME;
//    if (m_enableDocking) {
//        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
//        // optional: enable viewports only if you want multi-window OS viewports
//        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
//    }
//
//
//    ImGui::StyleColorsDark();
//
//    ImGui_ImplGlfw_InitForOpenGL(window, true);
//  //  ImGui_ImplOpenGL3_Init(SHADER_VERSION);
//    const char* glsl_version = "#version 460 core";
//    ImGui_ImplOpenGL3_Init(glsl_version);
//
//
//    // Make it possible to use Icons From FontAwesome5
//    ImFontConfig fontconfig;
//    fontconfig.MergeMode = true;
//    fontconfig.PixelSnapH = true;
//    static const ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
//
//    fontconfig.GlyphOffset = ImVec2(0.0f, 1.0f);
//    std::string fontPath = GetAssetPath(FONT_PATH_MAIN_REL);
//    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), FONT_SIZE);
//
//    // set the fonts
//    //io.Fonts->AddFontFromFileTTF(FONT_PATH_MAIN_REL, FONT_SIZE); // comic sans font type
//    //io.Fonts->AddFontFromFileTTF(FONT_PATH_MAIN, FONT_SIZE); // comic sans font type
//    //io.Fonts->AddFontFromFileTTF(ROBOTO_REG_PATH, FONT_SIZE); // sandard font type
//   // io.Fonts->AddFontFromFileTTF(FA_SOLID_PATH, FONT_SIZE, &fontconfig, ranges);
//}
//void SpxWindow::NewImguiFrame(GLFWwindow* window)
//{
//    // New Frame
//    ImGui_ImplOpenGL3_NewFrame();
//    ImGui_ImplGlfw_NewFrame();
//    ImGui::NewFrame();
//}
//
//void SpxWindow::SetEnableDocking(bool enabled)
//{
//    m_enableDocking = enabled;
//}
//
//bool SpxWindow::GetEnableDocking() const
//{
//    return m_enableDocking;
//}
//
//void SpxWindow::MainDockSpace(bool* p_open)  
//{
//   
//    if (m_enableDocking) { // Docking on or off
//        static bool opt_fullscreen = true;
//        static bool opt_padding = false;
//        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;// I changed this so my scean shows up on start up
//
//        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
//        if (opt_fullscreen)
//        {
//            const ImGuiViewport* viewport = ImGui::GetMainViewport();
//            ImGui::SetNextWindowPos(viewport->WorkPos);
//            ImGui::SetNextWindowSize(viewport->WorkSize);
//            ImGui::SetNextWindowViewport(viewport->ID);
//            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
//            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
//            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
//            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
//        }
//        else
//        {
//            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
//        }
//
//        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
//            window_flags |= ImGuiWindowFlags_NoBackground;
//
//        if (!opt_padding)
//            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); // you can add a bit of padding  
//        ImGui::Begin("DockSpace Demo", p_open, window_flags);
//        if (!opt_padding)
//            ImGui::PopStyleVar();
//
//        if (opt_fullscreen)
//            ImGui::PopStyleVar(2);
//
//
//        // Submit the DockSpace to the ini file
//        ImGuiIO& io = ImGui::GetIO();
//        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
//        {
//            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
//            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
//        }
//
//        ImGui::End();
//    }
//}
//// ######### The main Imgui window for rendering the scene #########
//void SpxWindow::MainSceneWindow(GLFWwindow* window)
//{
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
//    ImGui::Begin("Main scene");
//
//    // ##
//    const float window_width = ImGui::GetContentRegionAvail().x;
//    const float window_height = ImGui::GetContentRegionAvail().y;
//
//    //Rescale_frambuffer(window_width, window_height);
//    //glViewport(0, 0, window_width, window_height);
//
//    ImVec2 pos = ImGui::GetCursorScreenPos();
//
//    /*ImGui::GetWindowDrawList()->AddImage((void*)main_scene_texture_id, ImVec2(pos.x, pos.y),
//        ImVec2(pos.x + window_width, pos.y + window_height), ImVec2(0, 1), ImVec2(1, 0));*/
//    // Detect right-click for popup menu 
//    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
//    {
//        ImGui::OpenPopup("RightClickMenu");
//    }
//    // Create the right-click popup menu 
//    if (ImGui::BeginPopup("RightClickMenu"))
//    {
//
//        //if (ImGui::BeginMenu("New Mesh", &ShouldAddCube)) { // as far as i can tell ShouldAddCube is not nedded
//        if (ImGui::BeginMenu("Add a new mesh")) {
//            if (ImGui::MenuItem("gltf Model File")) {
//                // set ShouldAddglTFModel to true then add gltf file to the tree
//               // ShouldAddglTFModel = true;
//                //dialogType = false;   // sets is textured or gltf file for the opendialog box
//
//            }
//            if (ImGui::MenuItem(".Obj File")) {
//                // set ShouldAddObjModel to true then add obj file to the tree
//                //ShouldAddObjModel = true;
//                //dialogType = false;   // sets is textured or obj file for the opendialog box
//
//            }
//            if (ImGui::MenuItem("Cube")) {
//                // set ShouldAddCube to true then add cube to the tree
//                //ShouldAddCube = true;
//                //dialogType = true;   // sets dialogType is textured or obj file for the opendialog box
//
//            }
//            if (ImGui::MenuItem("Plane")) {
//                // set ShouldAddPlane to true then add plane to the tree
//                 ShouldAddPlane = true;
//				 
//            }
//            if (ImGui::MenuItem("Circle")) {}
//            if (ImGui::MenuItem("Sphere")) {
//               // ShouldAddSphere = true;
//                //dialogType = true;
//            }
//            if (ImGui::MenuItem("Cylinder")) {}
//            if (ImGui::MenuItem("Torus")) {}
//            if (ImGui::MenuItem("Grid")) {}
//            if (ImGui::MenuItem("Cone")) {}
//            if (ImGui::MenuItem("Pyramid")) {
//                //ShouldAddPyramid = true;
//            }
//
//            ImGui::EndMenu();
//        }
//        if (ImGui::BeginMenu("Add a new Light")) {
//            if (ImGui::MenuItem("Sun Light")) {}
//            if (ImGui::MenuItem("Point Light")) {}
//            if (ImGui::MenuItem("Spot Light")) {}
//            if (ImGui::MenuItem("Area Light")) {}
//
//            ImGui::EndMenu();
//        }
//        if (ImGui::BeginMenu("Add a Terrain")) {
//            if (ImGui::MenuItem("Terrain")) {}
//            if (ImGui::MenuItem("Water")) {}
//            if (ImGui::MenuItem("Floor")) {}
//
//            ImGui::EndMenu();
//        }
//
//        ImGui::EndPopup();
//    }
//
//    /* float relativeX;
//     float relativeY;*/
//
//     // ##### Mouse picking
//    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
//    {
//        /* ImVec2 mousePos = ImGui::GetMousePos();
//         ImVec2 windowPos = ImGui::GetWindowPos();
//
//         relativeX = mousePos.x - windowPos.x;
//         relativeY = mousePos.y - windowPos.y;
//
//         std::cout << "The mouse x (relative): " << relativeX << " The mouse y (relative): " << relativeY << std::endl;*/
//
//         // this needs to go off and find the object we just clicked
//       // SelectedObject = true;
//    }
//    //#####
//
//    ImGui::Text("Right-click for popup Menu.");
//    ImGui::End();
//    ImGui::PopStyleVar();
//
//    
//}
//// Needed for framebuffer creation to draw in main ImGui window
//void SpxWindow::Creat_FrameBuffer()
//{
//	
//}
//// We will need this later for texture rendering
//void SpxWindow::Bind_Framebuffer()
//{
//}
//// We will to unbind later
//void SpxWindow::Unbinde_Frambuffer()
//{
//}
//// Rescale framebuffer on window resize
//void SpxWindow::Rescale_frambuffer(float width, float height)
//{
//}
//
//void SpxWindow::RenderImGui(GLFWwindow* window)
//{
//	ImGui::Render();
//    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//
//    // Handle multiple viewports / platform windows
//    ImGuiIO& io = ImGui::GetIO();
//    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
//        GLFWwindow* backup_current_context = glfwGetCurrentContext();
//        ImGui::UpdatePlatformWindows();
//        ImGui::RenderPlatformWindowsDefault();
//        glfwMakeContextCurrent(backup_current_context);
//    }
//}
//void SpxWindow::ImGuiShutdown()
//{
//    ImGui_ImplOpenGL3_Shutdown();
//    ImGui_ImplGlfw_Shutdown();
//    ImGui::DestroyContext();
//}
//// ############################################# End ImGui Set up #############################################
//
//SpxWindow::~SpxWindow() {
//    if (window) {
//        glfwDestroyWindow(window);
//        window = nullptr;
//    }
//
//    --s_glfwRefCount;
//    if (s_glfwRefCount == 0) {
//        glfwTerminate();
//    }
//}
//
//bool SpxWindow::IsValid() const {
//    return window != nullptr;
//}
//
//bool SpxWindow::ShouldClose() const {
//    return !IsValid() || glfwWindowShouldClose(window);
//}
//
//void SpxWindow::PollEvents() {
//    // Delegate to GLFW (safe to call even if other code also polls)
//    glfwPollEvents();
//}
//
//void SpxWindow::SwapBuffers() {
//    if (window) glfwSwapBuffers(window);
//}
//
//int SpxWindow::GetWidth() const {
//    if (!window) return 0;
//    int w, h;
//    glfwGetFramebufferSize(window, &w, &h);
//    return w;
//}
//
//int SpxWindow::GetHeight() const {
//    if (!window) return 0;
//    int w, h;
//    glfwGetFramebufferSize(window, &w, &h);
//    return h;
//}
//
//void SpxWindow::SetVSync(bool enabled) {
//    if (!window) return;
//    glfwMakeContextCurrent(window);
//    glfwSwapInterval(enabled ? 1 : 0);
//    m_config.vsync = enabled;
//}
//
//void SpxWindow::SetResizeCallback(ResizeCallback cb) {
//    m_resizeCallback = cb;
//}
//
//void* SpxWindow::GetNativeWindow() const {
//    return reinterpret_cast<void*>(window);
//}



