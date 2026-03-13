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
#include <cfloat>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../include/shader.h"
#include "../include/asset_path.h"
#include "../src/Input/EditorInput.h"
#include "../src/Model_loaders/objLoader.h"
#include "../src/Sky/skyBox.h"
#include "../Shaders/ShaderManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>

#include <json/json.hpp>
using json = nlohmann::json;

// Minimal JSON-safe string escaper (handles quotes and backslashes) ### This is used for serializing scene data to JSON (e.g. for SaveScene) 
// and should be sufficient for simple strings like file paths and names.
// It does not handle Unicode or other special characters, but it covers the basics needed for our use case.
static std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\"': out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default: out += c; break;
        }
    }
    return out;
}
Engine::Engine() = default;
Engine::~Engine() { Shutdown(); }

bool Engine::SaveScene(const std::string& path)
{
    if (path.empty()) return false;

    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        LOG_WARNING("Engine::SaveScene: could not open file " << path);
        return false;
    }

    // Header
    out << "{\n";
    out << "  \"version\": 1,\n";
    out << "  \"entities\": [\n";

    bool firstEntity = true;
    for (const auto& entPtr : m_entities) {
        if (!entPtr) continue;

        // write comma between objects
        if (!firstEntity) out << ",\n";
        firstEntity = false;

        int id = entPtr->entId;
        std::string name = entPtr->entName;
        int typeId = entPtr->entTypeID;

        glm::vec3 pos = entPtr->position;
        glm::vec3 rot = entPtr->rotation;
        glm::vec3 scl = entPtr->scale;

        out << "    {\n";
        out << "      \"id\": " << id << ",\n";
        out << "      \"name\": \"" << JsonEscape(name) << "\",\n";
        out << "      \"typeId\": " << typeId << ",\n";

        // If this entity has a texture path, write it (escape)
        if (!entPtr->texPath.empty()) {
            out << "      \"texPath\": \"" << JsonEscape(entPtr->texPath) << "\",\n";
        }

		// If this entity has an asset path (e.g. for models or skyboxes), write it (escape)
        if (!entPtr->assetPath.empty()) {
            out << "      \"assetPath\": \"" << JsonEscape(entPtr->assetPath) << "\",\n";
        }

        out << "      \"position\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
        out << "      \"rotation\": [" << rot.x << ", " << rot.y << ", " << rot.z << "],\n";
        out << "      \"scale\": [" << scl.x << ", " << scl.y << ", " << scl.z << "]\n";

        out << "    }";
    }

    out << "\n  ]\n";
    out << "}\n";

    out.close();
    return true;

}

bool Engine::LoadScene(const std::string& path)
{
    if (path.empty()) return false;

    std::ifstream in(path);
    if (!in.is_open()) {
        LOG_WARNING("Engine::LoadScene: could not open file " << path);
        return false;
    }

    json root;
    try {
        in >> root;
    }
    catch (const std::exception& e) {
        LOG_WARNING("Engine::LoadScene: JSON parse error: " << e.what());
        in.close();
        return false;
    }
    in.close();

    // Unload any textures referenced by current entities to avoid leaks
    for (auto& entPtr : m_entities) {
        if (!entPtr) continue;
        if (!entPtr->texPath.empty()) {
            // TextureManager::Unload will decrement refcount and delete if zero
            TextureManager::Unload(entPtr->texPath);
            entPtr->texPath.clear();
        }
        if (entPtr->tex_ID) {
            TextureManager::Unload(entPtr->tex_ID);
            entPtr->tex_ID = 0;
        }
    }

    // Clear current scene
    m_entities.clear();
    m_currentEntityIndex = 0;
    m_selectedEntityIndex = -1;

    // Read entities array
    if (!root.contains("entities") || !root["entities"].is_array()) {
        LOG_WARNING("Engine::LoadScene: no entities array in scene file");
        return true; // nothing to recreate
    }

    for (const auto& je : root["entities"]) {
        // Read data with safe defaults
        std::string name = je.value("name", std::string("entity"));
        int id = je.value("id", 0);
        // int typeId = je.value("typeId", 0); // not used for now

        glm::vec3 pos(0.0f), rot(0.0f), scl(1.0f);
        if (je.contains("position") && je["position"].is_array() && je["position"].size() >= 3) {
            pos.x = je["position"][0].get<float>();
            pos.y = je["position"][1].get<float>();
            pos.z = je["position"][2].get<float>();
        }
        if (je.contains("rotation") && je["rotation"].is_array() && je["rotation"].size() >= 3) {
            rot.x = je["rotation"][0].get<float>();
            rot.y = je["rotation"][1].get<float>();
            rot.z = je["rotation"][2].get<float>();
        }
        if (je.contains("scale") && je["scale"].is_array() && je["scale"].size() >= 3) {
            scl.x = je["scale"][0].get<float>();
            scl.y = je["scale"][1].get<float>();
            scl.z = je["scale"][2].get<float>();
        }

        std::string texPath;
        if (je.contains("texPath") && je["texPath"].is_string()) {
            texPath = je["texPath"].get<std::string>();
        }
        // ############################################################# New Bit ###################################
       // std::string type = je.value("typeId", std::string("GameObj"));
		int typeId = je.value("typeId", 0);
        std::string asset = je.value("assetPath", std::string());

        // create appropriate entity based on type
        if (typeId == OBJ_CUBE) {
            AddCube(pos);
        }
        else if (typeId == OBJ_PLANE) {
            AddPlane(pos);
        }
        else if (typeId == OBJ_FLOOR) {
            AddFloor(pos);
        }
        else if (typeId == OBJ_OBJ_MODEL) {
            // create object from file using Entity helper (does not change selection behavior)
            if (m_entity) {
                m_entity->CreateObjFromFile(m_entities, m_currentEntityIndex, m_modelObjIdx, asset, pos);
                m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
                // set type/assetPath on created object
                if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                    GameObj* g = m_entities[m_selectedEntityIndex].get();
                    if (g) {
                        g->entTypeID = OBJ_OBJ_MODEL;
                        g->assetPath = asset;
                    }
                }
            }
        }
        else if (typeId == GLTF_OBJ_MODEL) {
            if (m_entity) {
                m_entity->CreateGltfFromFile(m_entities, m_currentEntityIndex, m_modelGltfIdx, asset, pos);
                m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
                if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                    GameObj* g = m_entities[m_selectedEntityIndex].get();
                    if (g) {
                        g->entTypeID = GLTF_OBJ_MODEL;
                        g->assetPath = asset;
                    }
                }
            }
        }
        else if (typeId == SKY_OBJ) {
            if (m_entity) {
                m_entity->CreateSkyBox(m_entities, m_currentEntityIndex, m_skyIdx, asset, glm::vec3(0.0f));
                m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
                if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                    GameObj* g = m_entities[m_selectedEntityIndex].get();
                    if (g) {
                        g->entTypeID = SKY_OBJ;
                        g->assetPath = asset;
                    }
                }
            }
        }
        else {
            // fallback: create a cube so you can inspect the saved transform
            AddCube(pos);
        }




        // For this minimal loader, create a cube for each saved entity (you can expand later)
        //AddCube(pos); // creates and selects a new cube, sets m_selectedEntityIndex

        //if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
        //    GameObj* newObj = m_entities[m_selectedEntityIndex].get();
        //    if (newObj) {
        //        // restore transform and name/id
        //        newObj->position = pos;
        //        newObj->rotation = rot;
        //        newObj->scale = scl;
        //        // recompute modelMatrix (same logic as inspector)
        //        newObj->modelMatrix = glm::translate(glm::mat4(1.0f), newObj->position);
        //        newObj->modelMatrix = glm::rotate(newObj->modelMatrix, newObj->rotation.x, glm::vec3(1, 0, 0));
        //        newObj->modelMatrix = glm::rotate(newObj->modelMatrix, newObj->rotation.y, glm::vec3(0, 1, 0));
        //        newObj->modelMatrix = glm::rotate(newObj->modelMatrix, newObj->rotation.z, glm::vec3(0, 0, 1));
        //        newObj->modelMatrix = glm::scale(newObj->modelMatrix, newObj->scale);

        //        newObj->entName = name;
        //        newObj->entId = id;

        //        // apply texture if present (this loads full-resolution texture)
        //        if (!texPath.empty()) {
        //            if (!m_entity->SetTextureForGameObj(newObj, texPath)) {
        //                LOG_WARNING("Engine::LoadScene: failed to apply texture " << texPath << " to entity " << id);
        //            }
        //            else {
        //                LOG_INFO("Engine::LoadScene: applied texture " << texPath << " to entity " << id);
        //            }
        //        }
        //    }
        //}
    }

    // After loading, clear selection
    m_selectedEntityIndex = -1;
    LOG_INFO("Engine::LoadScene: finished loading scene from " << path);
    return true;
}

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
	m_cubeObjIdx = 0;
    m_planeObjIdx = 0;
	m_floorObjIdx = 0;
	m_skyIdx = 0;


    std::string shaderDir = GetAssetPath("Shaders/");
    ShaderManager::SetupShaders(shaderDir);

    // reference the default shader (non-owning)
    m_planeShader = ShaderManager::Default();
    if (!m_planeShader) {
        LOG_WARNING("Engine: ShaderManager failed to provide default shader");
    }
    else {
        LOG_INFO("Engine: acquired default shader from ShaderManager");
    }
	// sky shader for skybox rendering
    m_skyShader = ShaderManager::Sky();
    if (!m_skyShader) {
        LOG_WARNING("Engine: ShaderManager failed to provide default shader");
    }
    else {
        LOG_INFO("Engine: acquired Sky shader from ShaderManager");
    }

        
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

        // ---- NEW: rotate objects that have rotateY enabled ----
        //const float TWO_PI = 6.28318530717958647692f;
        //const float DEFAULT_ROT_SPEED_DEG = 45.0f; // degrees per second
        const float rotSpeed = glm::radians(DEFAULT_ROT_SPEED_DEG); // radians per second

        // Update rotating objects before rendering so the change is visible immediately
        for (auto& uptr : m_entities) {
            if (!uptr) continue;
            if (!uptr->rotateY) continue;

            // increment rotation.y (rotation is stored in radians)
            uptr->rotation.y += rotSpeed * m_frameDt;
            // wrap to avoid growth
            if (uptr->rotation.y > TWO_PI) uptr->rotation.y -= TWO_PI;
            else if (uptr->rotation.y < -TWO_PI) uptr->rotation.y += TWO_PI;

            // rebuild modelMatrix using the same TRS order your inspector uses:
            uptr->modelMatrix = glm::translate(glm::mat4(1.0f), uptr->position);
            uptr->modelMatrix = glm::rotate(uptr->modelMatrix, uptr->rotation.x, glm::vec3(1, 0, 0));
            uptr->modelMatrix = glm::rotate(uptr->modelMatrix, uptr->rotation.y, glm::vec3(0, 1, 0));
            uptr->modelMatrix = glm::rotate(uptr->modelMatrix, uptr->rotation.z, glm::vec3(0, 0, 1));
            uptr->modelMatrix = glm::scale(uptr->modelMatrix, uptr->scale);
        }
        // ---- end rotation update ----

        if (m_entity) {
            // Add a sky box
            m_entity->RenderSkyBox(m_skyShader, view, projection, m_entities, m_currentEntityIndex, m_skyIdx, selectedEntityId);
            // render other objects...
            m_entity->RenderGltfModel(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_modelGltfIdx, selectedEntityId);
            m_entity->RenderObjModel(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_modelObjIdx, selectedEntityId);
            m_entity->RenderCube(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_cubeObjIdx, selectedEntityId);
            m_entity->RenderPlane(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_planeObjIdx, selectedEntityId);
            m_entity->RenderFloor(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_floorObjIdx, selectedEntityId);
        }

   //     int fbw = window->GetFramebufferWidth();
   //     int fbh = window->GetFramebufferHeight();
   //     if (fbw <= 0 || fbh <= 0) return;

   //     float aspect = (fbh > 0) ? static_cast<float>(fbw) / static_cast<float>(fbh) : 1.0f;

   //     glm::mat4 view = m_camera.GetViewMatrix();
   //     glm::mat4 projection = m_camera.GetProjectionMatrix(aspect);

   //     // compute selected entity id (or -1 if none)
   //     int selectedEntityId = -1;
   //     if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
   //         selectedEntityId = m_entities[m_selectedEntityIndex]->entId;
   //     }

   //     if (m_entity) {
   //         // Add a sky box
   //         m_entity->RenderSkyBox(m_skyShader, view, projection, m_entities, m_currentEntityIndex, m_skyIdx, selectedEntityId);
   //         // render cubes and planes (updated signatures with selectedEntityId)
			//m_entity->RenderGltfModel(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_modelGltfIdx, selectedEntityId);
			//m_entity->RenderObjModel(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_modelObjIdx, selectedEntityId);
   //         m_entity->RenderCube(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_cubeObjIdx, selectedEntityId);
   //         m_entity->RenderPlane(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_planeObjIdx, selectedEntityId);
   //         m_entity->RenderFloor(m_planeShader, view, projection, m_entities, m_currentEntityIndex, m_floorObjIdx, selectedEntityId);
   //     
   //     
   //     }


    });
    

    // Register action callback (UI -> Engine) so clicking "Add Plane" invokes Engine::AddPlane
    window->SetActionCallback([this](const std::string& cmd) {
        if (cmd == "SaveScene") {
            // Ask UI for a file path (reuse openFileDialog). If the user cancelled, do nothing.
            std::string savePath;
            if (window) {
                savePath = window->openSaveFileDialog("spxscene", "Scene Files\0*.spx\0All Files\0*.*\0\0");
                //savePath = window->openSaveFileDialog("json", "JSON Files\0*.json\0All Files\0*.*\0\0");

            }

            if (savePath.empty()) {
                LOG_INFO("Engine: SaveScene cancelled by user");
            }
            else {
                if (SaveScene(savePath)) {
                    LOG_INFO("Engine: Scene saved to " << savePath);
                }
                else {
                    LOG_WARNING("Engine: Failed to save scene to " << savePath);
                }
            }
            return;
        }

        const std::string loadPrefix = "LoadScene";
        if (cmd.rfind(loadPrefix, 0) == 0) {
            std::string payload = cmd.substr(loadPrefix.size());
            // trim whitespace
            auto trim = [](std::string& s) {
                while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
                while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
            };
            trim(payload);
            if (payload.empty()) {
                LOG_WARNING("Engine: LoadScene command received but no path provided");
                return;
            }
            if (!LoadScene(payload)) {
                LOG_WARNING("Engine: LoadScene failed for " << payload);
            }
            else {
                LOG_INFO("Engine: Loaded scene " << payload);
            }
            return;
        }




		// gltf Models
        if (cmd == "AddGltf") {
            // Open file dialog via the window (UI owned by SpxWindow)
            std::string modelPath;
            if (window) {
                // need to work how to set file type
                modelPath = window->openFileDialog();
            }
            if (!modelPath.empty()) {
                if (m_entity) {
                    // Create an object from the selected file and append to m_entities
                    m_entity->CreateGltfFromFile(m_entities, m_currentEntityIndex, m_modelGltfIdx, modelPath, glm::vec3(0.0f));
                    // select the newly added entity
                    m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
                    ImGui::SetWindowFocus("Object Inspector");
                    LOG_INFO("Engine: Added object from " << modelPath);
                }
            }
            else {
                LOG_INFO("Engine: AddGltf cancelled or no file selected");
            }
        }
        // Obj Models
        if (cmd == "AddObj") {
            // Open file dialog via the window (UI owned by SpxWindow)
            std::string modelPath;
            if (window) {
                // need to work how to set file type
                modelPath = window->openFileDialog();
            }
            if (!modelPath.empty()) {
                if (m_entity) {
                    // Create an object from the selected file and append to m_entities
                    m_entity->CreateObjFromFile(m_entities, m_currentEntityIndex, m_modelObjIdx, modelPath, glm::vec3(0.0f));
                    // select the newly added entity
                    m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
                    ImGui::SetWindowFocus("Object Inspector");
                    LOG_INFO("Engine: Added object from " << modelPath);
                }
            }
            else {
                LOG_INFO("Engine: AddObj cancelled or no file selected");
            }
        }

		//##################################################### Add Texture command handling (UI -> Engine) ###################################
        const std::string texPrefix = "AddTexture:";
        if (cmd.rfind(texPrefix, 0) == 0) { // starts_with
            std::string payload = cmd.substr(texPrefix.size());
            // trim whitespace
            auto trim = [](std::string& s) {
                while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
                while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
            };
            trim(payload);
            if (payload.empty()) {
                LOG_WARNING("Engine: AddTexture command received but no path provided");
                return;
            }

            LOG_INFO("Engine: AddTexture command received with path: " << payload);

            // If there is a selected entity, apply the texture to it
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                GameObj* sel = m_entities[m_selectedEntityIndex].get();
                if (sel && m_entity) {
                    if (!m_entity->SetTextureForGameObj(sel, payload)) {
                        LOG_WARNING("Engine: Failed to apply texture to selected entity: " << payload);
                    }
                    else {
                        LOG_INFO("Engine: Applied texture to entity " << sel->entId << " -> " << payload);
                    }
                }
                return; // handled
            }

            // No selection: create a new plane (or cube) and apply the texture to it
            //if (m_entity) {
            //    // This will create a Floor (or call AddPlane/AddCube if you prefer)
            //    AddPlane(glm::vec3(0.0f)); // creates and selects new plane
            //    if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
            //        GameObj* newObj = m_entities[m_selectedEntityIndex].get();
            //        if (newObj) {
            //            if (!m_entity->SetTextureForGameObj(newObj, payload)) {
            //                LOG_WARNING("Engine: Failed to apply texture to new entity: " << payload);
            //            }
            //            else {
            //                LOG_INFO("Engine: Created new entity and applied texture: " << payload);
            //            }
            //        }
            //    }
            //}
            //else {
            //    LOG_WARNING("Engine: no entity manager available to add/apply texture");
            //}

            return; // handled
        }
        
        //##################################################### End Texture ###########################################

        // ---- Handle "AddSkyBox:<path>" without creating duplicates ----
        const std::string prefix = "AddSkyBox:";
        if (cmd.rfind(prefix, 0) == 0) { // starts_with
            std::string payload = cmd.substr(prefix.size());
            // trim whitespace
            auto trim = [](std::string& s) {
                while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
                while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
            };
            trim(payload);
            if (payload.empty()) {
                LOG_WARNING("Engine: AddSkyBox command received but no path provided");
                return;
            }

            // Use payload exactly as sent by the UI (may be folder or file path)
            std::string pathToLoad = payload;

            // 1) Try to find an existing skybox in the scene and update it
            for (int i = 0; i < (int)m_entities.size(); ++i) {
                GameObj* obj = m_entities[i].get();
                if (!obj) continue;
                if (auto* sky = dynamic_cast<LoadSkybox*>(obj)) {
                    // Attempt to replace the cubemap on the existing sky object
                    LOG_INFO("Engine: updating existing skybox (entity %d) with %s", sky->entId, pathToLoad.c_str());
                    // Make sure LoadFromFolder accepts a file path too (it should). It should also free previous textures.
                    if (!sky->LoadFromFolder(pathToLoad)) {
                        LOG_WARNING("Engine: LoadFromFolder failed for %s (existing sky not replaced)." << pathToLoad.c_str());
                    }
                    else {
                        // update a saved scene
                        sky->assetPath = pathToLoad;
                        // mark selection to this skybox
                        m_selectedEntityIndex = i;
                        ImGui::SetWindowFocus("Object Inspector");
                        LOG_INFO("Engine: updated existing skybox entity %d", sky->entId);
                    }
                    return; // handled
                }
            }

            // 2) No existing skybox found -> create a new one
            LOG_INFO("Engine: no existing skybox found, creating new skybox from %s", pathToLoad.c_str());
            if (m_entity) {
                // CreateSkyBox expects folderPath in your code; we pass the payload which may be a file path or folder.
                // Your LoadSkybox::LoadFromFolder should handle file paths. If it expects a folder, adjust accordingly.
                m_entity->CreateSkyBox(m_entities, m_currentEntityIndex, m_skyIdx, pathToLoad, glm::vec3(0.0f));

                m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
                ImGui::SetWindowFocus("Object Inspector");
                LOG_INFO("Engine: created new skybox entity (index %d) from %s", m_selectedEntityIndex, pathToLoad.c_str());
            }
            else {
                LOG_WARNING("Engine: no entity manager available to create skybox");
            }

            return; // handled the AddSkyBox action
        }

        
   
        


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
        // new reset camera command
        if (cmd == "resetCameraPos" || cmd == "ResetCameraPosition") {
            // Choose your preferred default here:
            glm::vec3 defaultPos(0.0f, 0.0f, 5.0f);
            m_camera.ResetToDefaults(defaultPos, YAW, PITCH);

            // Ensure EditorInput/other systems see the updated camera
            if (m_input) {
                m_input->SetCamera(&m_camera);
                // reset input mouse tracking to avoid jumps
                m_input->SetSceneHovered(false);
            }
            LOG_INFO("Engine: Camera reset to default position");
        }
        // inside the existing SetActionCallback lambda:
        if (cmd == "focusSelected") {
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                GameObj* sel = m_entities[m_selectedEntityIndex].get();
                if (sel) {
                    // world position of the object's origin
                    glm::vec3 worldPos = glm::vec3(sel->modelMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                    // choose a distance appropriate for your scene scale; 5.0f is a reasonable default
                    float focusDistance = 5.0f;
                    m_camera.FocusOn(worldPos, focusDistance);

                    // ensure input uses the updated camera and avoid big mouse jumps
                    if (m_input) {
                        m_input->SetCamera(&m_camera);
                        m_input->SetSceneHovered(false);
                    }
                    LOG_INFO("Engine: Focused camera on entity " << sel->entId);
                }
            }
            else {
                LOG_INFO("Engine: focusSelected called but no entity selected");
            }
        }

		// new ortho/ perspective toggle command
        // inside the SetActionCallback lambda:
        if (cmd == "TopDownView") {
            // Center on selected object if present, otherwise origin
            glm::vec3 center(0.0f);
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < (int)m_entities.size()) {
                GameObj* sel = m_entities[m_selectedEntityIndex].get();
                if (sel) {
                    center = glm::vec3(sel->modelMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                }
            }
            float orthoHalfHeight = 10.0f; // tune based on scene scale
            float heightAbove = 15.0f;     // camera altitude 25.0
            m_camera.SetOrthographicTopDown(center, orthoHalfHeight, heightAbove);

            if (m_input) {
                m_input->SetCamera(&m_camera);
                m_input->SetSceneHovered(false);
            }
            LOG_INFO("Engine: switched to TopDownView at " << center.x << "," << center.y << "," << center.z);
        }
        ImGui::SameLine();
		    if (cmd == "PerspectiveView") {
		    	m_camera.SetPerspectiveFromDefaults();
		    	if (m_input) {
		    		m_input->SetCamera(&m_camera);
		    		m_input->SetSceneHovered(false);
		    	}
		    	LOG_INFO("Engine: switched to PerspectiveView");
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

		m_frameDt = dt; // rotating objects use this to rotate at a consistent speed regardless of frame rate

        // 2) Start ImGui frame (only if enabled)
        if (m_config.enableImGui) {
            window->NewImguiFrame(glfwwindow);

            // Ensure the dockspace exists before other windows so they can dock into it
            window->MainDockSpace(nullptr);


			
            // ############################## Score board ################################
            
            // ############################## End Score board ################################

   
           
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
						if (ImGui::Checkbox("Rotate Y", &selected->rotateY)) {
							// toggling rotateY will cause the object to start/stop rotating in the render loop
							selected->rotateY = selected->rotateY; // just to emphasize the change happens immediately
						}
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

                        ImGui::SeparatorText("Texture on selected object");

                        // show path or "None"
                        if (!selected->texPath.empty()) {
                            ImGui::TextWrapped("Path: %s", selected->texPath.c_str());
                        }
                        else {
                            ImGui::Text("Texture: None");
                        }

                        // Preview (if texture present)
                        if (selected->tex_ID != 0) {
                            ImGui::Text("Preview:");
                            ImGui::Image((void*)(intptr_t)selected->tex_ID, ImVec2(128, 128));
                        }

                        // Change texture button
                        if (ImGui::Button("Change Texture")) {
                            // Blocking Win32 dialog - returns UTF-8 path (your openFileDialog returns std::string)
                            std::string path;
                            if (window) {
                                path = window->openFileDialog();
                            }

                            if (!path.empty()) {
                                if (!m_entity->SetTextureForGameObj(selected, path)) {
                                    LOG_WARNING("Failed to set texture for entity " << selected->entId);
                                }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Clear Texture")) {
                            // Clear/unload texture
                            m_entity->SetTextureForGameObj(selected, "");
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
            window->ResourcesInspector(glfwwindow);
			window->EnvironmentExplorer(glfwwindow);
			window->AssetBrowser(glfwwindow);

         // ##################################################### Picking ########################################################
		 // ProcessViewportPick is in EditorInput and returns the picked entity index or -1 if none.
         // It also updates internal hovered state for the viewport.              
            if (m_input) {
                ImVec2 vpMin = window->GetSceneViewportPos();
                ImVec2 vpSize = window->GetSceneViewportSize();
                bool sceneHovered = window->IsSceneWindowHovered();

                int picked = m_input->ProcessViewportPick(m_entities, vpMin, vpSize, sceneHovered, dt);
                if (picked >= 0) {
                    m_selectedEntityIndex = picked;
                    LOG_INFO("Picked entity index " << picked << " entId = " << m_entities[picked]->entId);
                }
                // Note: EditorInput::ProcessViewportPick already calls Update(dt) and SetSceneHovered(sceneHovered).
            }
                        
        }
        // ################################################# End Picking ########################################################
            
        window->PollEvents();


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
   // m_planeShader.reset();
    if (window) {
        window.reset();
    }

    m_running = false;
    LOG_INFO("Engine shutdown");
}
void Engine::AddGltf(const std::string& modelPath, const glm::vec3& pos)
{
    if (!m_entity) return;
    if (modelPath.empty()) return;

    m_entity->CreateObjFromFile(m_entities, m_currentEntityIndex, m_modelObjIdx, modelPath, pos);

    m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
    ImGui::SetWindowFocus("Object Inspector");
    LOG_INFO("Engine: Added object from file " << modelPath << " at pos (" << pos.x << "," << pos.y << "," << pos.z << ")");
}


void Engine::AddObj(const std::string& modelPath, const glm::vec3& pos)
{
    if (!m_entity) return;
    if (modelPath.empty()) return;

    m_entity->CreateObjFromFile(m_entities, m_currentEntityIndex, m_modelObjIdx, modelPath, pos);

    m_selectedEntityIndex = static_cast<int>(m_entities.size()) - 1;
    ImGui::SetWindowFocus("Object Inspector");
    LOG_INFO("Engine: Added object from file " << modelPath << " at pos (" << pos.x << "," << pos.y << "," << pos.z << ")");
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
// ######################################################################################################################
// ##################################################### Picking ########################################################
// ######################################################################################################################

// #################################################### Picking end #####################################################

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

            glm::vec3 objWorldPos = glm::vec3(obj->modelMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));


            //float d = glm::length(obj->position - camPos);
            float d = glm::length(objWorldPos - camPos);

            


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

