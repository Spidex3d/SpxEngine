// engine.cpp
#include <glad/glad.h>
#include "engine.h"
#include <GLFW/glfw3.h>
#include "imgui\imgui.h"
#include <imgui\imgui_impl_glfw.h>
#include <imgui\imgui_impl_opengl3.h>
#include <imgui\ImGuiAF.h>
#include "log.h"
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../include/shader.h"
#include "../include/asset_path.h"
#include "../src/Input/EditorInput.h"


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
	// Create EditorInput now that we have a native window for keyboard/mouse input
    m_input = std::make_unique<EditorInput>(glfwwindow);
    m_input->SetCamera(&m_camera);
    LOG_INFO("EditorInput initialized");
    if (m_input->HasKeyboardAttached()) {
		LOG_INFO("Keyboard detected, input initialized");
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
	m_cubeObjIdx = 0;
    m_planeObjIdx = 0;
	m_floorObjIdx = 0;

    // Create the plane shader once here (Engine owns it).
    // Build full paths relative to assets using GetAssetPath.
    std::string shaderDir = GetAssetPath("shaders/"); // adjust if you use a SHADER_PATH macro
    std::string vertFile = shaderDir + "default.vert";
    std::string fragFile = shaderDir + "default.frag";

    // Try to construct shader; the Shader class should log compile/link errors if any.
    m_planeShader = std::make_unique<Shader>(vertFile, fragFile);
    if (!m_planeShader) {
        LOG_WARNING("Failed to create plane shader");
    }
    else {
        LOG_INFO("Plane shader created: %s , %s", vertFile.c_str(), fragFile.c_str());
    }

    // Register a render callback with the window so it can call into Engine while the FBO is bound.
    // The callback computes a simple centered orthographic projection and calls Entity::RenderPlane.
    
    window->SetRenderCallback([this]() {
        int fbw = window->GetFramebufferWidth();
        int fbh = window->GetFramebufferHeight();
        if (fbw <= 0 || fbh <= 0) return;

        float aspect = (fbh > 0) ? static_cast<float>(fbw) / static_cast<float>(fbh) : 1.0f;

        glm::mat4 view = m_camera.GetViewMatrix();
        glm::mat4 projection = m_camera.GetProjectionMatrix(aspect);

        // compute selected entity id (or -1 if none)
        int selectedEntityId = -1;
        if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
            selectedEntityId = m_entities[m_selectedEntityIndex]->entId;
        }

        if (m_entity) {
            // render cubes and planes (updated signatures with selectedEntityId)
            m_entity->RenderCube(m_planeShader.get(), view, projection, m_entities, m_currentEntityIndex, m_cubeObjIdx, selectedEntityId);
            m_entity->RenderPlane(m_planeShader.get(), view, projection, m_entities, m_currentEntityIndex, m_planeObjIdx, selectedEntityId);
            m_entity->RenderFloor(m_planeShader.get(), view, projection, m_entities, m_currentEntityIndex, m_floorObjIdx, selectedEntityId);
        }
    });
    

    // Register action callback (UI -> Engine) so clicking "Add Plane" invokes Engine::AddPlane
    window->SetActionCallback([this](const std::string& cmd) {
        if (cmd == "AddCube") {
            // place at center by default
            AddCube(glm::vec3(0.0f, 0.0f, 0.0f));
        }
        if (cmd == "AddPlane") {
            // place at center by default
            AddPlane(glm::vec3(0.0f, 0.0f, 0.0f));
        }
        if (cmd == "AddFloor") {
            // place at center by default
            AddFloor(glm::vec3(0.0f, 0.0f, 0.0f));
        }
	});

    m_running = true;
    m_lastTime = std::chrono::steady_clock::now();
    LOG_INFO("Engine initialized successfully");
    return true;
}

void Engine::Run() {
    window->SetIcon(glfwwindow);
    bool showGui = true; // persistent for the run session

    using clock = std::chrono::steady_clock;
    

    while (m_running && window && !window->ShouldClose()) {
        auto now = clock::now();
        std::chrono::duration<float> delta = now - m_lastTime;
        m_lastTime = now;
        float dt = delta.count();

        // 2) Start ImGui frame (only if enabled)
        if (m_config.enableImGui) {
            window->NewImguiFrame(glfwwindow);

            // Ensure the dockspace exists before other windows so they can dock into it
            window->MainDockSpace(nullptr);

            // ############################## object editor ################################
           
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                ImGui::Begin("Object Inspector");

                if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                    GameObj* selected = m_entities[m_selectedEntityIndex].get();
                    if (selected) {
                        // Name / rename
                        char nameBuf[128];
                        //strncpy(nameBuf, selected->entName.c_str(), sizeof(nameBuf));
                        strncpy_s(nameBuf, selected->entName.c_str(), sizeof(nameBuf));
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                            selected->entName = std::string(nameBuf);
                        }
                        ImGui::TextColored(COLOR_LIGHTBLUE, ICON_FA_EDIT "  Editor");
                        // Position
                        float pos[3] = { selected->position.x, selected->position.y, selected->position.z };
                        if (ImGui::InputFloat3("Position", pos)) {
                            selected->position = glm::vec3(pos[0], pos[1], pos[2]);
                        }

                        // Rotation (Euler degrees for editing)
                        // store rotation in radians or degrees depending on your representation, this example uses degrees
                        float rotDeg[3] = {
                            glm::degrees(selected->rotation.x),
                            glm::degrees(selected->rotation.y),
                            glm::degrees(selected->rotation.z)
                        };
                        if (ImGui::InputFloat3("Rotation (deg)", rotDeg)) {
                            selected->rotation = glm::vec3(glm::radians(rotDeg[0]), glm::radians(rotDeg[1]), glm::radians(rotDeg[2]));
                        }

                        // Scale
                        float sc[3] = { selected->scale.x, selected->scale.y, selected->scale.z };
                        if (ImGui::InputFloat3("Scale", sc)) {
                            selected->scale = glm::vec3(sc[0], sc[1], sc[2]);
                        }

                        // Update modelMatrix using TRS (make sure order is correct for your math)
                        selected->modelMatrix = glm::translate(glm::mat4(1.0f), selected->position);
                        // apply rotation (if you use Euler -> convert to quat / rotate)
                        selected->modelMatrix = glm::rotate(selected->modelMatrix, selected->rotation.x, glm::vec3(1, 0, 0));
                        selected->modelMatrix = glm::rotate(selected->modelMatrix, selected->rotation.y, glm::vec3(0, 1, 0));
                        selected->modelMatrix = glm::rotate(selected->modelMatrix, selected->rotation.z, glm::vec3(0, 0, 1));
                        selected->modelMatrix = glm::scale(selected->modelMatrix, selected->scale);

                        ImGui::SeparatorText("Scene Properties");
                        ImGui::Text("Gameplay Properties");
                        ImGui::InputInt("Points", &selected->entPoints);
                        if (ImGui::Checkbox("Active", &selected->isActive)) { /* optionally handle enable/disable */ }
                        if (ImGui::Checkbox("Health Pack", &selected->isHealthPack)) {
                            // show health points input only if flagged
                        }
                        if (selected->isHealthPack) {
                            ImGui::InputInt("Health Pack Points", &selected->HealthPackPoints);
                        }
                        ImGui::Checkbox("Dangerous", &selected->isDangerous);
                        ImGui::Checkbox("Collidable", &selected->isCollidable);
                        if (ImGui::Checkbox("Visible", &selected->isVisible)) {
                            // toggling visible will affect rendering next frame
                        }

                        // Buttons for convenience
                        if (ImGui::Button("Focus")) {
                            // implement camera focus in future: center camera on selected->position
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Delete")) {
                            m_entities.erase(m_entities.begin() + m_selectedEntityIndex);
                            m_selectedEntityIndex = -1;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Exit")) {
				    		// close object inspector - editor
				    		m_selectedEntityIndex = -1;
                        }
                    }
                }
                else {
                    ImGui::Text("No object selected");
                }

                ImGui::End();
            }
			// ################################################ End object editor ###############################

			// ######################## add Main Object Explorer Window for right-click menu ####################
            if (showGui) {

                ImGui::Begin("Object Explorer");

                // Walk engine-owned entity list: m_entities
                for (int i = 0; i < (int)m_entities.size(); ++i) {
                    GameObj* obj = m_entities[i].get();
                    if (!obj) continue;

                    ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (m_selectedEntityIndex == i)
                        node_flags |= ImGuiTreeNodeFlags_Selected;
                    // ICON_FA_TRASH_ALT ICON_FA_PLUS ICON_FA_EDIT


                    // Display name + id
                    // choose icon based on visibility (requires FA icons loaded)
                    const char* visibilityIcon = obj->isVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;

                    // display name + id with icon prefix, keep unique ID suffix
                    std::string displayName = obj->entName.empty() ? ("Entity " + std::to_string(obj->entId)) : obj->entName;
                    std::string label = std::string(visibilityIcon) + " " + displayName + "##" + std::to_string(obj->entId);
                    //std::string label = std::string(displayName) + " " + visibilityIcon + "##" + std::to_string(obj->entId);

                    // render the tree node (leaf)
                    ImGui::TreeNodeEx(label.c_str(), node_flags);
                    
                    // ################################################ Pop up ################################
                    if (ImGui::BeginPopupContextItem(label.c_str())) {
                        if (ImGui::MenuItem(ICON_FA_TRASH_ALT" Delete")) {
                            // delete entity
                            m_entities.erase(m_entities.begin() + i);
                            if (m_selectedEntityIndex == i) m_selectedEntityIndex = -1;
                            else if (m_selectedEntityIndex > i) --m_selectedEntityIndex;
                            ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                            break; // container changed; break out of loop
                        }

                        if (ImGui::MenuItem(ICON_FA_EDIT" Edit")) {
                            // set selection to this entity so Inspector opens
                            m_selectedEntityIndex = i;
                            ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                            // no container change — safe to continue
                            // optionally call ImGui::SetWindowFocus("Inspector") here to bring it to front
                            break;
                        }
                        // ICON_FA_PLUS
                        if (ImGui::MenuItem(ICON_FA_AD" New")) { //will need to know which obj to add
                            //AddPlane(glm::vec3(-0.5f, 0.0f, 0.0f));
                            ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                            break; // container changed; break out of loop
                        }

                        ImGui::EndPopup();
                    }
                 
		
                    // For your current TreeNodeEx style, check click:
                    if (ImGui::IsItemClicked()) {
                        m_selectedEntityIndex = i;
                    }

                }

                ImGui::End();

            }
			// ######################## End Main Object Explorer Window ####################


            // Draw the MainSceneWindow which will call the registered render callback while FBO is bound
            window->MainSceneWindow(glfwwindow);
			window->MainScreenMenu(glfwwindow);

            if (m_input) {
                bool sceneHovered = false;
                if (window) sceneHovered = window->IsSceneWindowHovered();
                m_input->SetSceneHovered(sceneHovered);
                m_input->Update(dt);
				//LOG_TRACE("Engine::Run: Input updated");
            }
            else {
				// window->PollEvents();
            }

                window->PollEvents();

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

    ResolveCameraCollisionsAndPickups();
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
    m_planeShader.reset();
    if (window) {
        window.reset();
    }

    m_running = false;
    LOG_INFO("Engine shutdown");
}
// ######### This is where we add all the object to the game world #########
void Engine::AddCube(const glm::vec3& pos)
{
	// Check that m_entity is valid
    if (!m_entity) return;

    // Create the cube (appends into m_entities)
    m_entity->CreateCube(m_entities, m_currentEntityIndex, m_cubeObjIdx, pos);

    // Select the newly added entity so the Inspector opens
    m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;

    // Bring the inspector window to front (works while ImGui frame is active)
    ImGui::SetWindowFocus("Object Inspector");

}

void Engine::AddPlane(const glm::vec3& pos)
{
    if (!m_entity) return;
    m_entity->CreatePlane(m_entities, m_currentEntityIndex, m_planeObjIdx, pos);
    m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
    ImGui::SetWindowFocus("Object Inspector");
}
// add a floor to the scene at the given position
void Engine::AddFloor(const glm::vec3& pos)
{
	if (!m_entity) return;
	m_entity->CreateFloor(m_entities, m_currentEntityIndex, m_floorObjIdx, pos);
	m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
	ImGui::SetWindowFocus("Object Inspector"); // this will need work for terrain
}

static glm::vec3 ClampPointToAABB(const glm::vec3& p, const glm::vec3& minB, const glm::vec3& maxB) {
    return glm::vec3(
        glm::clamp(p.x, minB.x, maxB.x),
        glm::clamp(p.y, minB.y, maxB.y),
        glm::clamp(p.z, minB.z, maxB.z)
    );
}

bool Engine::SphereIntersectsAABB_World(const glm::vec3& sphereCenterWorld, float radius, const glm::mat4& modelMatrix, const glm::vec3& aabbMinLocal, const glm::vec3& aabbMaxLocal, glm::vec3& out_penetrationWorld)
{
    // transform sphere center into local space of object
    glm::mat4 invModel = glm::inverse(modelMatrix);
    glm::vec3 localCenter = glm::vec3(invModel * glm::vec4(sphereCenterWorld, 1.0f));

    // closest point from localCenter to local AABB
    glm::vec3 closest = ClampPointToAABB(localCenter, aabbMinLocal, aabbMaxLocal);

    // vector from closest point to sphere center in local space
    glm::vec3 localDelta = localCenter - closest;
    float distSq = glm::dot(localDelta, localDelta);

    // if outside or exactly on surface, no intersection if dist >= r^2
    if (distSq >= radius * radius) {
        return false;
    }

    float dist = sqrtf(distSq);
    // handle center inside AABB (dist == 0) -> choose outward normal using localCenter direction or nearest face
    glm::vec3 localPen;
    if (dist > 1e-6f) {
        localPen = localDelta * ((radius - dist) / dist); // local-space penetration vector
    }
    else {
        // sphere center lies inside the box or extremely close: push along the smallest-penetration axis
        float left = fabs(localCenter.x - aabbMinLocal.x);
        float right = fabs(aabbMaxLocal.x - localCenter.x);
        float down = fabs(localCenter.y - aabbMinLocal.y);
        float up = fabs(aabbMaxLocal.y - localCenter.y);
        float back = fabs(localCenter.z - aabbMinLocal.z);
        float front = fabs(aabbMaxLocal.z - localCenter.z);

        float minDist = left;
        localPen = glm::vec3(-1, 0, 0);
        if (right < minDist) { minDist = right; localPen = glm::vec3(1, 0, 0); }
        if (down < minDist) { minDist = down;  localPen = glm::vec3(0, -1, 0); }
        if (up < minDist) { minDist = up;    localPen = glm::vec3(0, 1, 0); }
        if (back < minDist) { minDist = back;  localPen = glm::vec3(0, 0, -1); }
        if (front < minDist) { minDist = front; localPen = glm::vec3(0, 0, 1); }

        localPen *= (radius + 0.001f);
    }

    // convert local penetration to world space (multiply by 3x3 linear part)
    glm::mat3 model3 = glm::mat3(modelMatrix);
    out_penetrationWorld = model3 * localPen;
    return true;
}

void Engine::ResolveCameraCollisionsAndPickups()
{
    if (!m_entity) return; // nothing to check

    glm::vec3 camPos = m_camera.Position;
    float radius = m_cameraRadius;

    // canonical local AABB for centered cube/plane primitives (change if models differ)
    glm::vec3 aabbMinLocal(-0.5f, -0.5f, -0.5f);
    glm::vec3 aabbMaxLocal(0.5f, 0.5f, 0.5f);

    // We'll apply immediate resolution per intersecting object (single pass).
    for (int i = 0; i < (int)m_entities.size(); ++i) {
        GameObj* obj = m_entities[i].get();
        if (!obj) continue;
        if (!obj->isCollidable) continue;
        if (!obj->isVisible) continue;

        glm::vec3 penetrationWorld;
        if (SphereIntersectsAABB_World(camPos, radius, obj->modelMatrix, aabbMinLocal, aabbMaxLocal, penetrationWorld)) {
            // push camera out of penetration
            camPos += penetrationWorld;
        }

        // pickup detection for health packs
        if (obj->isHealthPack && obj->isActive) {
            float d = glm::length(obj->position - camPos);
            if (d <= m_pickupRadius) {
                LOG_INFO("Collected Health Pack EntObj " << obj->entId << " Points " << obj->HealthPackPoints);
                // mark collected
                obj->isActive = false;
                obj->isVisible = false;
                // TODO: update player health/score state
            }
        }
    }

    // commit resolved position back to the camera
    m_camera.Position = camPos;
}

