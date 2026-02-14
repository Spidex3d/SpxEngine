#include "entity.h"
#include "stb/stb_image.h"
#include "../include/asset_path.h"
#include "../include/log.h"
#include "../include/textures.h"
#include "../include/shader.h"
#include <memory>

// This is my games engine start date 01/01/2026
// Spidex Engine 
// I hate Programing
// I hate Programing
// I hate Programing
// <It works.>
// I Love Programing

// Note: Entity no longer owns or creates shaders. It receives a Shader* from Engine when rendering.

Entity::Entity() {}
Entity::~Entity() {}

void Entity::CreateCube(std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex,
    int& CubeObjIdx, const glm::vec3& position)
{
    stbi_set_flip_vertically_on_load(true);

    auto newCube = std::make_unique<CubeModel>(currentIndex, "Default Cube", CubeObjIdx);

    newCube->position = position;
    newCube->scale = glm::vec3(1.0f);

    switch (CubeObjIdx) {
    case 0:
        newCube->position = glm::vec3(0.0f, 0.0f, 0.0f);
        newCube->scale = glm::vec3(1.0f, 1.0f, 1.0f);

        break;
    case 1:
        newCube->position = glm::vec3(1.1f, 0.0f, 0.0f);
        newCube->scale = glm::vec3(1.0f, 1.0f, 1.0f);
        break;

    case 2:
        newCube->position = glm::vec3(-1.0f, -0.5f, 0.0f);
        newCube->scale = glm::vec3(0.5f, 0.5f, 0.5f);
        break;
    default:
        newCube->position = glm::vec3(2.0f, 2.0f, 0.0f);
        newCube->scale = glm::vec3(1.0f, 1.0f, 1.0f);
        //posx += 1.5;
        break;
    }

    // Build TRS: translate * rotate * scale (no rotation here)
    newCube->modelMatrix = glm::translate(glm::mat4(1.0f), newCube->position);
    newCube->modelMatrix = glm::scale(newCube->modelMatrix, newCube->scale);

    //// Load texture via SetTextureForGameObj
    std::string texPath = GetAssetPath(TEXTURE_PATH);
    std::string texFile = "crate.jpg";
    std::string full = texPath + texFile;

    if (!SetTextureForGameObj(newCube.get(), full)) {
        LOG_WARNING("CreateCube: Failed to set texture: " << full);
        // newCube->tex_ID remains 0; shader should handle missing texture
    }
    else {
        LOG_INFO("CreateCube: texture loaded tex_ID=" << newCube->tex_ID << " path=" << newCube->texPath);
    }

    entVector.push_back(std::move(newCube));
    ++currentIndex;
	++CubeObjIdx;
}

void Entity::RenderCube(Shader* shader, const glm::mat4& view, const glm::mat4& projection,
    std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& CubeObjIdx, int& selectedEntityId)
{
    // Ensure shader is available
    if (!shader) {
        LOG_WARNING("Entity::RenderPlane called without shader; skipping draw.");
        return;
    }

    shader->Use();
    shader->SetUniformInt("myTexture", 0);
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);

    // Render stored planes using their stored modelMatrix and persistent texture id
    for (const auto& model : entVector) {
        if (!model) continue;

        // Skip invisible objects
        if (!model->isVisible) continue;

        if (auto* cube = dynamic_cast<CubeModel*>(model.get())) {
            shader->setMat4("model", cube->modelMatrix);

            // Set selection uniform: compare entity id
            int isSelected = (cube->entId == selectedEntityId) ? 1 : 0;
            shader->SetUniformInt("u_selected", isSelected);
            shader->setVec3("u_highlightColor", glm::vec3(0.2, 0.2f, 0.8f)); // orange-ish



            if (cube->tex_ID) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cube->tex_ID);
            }
            else {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            cube->DrawCube(); // assuming Draw() exists for CubeModel

            if (cube->tex_ID) {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }
    // reset selection uniform (optional)
    shader->SetUniformInt("u_selected", 0);
    
}

void Entity::CreatePlane(std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex,
    int& PlaneObjIdx, const glm::vec3& position)
{
    stbi_set_flip_vertically_on_load(true);

    auto newPlane = std::make_unique<PlaneModel>(currentIndex, "Default Plane", PlaneObjIdx);

    newPlane->position = position;
    newPlane->scale = glm::vec3(1.0f);

    switch (PlaneObjIdx) {
    case 0:
        newPlane->position = glm::vec3(0.0f, 0.0f, 0.0f);
        newPlane->scale = glm::vec3(1.0f, 1.0f, 1.0f);

        break;
    case 1:
        newPlane->position = glm::vec3(1.1f, 0.0f, 0.0f);
        newPlane->scale = glm::vec3(1.0f, 1.0f, 0.5f);
        break;

    case 2:
        newPlane->position = glm::vec3(1.0f, 1.5f, 0.0f);
        newPlane->scale = glm::vec3(0.5f, 0.5f, 0.5f);
        break;
    default:
        newPlane->position = glm::vec3(2.0f, 2.0f, 0.0f);
        newPlane->scale = glm::vec3(1.0f, 1.0f, 1.0f);
        //posx += 1.5;
        break;
    }

    // Build TRS: translate * rotate * scale (no rotation here)
    newPlane->modelMatrix = glm::translate(glm::mat4(1.0f), newPlane->position);
    newPlane->modelMatrix = glm::scale(newPlane->modelMatrix, newPlane->scale);

    // Load texture via SetTextureForGameObj
    std::string texPath = GetAssetPath(TEXTURE_PATH);
    std::string texFile = "github.jpg";
    std::string full = texPath + texFile;

    if (!SetTextureForGameObj(newPlane.get(), full)) {
        LOG_WARNING("CreatePlane: Failed to set texture: " << full);
    }
    else {
        LOG_INFO("CreatePlane: texture loaded tex_ID=" << newPlane->tex_ID << " path=" << newPlane->texPath);
    }

    entVector.push_back(std::move(newPlane));
    ++currentIndex;
    // PlaneObjIdx updated by caller if needed
	++PlaneObjIdx;
}


 //Render existing planes using the provided shader
void Entity::RenderPlane(Shader* shader, const glm::mat4& view, const glm::mat4& projection,
    std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& PlaneObjIdx, int& selectedEntityId)
{
    // Ensure shader is available
    if (!shader) {
        LOG_WARNING("Entity::RenderPlane called without shader; skipping draw.");
        return;
    }

    shader->Use();
    shader->SetUniformInt("myTexture", 0);
   // shader->SetUniformInt("u_selected", 0); // initialize selection uniform
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);

    // Render stored planes using their stored modelMatrix and persistent texture id
    for (const auto& model : entVector) {
        if (!model) continue;

        // Skip invisible objects early
        if (!model->isVisible) continue;

        if (auto* plane = dynamic_cast<PlaneModel*>(model.get())) {
            // Use the pre-calculated model matrix (don't reset it)
            shader->setMat4("model", plane->modelMatrix);

            // Set selection uniform: compare entity id
            int isSelected = (plane->entId == selectedEntityId) ? 1 : 0;
            shader->SetUniformInt("u_selected", isSelected);
            shader->setVec3("u_highlightColor", glm::vec3(0.2, 0.2f, 0.8f)); // orange-ish

            if (plane->tex_ID) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, plane->tex_ID);
            }
            else {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            plane->DrawPlane();

            if (plane->tex_ID) {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }
    // reset selection uniform (optional)
    shader->SetUniformInt("u_selected", 0);
}

void Entity::CreateFloor(std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex,
    int& FloorObjIdx, const glm::vec3& position)
{
    stbi_set_flip_vertically_on_load(true);

    auto newFloor = std::make_unique<FloorTerrain>(currentIndex, "Default Floor", FloorObjIdx);
    newFloor->position = position;
    newFloor->scale = glm::vec3(1.0f);
   

    newFloor->position = glm::vec3(0.0f, -0.5f, 0.0f);
    newFloor->scale = glm::vec3(10.0f, 0.1f, 10.0f);

    // Build TRS: translate * rotate * scale (no rotation here)
    newFloor->modelMatrix = glm::translate(glm::mat4(1.0f), newFloor->position);
    newFloor->modelMatrix = glm::scale(newFloor->modelMatrix, newFloor->scale);

    // Load texture via SetTextureForGameObj
    std::string texPath = GetAssetPath(TEXTURE_PATH);
    std::string texFile = "stone.jpg";
    std::string full = texPath + texFile;

    if (!SetTextureForGameObj(newFloor.get(), full)) {
        LOG_WARNING("CreateFloor: Failed to set texture: " << full);
    }
    else {
        LOG_INFO("CreateFloor: texture loaded tex_ID=" << newFloor->tex_ID << " path=" << newFloor->texPath);
    }

    entVector.push_back(std::move(newFloor));
    ++currentIndex;
	++FloorObjIdx;
    
}

void Entity::RenderFloor(Shader* shader, const glm::mat4& view, const glm::mat4& projection,
    std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& FloorObjIdx, int& selectedEntityId)
{
    // Ensure shader is available
    if (!shader) {
        LOG_WARNING("Entity::RenderPlane called without shader; skipping draw.");
        return;
    }

    shader->Use();
    shader->SetUniformInt("myTexture", 0);
    // shader->SetUniformInt("u_selected", 0); // initialize selection uniform
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);

    // Render stored planes using their stored modelMatrix and persistent texture id
    for (const auto& model : entVector) {
        if (!model) continue;

        // Skip invisible objects early
        if (!model->isVisible) continue;

        if (auto* floor = dynamic_cast<FloorTerrain*>(model.get())) {
            // Use the pre-calculated model matrix (don't reset it)
            shader->setMat4("model", floor->modelMatrix);

            // Set selection uniform: compare entity id
            int isSelected = (floor->entId == selectedEntityId) ? 1 : 0;
            shader->SetUniformInt("u_selected", isSelected);
            shader->setVec3("u_highlightColor", glm::vec3(0.2, 0.2f, 0.8f)); // orange-ish

            if (floor->tex_ID) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, floor->tex_ID);
            }
            else {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            floor->DrawFloorTerrain();

            if (floor->tex_ID) {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }
    // reset selection uniform (optional)
    shader->SetUniformInt("u_selected", 0);

}

//bool SetTextureForGameObj(GameObj* obj, const std::string& path) {
//    if (!obj) return false;
//
//    if (!path.empty() && path == obj->texPath) {
//        LOG_INFO("SetTextureForGameObj: path unchanged, tex_ID=" << obj->tex_ID);
//        return true;
//    }
//
//    if (!obj->texPath.empty()) {
//        LOG_INFO("SetTextureForGameObj: unloading old path " << obj->texPath);
//        TextureManager::Unload(obj->texPath);
//        obj->texPath.clear();
//    }
//    else if (obj->tex_ID != 0) {
//        LOG_INFO("SetTextureForGameObj: unloading old tex ID " << obj->tex_ID);
//        TextureManager::Unload(obj->tex_ID);
//    }
//    obj->tex_ID = 0;
//
//    if (!path.empty()) {
//        LOG_INFO("SetTextureForGameObj: loading " << path);
//        GLuint tex = TextureManager::Load(path);
//        if (tex == 0) {
//            LOG_ERROR("SetTextureForGameObj: Failed to load " << path);
//            return false;
//        }
//        obj->tex_ID = tex;
//        obj->texPath = path;
//        LOG_INFO("SetTextureForGameObj: loaded tex_ID=" << tex);
//    }
//
//    return true;
//}

bool Entity::SetTextureForGameObj(GameObj* obj, const std::string& path)
{
    if (!obj) return false;

    // If same path, nothing to do
    if (!path.empty() && path == obj->texPath) return true;

    // Unload old texture (by path if available, fallback to ID)
    if (!obj->texPath.empty()) {
        TextureManager::Unload(obj->texPath);
        obj->texPath.clear();
    }
    else if (obj->tex_ID != 0) {
        // manager supports unloading by ID too
        TextureManager::Unload(obj->tex_ID);
    }
    obj->tex_ID = 0;

    if (!path.empty()) {
        GLuint tex = TextureManager::Load(path);
        if (tex == 0) {
            LOG_ERROR("SetTextureForGameObj: Failed to load " << path.c_str());
            return false;
        }
        obj->tex_ID = tex;
        obj->texPath = path;
    }
    return true;
}
