#include "entity.h"
#include "stb/stb_image.h"
#include "../include/asset_path.h"
#include "../include/log.h"

#include <engine.h>
#include "../src/Textures/textures.h"
#include "../include/shader.h"
#include "../src/Model_loaders/objLoader.h"
#include "../src/Model_loaders/gltf.h"
#include "../src/Sky/skyBox.h"
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
// helper function to set common shader uniforms for all entity types; called by each RenderX function before drawing

void Entity::loadShader(Shader* shader, const glm::mat4& view, const glm::mat4& projection)
{
    shader->Use();
    shader->SetUniformInt("myTexture", 0); // ensure sampler unit 0
    shader->setVec3("u_albedo", glm::vec3(1.0f, 1.0f, 1.0f)); // fallback color
    shader->setVec3("u_lightDir", glm::vec3(0.5f, 1.0f, 0.3f)); // optional
    shader->setVec3("u_lightColor", glm::vec3(1.0f));
    // end new
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);
}

//void Entity::CreateSkyBox(std::vector<std::unique_ptr<GameObj>>& entVector,
//    int& currentIndex, int& skyObjIdx, const std::string& folderPath, const std::string& skyFile, const glm::vec3& position)
//{
//}

// construct a skybox entity from a file path (for now, we can just set up the cube geometry and load a cubemap texture;
// the actual shader and rendering will be handled in RenderSkyBox)

//void Entity::CreateSkyBox(std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex,
//    int& skyObjIdx, const std::string& folderPath, const glm::vec3& position)
    void Entity::CreateSkyBox(std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex,
        int& skyObjIdx, const std::string& folderPath, const glm::vec3& position)


{
    if (folderPath.empty()) {
        LOG_WARNING("CreateSkyBox: empty folderPath");
        return;
    }

   // std::string name = "Skybox_" + std::to_string(skyObjIdx);
    std::string name = "Skybox";
    auto sky = std::make_unique<LoadSkybox>(currentIndex, name, skyObjIdx);

    // create geometry
    sky->SkyBox();

    // load cubemap images from folder (expects 1 or more matched images)
    //if (!sky->LoadFromFolder(folderPath)) {
    if (!sky->LoadFromFolder(folderPath)) {
    //if (!sky->LoadFromFolder(folderPath)) {
        LOG_WARNING("CreateSkyBox: failed to load textures from " << folderPath);
        // still can push sky (empty) if you want; here we abort
        return;
    }

    // set transform
    sky->position = position;
    sky->modelMatrix = glm::translate(glm::mat4(1.0f), sky->position);
    sky->modelMatrix = glm::scale(sky->modelMatrix, sky->scale);

    entVector.push_back(std::move(sky));
    ++currentIndex;
    ++skyObjIdx;
    LOG_INFO("CreateSkyBox: added skybox from " << folderPath);
}
// add a sky to the scene
void Entity::RenderSkyBox(Shader* shader, const glm::mat4& view, const glm::mat4& projection,
    std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& m_SkyIdx, int& selectedEntityId)
{
	// Render the sky box with the given shader, view, and projection matrices.
    // The skybox should ignore the camera's translation to always appear at the same position relative to the camera.
    if (!shader) {
        LOG_WARNING("Entity::RenderSkyBox called without shader; skipping draw.");
        return;
    }

    // Use the shader (should be the skybox shader)
    shader->Use();

    // Render all skybox GameObjs (there will usually be one)
    for (const auto& model : entVector) {
        if (!model) continue;
        if (!model->isVisible) continue;

        if (auto* sky = dynamic_cast<LoadSkybox*>(model.get())) {
            // SkyBox::DrawSkyBox sets view/projection/uniforms and binds cubemap
            sky->DrawSkyBox(shader, view, projection);
        }
    }

}

void Entity::CreateGltfFromFile(std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex,
    int& m_modelGltfIdx, const std::string& modelPath, const glm::vec3& position)
{
    if (modelPath.empty()) {
        LOG_WARNING("CreateGltfFromFile: empty modelPath");
        return;
    }

    LOG_INFO("CreateGltfFromFile: loading " << modelPath);

    // Derive a friendly base name for the object
    size_t p = modelPath.find_last_of("/\\");
    std::string baseName = (p == std::string::npos) ? modelPath : modelPath.substr(p + 1);

    // Determine binary buffer path (for .gltf JSON files that reference a .bin)
    std::string binPath;
    std::string ext;
    {
        size_t dot = modelPath.find_last_of('.');
        if (dot != std::string::npos) ext = modelPath.substr(dot);
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    }

    if (ext == ".gltf") {
        // Parse the JSON to find the first buffer uri if present
        try {
            std::ifstream f(modelPath);
            if (f) {
                json j = json::parse(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
                if (j.contains("buffers") && j["buffers"].is_array() && !j["buffers"].empty()) {
                    const auto& buf = j["buffers"][0];
                    if (buf.contains("uri") && buf["uri"].is_string()) {
                        std::string uri = buf["uri"].get<std::string>();
                        // ignore data: URIs (embedded)
                        if (!uri.rfind("data:", 0) == 0) {
                            std::string baseDir = modelPath.substr(0, modelPath.find_last_of("/\\") + 1);
                            binPath = baseDir + uri;
                        }
                    }
                }
            }
        }
        catch (const std::exception& e) {
            LOG_WARNING("CreateGltfFromFile: failed parsing gltf JSON to find bin: " << e.what());
            binPath.clear();
        }
    }
    else if (ext == ".glb") {
        // For now, pass empty binPath; your gltf::LoadGLTF may not support .glb parsing.
        binPath = "";
    }
    else {
        // Not a glTF; bail.
        LOG_WARNING("CreateGltfFromFile: unsupported extension: " << ext);
        return;
    }

    // Create loader instance (gltf derives from GameObj)
    auto loader = std::make_unique<gltf>(currentIndex, baseName, m_modelGltfIdx);

    // Try to load (LoadGLTF returns bool)
    bool ok = loader->LoadGLTF(modelPath, binPath);
    if (!ok || !loader->IsLoaded()) {
        LOG_WARNING("CreateGltfFromFile: gltf loader failed for " << modelPath << " - adding fallback cube");

        auto fallback = std::make_unique<CubeModel>(currentIndex, baseName + "_fallback", m_modelGltfIdx);
        fallback->position = position;
        fallback->modelMatrix = glm::translate(glm::mat4(1.0f), fallback->position);
        fallback->modelMatrix = glm::scale(fallback->modelMatrix, fallback->scale);
        entVector.push_back(std::move(fallback));
        ++currentIndex;
        ++m_modelGltfIdx;
        return;
    }

    // Set transform on loader and push into engine entity vector
    loader->position = position;
    loader->scale = glm::vec3(1.0f);
    loader->modelMatrix = glm::translate(glm::mat4(1.0f), loader->position);
    loader->modelMatrix = glm::scale(loader->modelMatrix, loader->scale);

    // IMPORTANT: LoadGLTF already created VAO/VBO/EBO & textures inside loader (LoadGLTFMesh).
    // Push into the main entity list so Engine will render it.
    entVector.push_back(std::move(loader));

    ++currentIndex;
    ++m_modelGltfIdx;
}
void Entity::RenderGltfModel(Shader* shader, const glm::mat4& view, const glm::mat4& projection,
    std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& m_modelGltfIdx, int& selectedEntityId)
{
    if (!shader) {
        LOG_WARNING("Entity::RenderGltfModel called without shader; skipping draw.");
        return;
    }

    // Setup shared shader uniforms (projection/view etc.)
    loadShader(shader, view, projection);

    // derive camera world position from view matrix: inverse(view) * (0,0,0,1)
    glm::mat4 invView = glm::inverse(view);
    glm::vec3 viewPos = glm::vec3(invView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    shader->setVec3("u_viewPos", viewPos);

    for (const auto& model : entVector) {
        if (!model) continue;
        if (!model->isVisible) continue;

        if (auto* gltfModel = dynamic_cast<gltf*>(model.get())) {
            // set model matrix (same for all submeshes)
            shader->setMat4("model", gltfModel->modelMatrix);

            // selection highlight (per-model)
            int isSelected = (gltfModel->entId == selectedEntityId) ? 1 : 0;
            shader->SetUniformInt("u_selected", isSelected);
            shader->setVec3("u_highlightColor", glm::vec3(0.2f, 0.2f, 0.8f));

            // For each submesh set per-material uniforms and draw
            for (const auto& sub : gltfModel->m_mesh.submeshes) {
                // baseColor texture presence?
                bool hasTex = (sub.textures.find("baseColor") != sub.textures.end() && sub.textures.at("baseColor") != 0);
                shader->SetUniformInt("u_useTexture", hasTex ? 1 : 0);

                // set albedo (baseColorFactor) as fallback
                shader->setVec3("u_albedo", sub.baseColorFactor);

                // set shininess & specular color
                // NOTE: use your Shader float setter if named differently
                shader->SetUniformFloat("u_shininess", sub.shininess);
                shader->setVec3("u_specularColor", sub.specularFactor);

                // bind baseColor to texture unit 0 if present
                if (hasTex) {
                    GLuint tex = sub.textures.at("baseColor");
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, tex);
                }

               // bool hasTex = (sub.textures.find("baseColor") != sub.textures.end() && sub.textures.at("baseColor") != 0);
                /*LOG_INFO("GLTF submesh: hasTex=" << hasTex
                    << " baseColor=" << sub.baseColorFactor.r << "," << sub.baseColorFactor.g << "," << sub.baseColorFactor.b
                    << " shininess=" << sub.shininess
                    << " specular=" << sub.specularFactor.r << "," << sub.specularFactor.g << "," << sub.specularFactor.b
                    << " indexCount=" << sub.indexCount
                    << " texturesCount=" << sub.textures.size());*/

                // draw submesh
                if (sub.vao != 0 && sub.indexCount > 0) {
                    glBindVertexArray(sub.vao);
                    glDrawElements(GL_TRIANGLES, (GLsizei)sub.indexCount, GL_UNSIGNED_INT, 0);
                    glBindVertexArray(0);
                }

                // unbind texture if bound
                if (hasTex) {
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }

            // reset active texture
            glActiveTexture(GL_TEXTURE0);
        }
    }

    // reset selection uniform (optional)
    shader->SetUniformInt("u_selected", 0);


    //if (!shader) {
    //    LOG_WARNING("Entity::RenderGltfModel called without shader; skipping draw.");
    //    return;
    //}

    //// Setup shared shader uniforms (projection/view etc.)
    //loadShader(shader, view, projection);

    //for (const auto& model : entVector) {
    //    if (!model) continue;
    //    if (!model->isVisible) continue;

    //    if (auto* gltfModel = dynamic_cast<gltf*>(model.get())) {
    //        // Model matrix
    //        shader->setMat4("model", gltfModel->modelMatrix);

    //        // Selection highlight
    //        int isSelected = (gltfModel->entId == selectedEntityId) ? 1 : 0;
    //        shader->SetUniformInt("u_selected", isSelected);
    //        shader->setVec3("u_highlightColor", glm::vec3(0.2f, 0.2f, 0.8f));

    //        // Decide whether the model has a baseColor / diffuse texture (simple heuristic)
    //        bool hasTex = false;
    //        for (const auto& sub : gltfModel->m_mesh.submeshes) {
    //            // sub.textures is a map<string, GLuint> in your gltf::SubMesh
    //            auto it = sub.textures.find("baseColor");
    //            if (it != sub.textures.end() && it->second != 0) { hasTex = true; break; }
    //        }
    //        shader->SetUniformInt("u_useTexture", hasTex ? 1 : 0);

    //        // Set fallback albedo if no texture
    //        if (!hasTex) {
    //            shader->setVec3("u_albedo", glm::vec3(1.0f));
    //        }

    //        // Draw the glTF model (your gltf::DrawGltf binds textures and issues draw calls per-submesh)
    //        gltfModel->DrawGltf();

    //        // Optionally reset bound textures (DrawGltf unbinds textures itself, but to be safe)
    //        glActiveTexture(GL_TEXTURE0);
    //        glBindTexture(GL_TEXTURE_2D, 0);
    //    }
    //}

    //// reset selection uniform (optional)
    //shader->SetUniformInt("u_selected", 0);
}


void Entity::CreateObjFromFile(std::vector<std::unique_ptr<GameObj>>& entVector,
    int& currentIndex, int& modelObjIdx, const std::string& modelPath, const glm::vec3& position)
{
    if (modelPath.empty()) {
        LOG_WARNING("CreateObjFromFile: empty modelPath");
        return;
    }

    LOG_INFO("CreateObjFromFile: loading " << modelPath);

    stbi_set_flip_vertically_on_load(true);

    // Derive a short name from the filename for the loader's name param
    size_t p = modelPath.find_last_of("/\\");
    std::string baseName = (p == std::string::npos) ? modelPath : modelPath.substr(p + 1);

    // Construct loader (objLoader inherits GameObj)
    auto loader = std::make_unique<objLoader>(currentIndex, baseName, modelObjIdx);

    // Try to load the OBJ file
    bool ok = loader->Loadobj(modelPath);
    if (!ok) {
        LOG_WARNING("CreateObjFromFile: objLoader failed to load " << modelPath << " - adding fallback cube");
        // Fallback: create a CubeModel so something appears
        auto fallback = std::make_unique<CubeModel>(currentIndex, baseName + "_fallback", modelObjIdx);
        fallback->position = position;
        fallback->modelMatrix = glm::translate(glm::mat4(1.0f), fallback->position);
        fallback->modelMatrix = glm::scale(fallback->modelMatrix, fallback->scale);
        entVector.push_back(std::move(fallback));
        ++currentIndex;
        ++modelObjIdx;
        return;
    }

    // Prepare GPU buffers (VAO/VBO) from loaded vertex data
    loader->objModels();

    // Set transform and other properties
    loader->position = position;
    loader->scale = glm::vec3(1.0f);
    loader->modelMatrix = glm::translate(glm::mat4(1.0f), loader->position);
    loader->modelMatrix = glm::scale(loader->modelMatrix, loader->scale);

    // If you want to set a texture path on the GameObj wrapper, use loader->texPath / loader->tex_ID accordingly.
    // Push into the entity vector (Engine will own and render it)
    entVector.push_back(std::move(loader));

    ++currentIndex;
    ++modelObjIdx;
}

void Entity::RenderObjModel(Shader* shader, const glm::mat4& view, const glm::mat4& projection,
    std::vector<std::unique_ptr<GameObj>>& entVector, int& currentIndex, int& /*unused*/, int& selectedEntityId)
{
    if (!shader) {
        LOG_WARNING("Entity::RenderObjModel called without shader; skipping draw.");
        return;
    }
	loadShader(shader, view, projection);
    
    for (const auto& model : entVector) {
        if (!model) continue;
        if (!model->isVisible) continue;

        // objLoader derives from GameObj; render only those
        if (auto* objmodel = dynamic_cast<objLoader*>(model.get())) {
            
            // new
            // texture usage: ask the loader whether it has a diffuse texture
            bool hasTex = objmodel->HasDiffuseTexture();
            shader->SetUniformInt("u_useTexture", hasTex ? 1 : 0);
            // end new

            shader->setMat4("model", objmodel->modelMatrix);

            int isSelected = (objmodel->entId == selectedEntityId) ? 1 : 0;
            shader->SetUniformInt("u_selected", isSelected);
            shader->setVec3("u_highlightColor", glm::vec3(0.2f, 0.2f, 0.8f));

            if (hasTex) {
                GLuint tex = objmodel->GetFirstDiffuseTexture();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
            }

            // Let the loader draw its VAO and bind textures as implemented in objDrawModels()
            objmodel->objDrawModels();

            // Unbind textures (objDrawModels already does this), but keep shader state consistent
            if (hasTex) {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }

    shader->SetUniformInt("u_selected", 0);
}


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
    if (!shader) {
        LOG_WARNING("Entity::RenderCube called without shader; skipping draw.");
        return;
    }

    // set shared uniforms once
    loadShader(shader, view, projection);

    for (const auto& model : entVector) {
        if (!model) continue;
        if (!model->isVisible) continue;

        if (auto* cube = dynamic_cast<CubeModel*>(model.get())) {
            // per-object model matrix
            shader->setMat4("model", cube->modelMatrix);

            // selection highlight
            int isSelected = (cube->entId == selectedEntityId) ? 1 : 0;
            shader->SetUniformInt("u_selected", isSelected);
            shader->setVec3("u_highlightColor", glm::vec3(0.2f, 0.2f, 0.8f));

            // Does this object have a texture?
            bool hasTex = (cube->tex_ID != 0);
            shader->SetUniformInt("u_useTexture", hasTex ? 1 : 0);

            if (hasTex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cube->tex_ID);
            }
            else {
                // optionally give each object an albedo color (fallback)
                // you can expose a per-object color in GameObj, for now use white
                shader->setVec3("u_albedo", glm::vec3(1.0f, 1.0f, 1.0f));
            }

            cube->DrawCube();

            // cleanup: unbind only if we bound a texture
            if (hasTex) {
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

	loadShader(shader, view, projection);

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

            // Does this object have a texture?
            bool hasTex = (plane->tex_ID != 0);
            shader->SetUniformInt("u_useTexture", hasTex ? 1 : 0);

            if (hasTex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, plane->tex_ID);
            }
            else {
                // optionally give each object an albedo color (fallback)
                // you can expose a per-object color in GameObj, for now use white
                shader->setVec3("u_albedo", glm::vec3(1.0f, 1.0f, 1.0f));
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

	loadShader(shader, view, projection);
    
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

            // Does this object have a texture?
            bool hasTex = (floor->tex_ID != 0);
            shader->SetUniformInt("u_useTexture", hasTex ? 1 : 0);

            if (hasTex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, floor->tex_ID);
            }
            else {
                // optionally give each object an albedo color (fallback)
                // you can expose a per-object color in GameObj, for now use white
                shader->setVec3("u_albedo", glm::vec3(1.0f, 1.0f, 1.0f));
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
