// engine.h
#pragma once
#include "window.h"
#include "input.h"
#include <memory>
#include <chrono>

struct EngineConfig {
    WindowConfig windowConfig;
    bool enableImGui = false;
	//bool enableDocking = true;
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
	// All varibles with  leading m_ for member variables that I have looked at and understand what they do
    EngineConfig m_config;
    std::unique_ptr<SpxWindow> window;
    GLFWwindow* glfwwindow = nullptr;
    std::unique_ptr<Input> m_input;

	bool m_running = false; // main loop flag
    std::chrono::steady_clock::time_point m_lastTime;
    // add other managers/systems as direct members here
};
