// engine.cpp
#include "engine.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui\imgui.h"
#include <imgui\imgui_impl_glfw.h>
#include <imgui\imgui_impl_opengl3.h>
#include "log.h"
#include <iostream>

Engine::Engine() = default;
Engine::~Engine() { Shutdown(); }

bool Engine::Initialize(const EngineConfig& config) {
    config_ = config;

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
    input_ = std::make_unique<Input>(glfwwindow);
    if (input_->HasKeyboardAttached()) {
        LOG_INFO("Keyboard input initialized");
    }
    else {
        LOG_WARNING("No keyboard detected; input may not work");
    }

	// ##################  ImGui initialization could go here  ####################
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

    running_ = true;
    lastTime_ = std::chrono::steady_clock::now();
    LOG_INFO("Engine initialized successfully");
    return true;
}

void Engine::Run() {
    using clock = std::chrono::steady_clock;
    bool showGui = true; // persistent for the run session

    while (running_ && window && !window->ShouldClose()) {
        auto now = clock::now();
        std::chrono::duration<float> delta = now - lastTime_;
        lastTime_ = now;
        float dt = delta.count();

        // 1) Poll events / update input first
        if (input_) {
            input_->Update();   // calls glfwPollEvents()
            if (input_->IsKeyPressed(GLFW_KEY_ESCAPE)) {
                if (glfwwindow) glfwSetWindowShouldClose(glfwwindow, true);
            }
        }
        else {
            window->PollEvents();
        }

        // 2) Start ImGui frame (only if enabled)
        if (config_.enableImGui) {
            window->NewImguiFrame(glfwwindow);

            // 3) Build GUI (only add windows when shown)
            if (showGui) {
                ImGui::Begin("Editor");
                if (ImGui::Button(showGui ? "Play (stop GUI)" : "Stop (show GUI)")) {
                    showGui = !showGui;
                }
                ImGui::Text("FPS: %.1f", 1.0f / std::max(0.00001f, dt));
                ImGui::End();
            }
        }

        // 4) Update / render your scene AFTER building UI so UI overlays on top
        Tick(dt);

        // 5) Finish ImGui frame and render draw data (always do this if ImGui was started)
        if (config_.enableImGui) {
            window->RenderImGui(glfwwindow); // this calls ImGui::Render() internally
        }

        // 6) Present
        window->SwapBuffers();
    }

    running_ = false;

}

void Engine::Tick(float dt) {
    (void)dt;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Engine::RequestExit() { running_ = false; }
bool Engine::IsRunning() const { return running_; }
SpxWindow* Engine::GetWindow() { return window.get(); }

void Engine::Shutdown() {

    // Shutdown ImGui first (so it doesn't try to use destroyed GL resources)
    if (config_.enableImGui && window) {
        window->ImGuiShutdown();
		LOG_INFO("ImGui shutdown successfully");
    }

    // clean up in reverse order
    input_.reset();
    if (window) {
        window.reset();
    }
   
    running_ = false;
    LOG_INFO("Engine shutdown");
}
