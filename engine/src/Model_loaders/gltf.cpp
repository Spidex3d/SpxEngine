#include "gltf.h"

template <typename T>
inline T* GetAccessorData(const json& gltf, const std::vector<unsigned char>& buffer, const json& accessor) {
    const auto& bufferView = gltf["bufferViews"][accessor["bufferView"].get<int>()];
    size_t offset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
    return reinterpret_cast<T*>(const_cast<unsigned char*>(buffer.data() + offset));
}

gltf::gltf(int idx, const std::string& name, int m_modelGltfIdx)
    : gltfVAO(0), gltfVBO(0), gltfEBO(0), idx(idx), name(name), m_modelGltfIdx(m_modelGltfIdx), m_Loaded(false)
{
    entId = idx;
    entName = this->name;
    entObjectIndex = m_modelGltfIdx;
    entTypeID = GLTF_OBJ_MODEL; // ensure defined in globalVar.h

    position = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
    rotation = glm::vec3(0.0f);
    modelMatrix = glm::mat4(1.0f);
}

gltf::~gltf()
{
}

bool gltf::LoadGLTF(const std::string& gltfPath, const std::string& binPath)
{
    try {
        m_mesh = LoadGLTFMesh(gltfPath, binPath); // From your gltf.h
		m_Loaded = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "GLTF Load Error: " << e.what() << std::endl;
        m_Loaded = false;
        return false;
    }
}

std::string gltf::ReadGltfTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Failed to read file: " + path);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::vector<unsigned char> gltf::ReadGltfBinaryFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Failed to read binary: " + path);
    std::vector<unsigned char> buffer(file.tellg());
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    return buffer;
}

GLTFMesh gltf::LoadGLTFMesh(const std::string& gltfPath, const std::string& binPath)
{
    json gltf = json::parse(ReadGltfTextFile(gltfPath));
    std::vector<unsigned char> binData = ReadGltfBinaryFile(binPath);

    GLTFMesh result;

    // ###################                 #################### 
    // ################### not Working ####################
   // inside LoadGLTFMesh(...) - replace the name handling and the per-primitive processing block

// Safe name extraction
    if (gltf.contains("name") && gltf["name"].is_string()) {
        std::string gltfname = gltf["name"].get<std::string>();
        std::cout << "Mesh Name: " << gltfname << std::endl;
    }
    else {
        std::cerr << "Mesh does not have a name." << std::endl;
    }

    // process primitives
    const auto& primitives = gltf["meshes"][0]["primitives"];
    for (const auto& primitive : primitives) {
        const auto& posAcc = gltf["accessors"][primitive["attributes"]["POSITION"].get<int>()];
        float* positions = GetAccessorData<float>(gltf, binData, posAcc);

        // normals
        const auto& normAcc = primitive["attributes"].contains("NORMAL")
            ? gltf["accessors"][primitive["attributes"]["NORMAL"].get<int>()]
            : json{};
        float* normals = normAcc.is_null() ? nullptr : GetAccessorData<float>(gltf, binData, normAcc);

        // uvs
        const auto& uvAcc = primitive["attributes"].contains("TEXCOORD_0")
            ? gltf["accessors"][primitive["attributes"]["TEXCOORD_0"].get<int>()]
            : json{};
        float* uvs = uvAcc.is_null() ? nullptr : GetAccessorData<float>(gltf, binData, uvAcc);

        size_t vertexCount = posAcc["count"];
        std::vector<float> vertexBuffer;
        vertexBuffer.reserve(vertexCount * 8); // estimate

        for (size_t i = 0; i < vertexCount; ++i) {
            // position
            vertexBuffer.push_back(positions[i * 3 + 0]);
            vertexBuffer.push_back(positions[i * 3 + 1]);
            vertexBuffer.push_back(positions[i * 3 + 2]);

            // normal
            if (normals) {
                vertexBuffer.push_back(normals[i * 3 + 0]);
                vertexBuffer.push_back(normals[i * 3 + 1]);
                vertexBuffer.push_back(normals[i * 3 + 2]);
            }
            else {
                vertexBuffer.push_back(0.0f);
                vertexBuffer.push_back(0.0f);
                vertexBuffer.push_back(0.0f);
            }

            // texcoords (flip V)
            if (uvs) {
                float u = uvs[i * 2 + 0];
                float v = 1.0f - uvs[i * 2 + 1];
                vertexBuffer.push_back(u);
                vertexBuffer.push_back(v);
            }
            else {
                vertexBuffer.push_back(0.0f);
                vertexBuffer.push_back(0.0f);
            }
        }

        // Read indices robustly (handle 16-bit and 32-bit index accessors)
        std::vector<uint32_t> indices;
        if (primitive.contains("indices")) {
            const auto& idxAcc = gltf["accessors"][primitive["indices"].get<int>()];
            int componentType = idxAcc["componentType"].get<int>();

            size_t idxCount = idxAcc["count"].get<size_t>();
            // get bufferView+offset and then cast appropriately
            if (componentType == 5123) { // UNSIGNED_SHORT
                uint16_t* idxData16 = GetAccessorData<uint16_t>(gltf, binData, idxAcc);
                indices.resize(idxCount);
                for (size_t i = 0; i < idxCount; ++i) indices[i] = static_cast<uint32_t>(idxData16[i]);
            }
            else if (componentType == 5125) { // UNSIGNED_INT
                uint32_t* idxData32 = GetAccessorData<uint32_t>(gltf, binData, idxAcc);
                indices.assign(idxData32, idxData32 + idxCount);
            }
            else {
                std::cerr << "Unsupported index componentType = " << componentType << std::endl;
            }
        }

        // Create VAO/VBO/EBO for this primitive (keep them in members for now)
        glGenVertexArrays(1, &gltfVAO);
        glGenBuffers(1, &gltfVBO);
        glGenBuffers(1, &gltfEBO);

        glBindVertexArray(gltfVAO);

        // Upload index buffer using the correct size (uint32_t)
        if (!indices.empty()) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gltfEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
        }

        // Upload vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, gltfVBO);
        glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(float), vertexBuffer.data(), GL_STATIC_DRAW);

        // compute integer stride in floats
        int strideFloats = (vertexCount > 0) ? static_cast<int>(vertexBuffer.size() / vertexCount) : 0;
        int strideBytes = strideFloats * static_cast<int>(sizeof(float));

        // basic validation
        if (strideFloats < 5) {
            std::cerr << "Warning: unexpected floats-per-vertex: " << strideFloats << std::endl;
        }

        // position (3 floats)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*)(0));

        // normal (3 floats) at offset 3
        if (strideFloats >= 6) {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*)(3 * sizeof(float)));
        }
        else {
            glDisableVertexAttribArray(1);
        }

        // texcoord (2 floats) at offset 6
        if (strideFloats >= 8) {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, strideBytes, (void*)(6 * sizeof(float)));
        }
        else {
            glDisableVertexAttribArray(2);
        }

        // tangent (optional) at offset 8 if present (pos3 + norm3 + uv2 + tangent3 = 11 floats)
        if (strideFloats >= 11) {
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*)(8 * sizeof(float)));
        }
        else {
            glDisableVertexAttribArray(3);
        }

        // Create SubMesh entry
        SubMesh sub;
        sub.indexCount = indices.size();
        sub.textureID = 0;

        std::cout << "Total floats: " << vertexBuffer.size() << std::endl;
        std::cout << "Total vertices: " << vertexCount << std::endl;
        std::cout << "Floats per vertex: " << (float)vertexBuffer.size() / vertexCount << std::endl;

        if (primitive.contains("material")) {
            int materialIndex = primitive["material"];
            sub.textures = loadTextureForMaterial(gltf, materialIndex, gltfPath);
        }

        glBindVertexArray(0);

        result.submeshes.push_back(sub);
    }
    return result;
        
}

GLuint gltf::loadTextureFromImageIndex(const json& gltf, int imageIndex, const std::string& gltfPath)
{
    if (imageIndex >= (int)gltf["images"].size()) return 0;

    std::string imagePath = gltf["images"][imageIndex]["uri"];
    std::string baseDir = gltfPath.substr(0, gltfPath.find_last_of("/\\") + 1);
    std::string fullImagePath = baseDir + imagePath;

    // Ensure consistent vertical flip behavior across your loaders
    stbi_set_flip_vertically_on_load(true);

    int width = 0, height = 0, nrComponents = 0;
    unsigned char* imageData = stbi_load(fullImagePath.c_str(), &width, &height, &nrComponents, 0);
    if (!imageData) {
        std::cerr << "Failed to load texture: " << fullImagePath << std::endl;
        return 0;
    }

    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB8;
    if (nrComponents == 1) {
        format = GL_RED; internalFormat = GL_R8;
    }
    else if (nrComponents == 3) {
        format = GL_RGB; internalFormat = GL_RGB8;
    }
    else if (nrComponents == 4) {
        format = GL_RGBA; internalFormat = GL_RGBA8;
    }
    else {
        // unexpected, default to RGBA
        format = GL_RGBA; internalFormat = GL_RGBA8;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Use correct internal/format pair
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, imageData);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(imageData);

    return textureID;

}

std::map<std::string, GLuint> gltf::loadTextureForMaterial(const json& gltf, int materialIndex, const std::string& gltfPath)
{
    std::map<std::string, GLuint> textures;

    if (materialIndex < 0 || materialIndex >= gltf["materials"].size()) return textures;
    const auto& material = gltf["materials"][materialIndex];

    // PBR Metallic-Roughness Textures
    if (material.contains("pbrMetallicRoughness")) {
        const auto& pbr = material["pbrMetallicRoughness"];

        if (pbr.contains("baseColorTexture")) {
            int index = pbr["baseColorTexture"]["index"];
            int imageIndex = gltf["textures"][index]["source"];
            textures["baseColor"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
            std::cout << " This model contain baseColor texture" << std::endl;
        }
        else {
            std::cout << " This model dose not contain baseColor texture" << std::endl;
        }

        if (pbr.contains("metallicRoughnessTexture")) {
            int index = pbr["metallicRoughnessTexture"]["index"];
            int imageIndex = gltf["textures"][index]["source"];
            textures["metallicRoughnessMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
            std::cout << " This model contain metallicRoughnessMap texture" << std::endl;
        }
        else {
            std::cout << " This model dose not contain metallicRoughnessTexture texture" << std::endl;
        }
        // } moved down

        // Normal Map
        if (material.contains("normalTexture")) {
            int index = material["normalTexture"]["index"];
            int imageIndex = gltf["textures"][index]["source"];
            textures["normalMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
            std::cout << " This model contain normalMap texture" << std::endl;
        }
        else {
            std::cout << " This model dose not contain narmalMap  texture" << std::endl;
        }

        // Occlusion Map
        if (material.contains("occlusionTexture")) {
            int index = material["occlusionTexture"]["index"];
            int imageIndex = gltf["textures"][index]["source"];
            textures["occlusionMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
            std::cout << " This model contain occlusionMap texture" << std::endl;
        }
        else {
            std::cout << " This model dose not contain occlutionMap texture" << std::endl;
        }

        // Emissive Map
        if (material.contains("emissiveTexture")) {
            int index = material["emissiveTexture"]["index"];
            int imageIndex = gltf["textures"][index]["source"];
            textures["emissiveMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
            std::cout << " This model an contain emissiveMap texture" << std::endl;
        }
        else {
            std::cout << " This model dose not contain emissiveMap texture" << std::endl;
        }

    } // moved
    return textures;
}

//void gltf::DrawGltf(Shader& shader, Camera& camera)
void gltf::DrawGltf()
{
    // Draw each submesh once. Bind baseColor (if any) to unit 0 so your shader's sampler myTexture works.
    for (const auto& sub : m_mesh.submeshes) {
        // bind baseColor to texture unit 0 if present
        GLuint baseTex = 0;
        auto it = sub.textures.find("baseColor");
        if (it != sub.textures.end()) baseTex = it->second;

        if (baseTex != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, baseTex);
        }

        if (gltfVAO != 0 && sub.indexCount > 0) {
            glBindVertexArray(gltfVAO);
            // Use GL_UNSIGNED_INT since we uploaded uint32_t indices
            glDrawElements(GL_TRIANGLES, (GLsizei)sub.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        if (baseTex != 0) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Ensure active texture unit reset
        glActiveTexture(GL_TEXTURE0);
    }

}

void gltf::DestroyGLTFMesh(GLTFMesh& m_mesh)
{
    for (auto& subMesh : m_mesh.submeshes) {
        if (gltfEBO) glDeleteBuffers(1, &gltfEBO);
        if (gltfVBO) glDeleteBuffers(1, &gltfVBO);
        if (gltfVAO) glDeleteVertexArrays(1, &gltfVAO);
        if (subMesh.textureID) glDeleteTextures(1, &subMesh.textureID);
    }
    m_mesh.submeshes.clear();
}
