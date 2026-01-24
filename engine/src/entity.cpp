#include "entity.h"
#include "stb/stb_image.h"
#include "../include/asset_path.h"
#include "../include/shader.h"
#include "../include/log.h"
//#include "texture_manager.h"
#include "../include/textures.h"
#include <memory>

// Module-scope cached shader so we don't recompile every frame.
// This keeps the change local to this module. If you later want to own the shader in Engine,
// move initialization there and pass a pointer/reference into RenderPlane.
static std::unique_ptr<Shader> s_planeShader;

static void EnsurePlaneShader() {
    //  C:\Users\marty\Desktop\SPXEngine\SPXEngine\SpxEngine\engine\Shader\shader.frag
    std::string shaderPath = GetAssetPath(SHADER_PATH);
    std::string shaderVfile = "default.vert";
    std::string shaderFfile = "default.frag";
    std::string shaderVfull = shaderPath + shaderVfile;
    std::string shaderFfull = shaderPath + shaderFfile;

    if (!s_planeShader) {
        s_planeShader = std::make_unique<Shader>(shaderVfull, shaderFfull);
    }
}

Entity::Entity() {}
Entity::~Entity() {}

void Entity::RenderPlane(const glm::mat4& view, const glm::mat4& projection,
    std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& PlaneObjIdx)
{
    stbi_set_flip_vertically_on_load(true);

    // create plane(s) when requested
    if (ShouldAddPlane) {
        PlaneObjIdx = static_cast<int>(entVector.size());

        auto newPlane = std::make_unique<PlaneModel>(currentIndex, "DefaultPlane", PlaneObjIdx);
        switch (PlaneObjIdx) {
        case 0:
            newPlane->position = glm::vec3(0.0f, 0.0f, 0.0f);
            break;
        case 1:
            newPlane->position = glm::vec3(1.5f, 0.0f, 0.0f);
            break;
        case 2:
            newPlane->position = glm::vec3(2.0f, 0.0f, 0.0f);
            break;
        default:
            newPlane->position = glm::vec3(2.0f, 2.0f, 0.0f);
            break;
        }

        newPlane->scale = glm::vec3(1.0f);

        // Build TRS: translate * rotate * scale  (no rotation here)
        newPlane->modelMatrix = glm::translate(glm::mat4(1.0f), newPlane->position);
        newPlane->modelMatrix = glm::scale(newPlane->modelMatrix, newPlane->scale);

        // Load texture via TextureManager (cached) so it persists
        //Texture tex(texPath + texFile);
        //        if (!tex.IsLoaded()) {
        //            LOG_WARNING("Failed to load texture: textures/github.jpg");
        //            // optional: fall back or continue without texture
        //        }


        std::string texPath = GetAssetPath(TEXTURE_PATH);
        std::string texFile = "github.jpg";
        std::string full = texPath + texFile;
        GLuint texID = TextureManager::Load(full);
        if (texID == 0) {
            LOG_WARNING("Failed to load texture: %s", full.c_str());
            // fallback: leave tex_ID = 0 and shader should handle it
        }
        newPlane->tex_ID = texID;

        entVector.push_back(std::move(newPlane));
        ShouldAddPlane = false; // Reset the flag after adding the plane
    }

    // Ensure shader exists and reuse it for all planes
    EnsurePlaneShader();
    if (!s_planeShader) {
        LOG_WARNING("Plane shader not available");
        return;
    }

    Shader* shader = s_planeShader.get();
    shader->Use();
    shader->SetUniformInt("myTexture", 0);
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);

    // Render stored planes using their stored modelMatrix and persistent texture id
    for (const auto& model : entVector) {

        if (auto* plane = dynamic_cast<PlaneModel*>(model.get())) {
            // Use the pre-calculated model matrix (don't reset it)
            shader->setMat4("model", plane->modelMatrix);

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
}





//#include "entity.h"
//#include "stb/stb_image.h"
//#include "../include/asset_path.h"
//#include "../include/textures.h"
//#include "../include/shader.h"
//#include "../include/log.h"
//
//void Entity::RenderPlane(const glm::mat4& view, const glm::mat4& projection,
//	std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& PlaneObjIdx)
//{
//    stbi_set_flip_vertically_on_load(true);
//
//    if (ShouldAddPlane) {
//        PlaneObjIdx = entVector.size();
//
//        std::unique_ptr<PlaneModel> newPlane = std::make_unique<PlaneModel>(currentIndex, "DefaultPlane", PlaneObjIdx);
//        switch (PlaneObjIdx) {
//        case 0:
//            newPlane->position = glm::vec3(0.0f, 0.0f, 0.0f);
//            newPlane->scale = glm::vec3(1.0f, 1.0f, 1.0f);
//            
//            break;
//        case 1:
//            newPlane->position = glm::vec3(1.5f, 0.0f, 0.0f);
//            newPlane->scale = glm::vec3(1.0f, 1.0f, 1.0f);
//            break;
//
//        case 2:
//            newPlane->position = glm::vec3(2.0f, 0.0f, 0.0f);
//            newPlane->scale = glm::vec3(1.0f, 1.0f, 1.0f);
//            break;
//        default:
//            newPlane->position = glm::vec3(2.0f, 2.0f, 0.0f);
//            newPlane->scale = glm::vec3(1.0f, 1.0f, 1.0f);
//            //posx += 1.5;
//            break;
//        }
//
//        newPlane->modelMatrix = glm::mat4(1.0f);
//        newPlane->modelMatrix = glm::translate(newPlane->modelMatrix, newPlane->position);
//        newPlane->modelMatrix = glm::scale(newPlane->modelMatrix, newPlane->scale);
//
//        std::string texPath = GetAssetPath(TEXTURE_PATH);
//		std::string texFile = "github.jpg";
//        Texture tex(texPath + texFile);
//        if (!tex.IsLoaded()) {
//            LOG_WARNING("Failed to load texture: textures/github.jpg");
//            // optional: fall back or continue without texture
//        }
//        //newPlane->tex_ID = TextureManager::Load(texPath + texFile); // cached, persists
//		newPlane->tex_ID = tex.ID();
//        entVector.push_back(std::move(newPlane));
//
//
//        ShouldAddPlane = false; // Reset the flag after adding the plane
//    }
//    
//
//    // ###########
//    static Shader* planeShader = nullptr;
//    if (!planeShader) {
//        planeShader = new Shader("shaders/shader.vert", "shaders/shader.frag");
//    }
//    
//
//    planeShader->Use();
//    planeShader->SetUniformInt("myTexture", 0); // explicit: sample from texture unit 0
//    planeShader->setMat4("projection", projection);
//    planeShader->setMat4("view", view);
//
//    for (const auto& model : entVector) {
//
//        if (auto* plane = dynamic_cast<PlaneModel*>(model.get())) {
//            // Use the pre-calculated model matrix (don't reset it)
//            planeShader->setMat4("model", plane->modelMatrix);
//
//            glActiveTexture(GL_TEXTURE0);
//            glBindTexture(GL_TEXTURE_2D, plane->tex_ID);
//
//            plane->DrawPlane();
//
//            glBindTexture(GL_TEXTURE_2D, 0);
//        }
//    }
//
//}
