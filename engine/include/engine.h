// engine.h
#pragma once
#include "window.h"
#include "input.h"
#include <memory>
#include <chrono>

struct EngineConfig {
    WindowConfig windowConfig;
    bool enableImGui = false;
    float clearColor[4] = { 0.12f, 0.15f, 0.18f, 1.0f };
};

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool Initialize(const EngineConfig& config);
    void Run();
    void Tick(float dt);
    void RequestExit();
    bool IsRunning() const;
    SpxWindow* GetWindow();
    void Shutdown();

private:
    // moved here instead of Impl
    EngineConfig config_;
    std::unique_ptr<SpxWindow> window;
    GLFWwindow* glfwwindow = nullptr;
    std::unique_ptr<Input> input_;
    bool running_ = false;
    std::chrono::steady_clock::time_point lastTime_;
    // add other managers/systems as direct members here
};
