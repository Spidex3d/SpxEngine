// engine.cpp
#include "engine.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui\imgui.h"
#include <imgui\imgui_impl_glfw.h>
#include <imgui\imgui_impl_opengl3.h>
#include "log.h"
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Engine::Engine() = default;
Engine::~Engine() { Shutdown(); }

bool Engine::Initialize(const EngineConfig& config) {
    m_config = config;

    window = std::make_unique<SpxWindow>(config.windowConfig);
    if (!window || !window->IsValid()) {
        LOG_DEBUG("Engine: Failed to create window");
        window.reset();
        return false;
    }

    void* native = window->GetNativeWindow();
    if (!native) {
        LOG_DEBUG("Engine: no native window");
        return false;
    }
    glfwwindow = reinterpret_cast<GLFWwindow*>(native);
    glfwMakeContextCurrent(glfwwindow);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_DEBUG("Failed to initialize GLAD");
        return false;
    }
    // Create Input helper now that we have a native window for keyboard/mouse input
    m_input = std::make_unique<Input>(glfwwindow);
    if (m_input->HasKeyboardAttached()) {
        LOG_INFO("Keyboard input initialized");
    }
    else {
        LOG_WARNING("No keyboard detected; input may not work");
    }

    // ##################  ImGui initialization could go here  ####################
    // apply docking preference to window so ImGui is initialized with correct flags
    window->SetEnableDocking(config.enableDocking);

    if (config.enableImGui) {
        window->SetUpImGui(glfwwindow);
        LOG_INFO("ImGui initialized");
    }

    // ##################  END ImGui initialization  ####################

    int w = window->GetWidth();
    int h = window->GetHeight();
    glViewport(0, 0, w, h);

    glEnable(GL_DEPTH_TEST);
    glClearColor(config.clearColor[0], config.clearColor[1], config.clearColor[2], config.clearColor[3]);

    window->SetResizeCallback([this](int width, int height) {
        glViewport(0, 0, width, height);
    });

    // Create the engine-owned Entity and entity vector
    m_entity = std::make_unique<Entity>();
    m_entities.clear();
    m_currentEntityIndex = 0;
    m_planeObjIdx = 0;

    // Register a render callback with the window so it can call into Engine while the FBO is bound.
    // The callback computes a simple centered orthographic projection and calls Entity::RenderPlane.
    window->SetRenderCallback([this]() {
        int fbw = window->GetFramebufferWidth();
        int fbh = window->GetFramebufferHeight();
        if (fbw <= 0 || fbh <= 0) return;

        glm::mat4 view = glm::mat4(1.0f);
        float aspect = (fbh > 0) ? static_cast<float>(fbw) / static_cast<float>(fbh) : 1.0f;
        float orthoHalfHeight = 1.0f; // adjust to zoom in/out
        float orthoHalfWidth = orthoHalfHeight * aspect;
        glm::mat4 projection = glm::ortho(-orthoHalfWidth, orthoHalfWidth, -orthoHalfHeight, orthoHalfHeight, -10.0f, 10.0f);

        if (m_entity) {
            m_entity->RenderPlane(view, projection, m_entities, m_currentEntityIndex, m_planeObjIdx);
        }
    });

    m_running = true;
    m_lastTime = std::chrono::steady_clock::now();
    LOG_INFO("Engine initialized successfully");
    return true;
}

void Engine::Run() {
    window->SetIcon(glfwwindow);

    using clock = std::chrono::steady_clock;
    bool showGui = true; // persistent for the run session

    while (m_running && window && !window->ShouldClose()) {
        auto now = clock::now();
        std::chrono::duration<float> delta = now - m_lastTime;
        m_lastTime = now;
        float dt = delta.count();

        // 1) Poll events / update input first
        if (m_input) {
            m_input->Update();   // calls glfwPollEvents()
            if (m_input->IsKeyPressed(GLFW_KEY_ESCAPE)) {
                if (glfwwindow) glfwSetWindowShouldClose(glfwwindow, true);
            }
        }
        else {
            window->PollEvents();
        }


        // 2) Start ImGui frame (only if enabled)
        if (m_config.enableImGui) {
            window->NewImguiFrame(glfwwindow);

            // Ensure the dockspace exists before other windows so they can dock into it
            window->MainDockSpace(nullptr);

            // 3) Build GUI (only add windows when shown)
            if (showGui) {
                ImGui::Begin("Editor");

                // Runtime docking toggle (reflects current state and updates ImGui IO)
                bool dockingEnabled = window->GetEnableDocking();
                if (ImGui::Checkbox("Enable Docking", &dockingEnabled)) {
                    window->SetEnableDocking(dockingEnabled);
                    ImGuiIO& io = ImGui::GetIO();
                    if (dockingEnabled) {
                        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
                    }
                    else {
                        io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
                    }
                }

                if (ImGui::Button(showGui ? "Play (stop GUI)" : "Stop (show GUI)")) {
                    showGui = !showGui;
                }
                ImGui::Text("FPS: %.1f", 1.0f / std::max(0.00001f, dt));
                ImGui::End();
            }

            // Draw the MainSceneWindow which will call the registered render callback while FBO is bound
            window->MainSceneWindow(glfwwindow);
        }

        // 4) Update / render your scene AFTER building UI so UI overlays on top
        Tick(dt);

        // 5) Finish ImGui frame and render draw data (always do this if ImGui was started)
        if (m_config.enableImGui) {
            window->RenderImGui(glfwwindow); // this calls ImGui::Render() internally
        }

        // 6) Present
        window->SwapBuffers();
    }

    m_running = false;

}

void Engine::Tick(float dt) {
    (void)dt;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Engine::RequestExit() { m_running = false; }
bool Engine::IsRunning() const { return m_running; }
SpxWindow* Engine::GetWindow() { return window.get(); }

void Engine::Shutdown() {

    // Shutdown ImGui first (so it doesn't try to use destroyed GL resources)
    if (m_config.enableImGui && window) {
        window->ImGuiShutdown();
        LOG_INFO("ImGui shutdown successfully");
    }

    // clean up in reverse order
    m_input.reset();
    m_entity.reset();
    m_entities.clear();
    if (window) {
        window.reset();
    }

    m_running = false;
    LOG_INFO("Engine shutdown");
}





//// engine.cpp
//#include "engine.h"
//#include <glad/glad.h>
//#include <GLFW/glfw3.h>
//#include "imgui\imgui.h"
//#include <imgui\imgui_impl_glfw.h>
//#include <imgui\imgui_impl_opengl3.h>
//#include "log.h"
//#include <iostream>
//
//Engine::Engine() = default;
//Engine::~Engine() { Shutdown(); }
//
//bool Engine::Initialize(const EngineConfig& config) {
//    m_config = config;
//
//    window = std::make_unique<SpxWindow>(config.windowConfig);
//    if (!window || !window->IsValid()) {
//        LOG_DEBUG("Engine: Failed to create window");
//        window.reset();
//        return false;
//    }
//
//    void* native = window->GetNativeWindow();
//    if (!native) {
//        LOG_DEBUG("Engine: no native window");
//        return false;
//    }
//    glfwwindow = reinterpret_cast<GLFWwindow*>(native);
//    glfwMakeContextCurrent(glfwwindow);
//
//    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
//        LOG_DEBUG("Failed to initialize GLAD");
//        return false;
//    }
//	// Create Input helper now that we have a native window for keyboard/mouse input
//    m_input = std::make_unique<Input>(glfwwindow);
//    if (m_input->HasKeyboardAttached()) {
//        LOG_INFO("Keyboard input initialized");
//    }
//    else {
//        LOG_WARNING("No keyboard detected; input may not work");
//    }
//
//	// ##################  ImGui initialization could go here  ####################
//    //window = std::make_unique<SpxWindow>(config.windowConfig);
//    //window->SetEnableDocking(config.enableDocking); // apply initial value
//
//
//	if (config.enableImGui) {
//		window->SetUpImGui(glfwwindow);
//		LOG_INFO("ImGui initialized");
//	}
//
//	// ##################  END ImGui initialization  ####################
//
//    int w = window->GetWidth();
//    int h = window->GetHeight();
//    glViewport(0, 0, w, h);
//
//    glEnable(GL_DEPTH_TEST);
//    glClearColor(config.clearColor[0], config.clearColor[1], config.clearColor[2], config.clearColor[3]);
//
//    window->SetResizeCallback([this](int width, int height) {
//        glViewport(0, 0, width, height);
//    });
//
//    m_running = true;
//    m_lastTime = std::chrono::steady_clock::now();
//    LOG_INFO("Engine initialized successfully");
//    return true;
//
//   
//
//	
//}
//
//void Engine::Run() {
//    window->SetIcon(glfwwindow);
//   // window->MainDockSpace(nullptr);
//
//    using clock = std::chrono::steady_clock;
//    bool showGui = true; // persistent for the run session
//
//    while (m_running && window && !window->ShouldClose()) {
//        auto now = clock::now();
//        std::chrono::duration<float> delta = now - m_lastTime;
//        m_lastTime = now;
//        float dt = delta.count();
//
//        // 1) Poll events / update input first
//        if (m_input) {
//            m_input->Update();   // calls glfwPollEvents()
//            if (m_input->IsKeyPressed(GLFW_KEY_ESCAPE)) {
//                if (glfwwindow) glfwSetWindowShouldClose(glfwwindow, true);
//            }
//        }
//        else {
//            window->PollEvents();
//        }
//        
//
//        // 2) Start ImGui frame (only if enabled)
//        if (m_config.enableImGui) {
//            window->NewImguiFrame(glfwwindow);
//            window->MainDockSpace(nullptr);
//
//            // 3) Build GUI (only add windows when shown)
//			// ############################## Add all editor GUI here ##############################
//			window->MainSceneWindow(glfwwindow); // Main ImGui scene window
//
//
//
//
//            if (showGui) {
//                ImGui::Begin("Editor");
//
//                bool dockingEnabled = window->GetEnableDocking();
//                if (ImGui::Checkbox("Turn Docking off", &dockingEnabled)) {
//                    window->SetEnableDocking(dockingEnabled);
//                    ImGuiIO& io = ImGui::GetIO();
//                    if (dockingEnabled) {
//                        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
//                    }
//                    else {
//                        io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
//                    }
//                }
//
//                if (ImGui::Button(showGui ? "Play (stop GUI)" : "Stop (show GUI)")) {
//                    showGui = !showGui;
//                }
//                ImGui::Text("FPS: %.1f", 1.0f / std::max(0.00001f, dt));
//                ImGui::End();
//            }
//        }
//		// ############ No more editor GUI building beyond this point #############
//        
//        // 4) Update / render your scene AFTER building UI so UI overlays on top
//        Tick(dt);
//
//        // 5) Finish ImGui frame and render draw data (always do this if ImGui was started)
//        if (m_config.enableImGui) {
//            window->RenderImGui(glfwwindow); // this calls ImGui::Render() internally
//        }
//
//        // 6) Present
//        window->SwapBuffers();
//    }
//
//    m_running = false;
//
//}
//
//void Engine::Tick(float dt) {
//    (void)dt;
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//}
//
//void Engine::RequestExit() { m_running = false; }
//bool Engine::IsRunning() const { return m_running; }
//SpxWindow* Engine::GetWindow() { return window.get(); }
//
//void Engine::Shutdown() {
//
//    // Shutdown ImGui first (so it doesn't try to use destroyed GL resources)
//    if (m_config.enableImGui && window) {
//        window->ImGuiShutdown();
//		LOG_INFO("ImGui shutdown successfully");
//    }
//
//    // clean up in reverse order
//    m_input.reset();
//    if (window) {
//        window.reset();
//    }
//   
//    m_running = false;
//    LOG_INFO("Engine shutdown");
//}
