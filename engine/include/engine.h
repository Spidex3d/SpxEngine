#pragma once
#include "window.h"
#include <memory>
#include <chrono>
#include <vector>

#include "../src/Camera/Camera.h" // <-- new Camera class
#include "../src/Input/EditorInput.h" // <-- new Editor input handling
// Forward-declare to avoid pulling shader header into every consumer
class Shader;

#include "entity.h" // Engine will own the Entity and the entity vector

struct EngineConfig {
    WindowConfig windowConfig;
    bool enableImGui = false;
    bool enableDocking = true;
    float clearColor[4] = { 0.12f, 0.15f, 0.18f, 1.0f };
};
const ImVec4 COLOR_LIGHTBLUE(0.43f, 0.7f, 0.89f, 1.0f);

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

	int GetSelectedEntityIndex() const { return m_selectedEntityIndex; }  // use for selecting entity in UI
	void SetSelectedEntityIndex(int idx) { m_selectedEntityIndex = idx; } // set from UI

    void AddCube(const glm::vec3& pos = glm::vec3(0.0f));
    // Add a plane to the scene at the given position (default center)
    void AddPlane(const glm::vec3& pos = glm::vec3(0.0f));

    // Access camera
    Camera& GetCamera() { return m_camera; }


private:
    // All varibles with  leading m_ for member variables that I have looked at and understand what they do
    EngineConfig m_config;
    std::unique_ptr<SpxWindow> window;
    GLFWwindow* glfwwindow = nullptr;
    //std::unique_ptr<Input> m_input;
    std::unique_ptr<EditorInput> m_input;

    // Engine-owned entity state
    std::unique_ptr<Entity> m_entity;
    std::vector<std::unique_ptr<GameObj>> m_entities;
    int m_currentEntityIndex = 0;
	int m_planeObjIdx; // plane object index
	int m_cubeObjIdx;  // cube object index

    int m_selectedEntityIndex = -1; // -1 = none selected

    // Engine-owned shader for plane rendering
    std::unique_ptr<Shader> m_planeShader;
    // Engine-owned camera (new)
    Camera m_camera = Camera(glm::vec3(0.0f, 0.0f, 6.0f));


    bool m_running = false; // main loop flag
    std::chrono::steady_clock::time_point m_lastTime;
    // add other managers/systems as direct members here
};





