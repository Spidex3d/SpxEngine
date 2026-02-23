#include "gltf.h"
#include <cstring> // memcpy
#include <glm/gtc/type_ptr.hpp>

template <typename T>
inline T* GetAccessorData(const json& gltf, const std::vector<unsigned char>& buffer, const json& accessor) {
    const auto& bufferView = gltf["bufferViews"][accessor["bufferView"].get<int>()];
    size_t offset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
    return reinterpret_cast<T*>(const_cast<unsigned char*>(buffer.data() + offset));
}

// ComputeTangents helper (same as you have)
static void ComputeTangents(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec2>& uvs,
    const std::vector<glm::vec3>& normals,
    const std::vector<uint32_t>& indices,
    std::vector<glm::vec3>& outTangents)
{
    size_t vcount = positions.size();
    outTangents.assign(vcount, glm::vec3(0.0f));

    // accumulate tangents per triangle
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const glm::vec3& p0 = positions[i0];
        const glm::vec3& p1 = positions[i1];
        const glm::vec3& p2 = positions[i2];

        const glm::vec2& uv0 = uvs[i0];
        const glm::vec2& uv1 = uvs[i1];
        const glm::vec2& uv2 = uvs[i2];

        glm::vec3 e1 = p1 - p0;
        glm::vec3 e2 = p2 - p0;

        glm::vec2 duv1 = uv1 - uv0;
        glm::vec2 duv2 = uv2 - uv0;

        float denom = duv1.x * duv2.y - duv2.x * duv1.y;
        if (fabs(denom) < 1e-8f) {
            // degenerative UV triangle; skip tangent contribution
            continue;
        }
        float r = 1.0f / denom;

        glm::vec3 tangent = (e1 * duv2.y - e2 * duv1.y) * r;

        outTangents[i0] += tangent;
        outTangents[i1] += tangent;
        outTangents[i2] += tangent;
    }

    // orthonormalize tangents with respect to normals
    for (size_t i = 0; i < vcount; ++i) {
        glm::vec3 t = outTangents[i];
        glm::vec3 n = (i < normals.size()) ? normals[i] : glm::vec3(0.0f, 0.0f, 1.0f);
        // Gram-Schmidt orthogonalize
        glm::vec3 orth = t - n * glm::dot(n, t);
        if (glm::dot(orth, orth) > 1e-8f) {
            outTangents[i] = glm::normalize(orth);
        }
        else {
            outTangents[i] = glm::vec3(1.0f, 0.0f, 0.0f); // fallback tangent
        }
    }
}

// ctor/dtor
gltf::gltf(int idx_, const std::string& name_, int m_modelGltfIdx_)
    : idx(idx_), name(name_), m_modelGltfIdx(m_modelGltfIdx_), m_Loaded(false)
{
    entId = idx;
    entName = this->name;
    entObjectIndex = m_modelGltfIdx;
    entTypeID = GLTF_OBJ_MODEL;

    position = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
    rotation = glm::vec3(0.0f);
    modelMatrix = glm::mat4(1.0f);
}

gltf::~gltf()
{
    DestroyGLTFMesh(m_mesh);
}

bool gltf::LoadGLTF(const std::string& gltfPath, const std::string& binPath)
{
    try {
        m_mesh = LoadGLTFMesh(gltfPath, binPath);
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
    std::vector<unsigned char> buffer((size_t)file.tellg());
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    return buffer;
}

GLTFMesh gltf::LoadGLTFMesh(const std::string& gltfPath, const std::string& binPath)
{
    json gltf = json::parse(ReadGltfTextFile(gltfPath));
    std::vector<unsigned char> binData;
    if (!binPath.empty()) {
        binData = ReadGltfBinaryFile(binPath);
    }

    GLTFMesh result;

    // Safe name extraction
    if (gltf.contains("name") && gltf["name"].is_string()) {
        std::string gltfname = gltf["name"].get<std::string>();
        std::cout << "Mesh Name: " << gltfname << std::endl;
    }
    else {
        std::cerr << "Mesh does not have a name." << std::endl;
    }

    // process primitives (each primitive -> one SubMesh with its own VAO/VBO/EBO)
    const auto& primitives = gltf["meshes"][0]["primitives"];
    for (const auto& primitive : primitives) {
        // POSITION accessor
        const auto& posAcc = gltf["accessors"][primitive["attributes"]["POSITION"].get<int>()];
        float* positions = GetAccessorData<float>(gltf, binData, posAcc);

        // NORMAL accessor (may be absent)
        const auto& normAcc = primitive["attributes"].contains("NORMAL")
            ? gltf["accessors"][primitive["attributes"]["NORMAL"].get<int>()]
            : json{};
        float* normals = normAcc.is_null() ? nullptr : GetAccessorData<float>(gltf, binData, normAcc);

        // TEXCOORD_0 accessor (may be absent)
        const auto& uvAcc = primitive["attributes"].contains("TEXCOORD_0")
            ? gltf["accessors"][primitive["attributes"]["TEXCOORD_0"].get<int>()]
            : json{};
        float* uvs = uvAcc.is_null() ? nullptr : GetAccessorData<float>(gltf, binData, uvAcc);

        size_t vertexCount = posAcc["count"];
        std::vector<glm::vec3> positionsVec(vertexCount);
        std::vector<glm::vec3> normalsVec(vertexCount);
        std::vector<glm::vec2> uvsVec(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            positionsVec[i] = glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
            if (normals) normalsVec[i] = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
            else normalsVec[i] = glm::vec3(0.0f, 0.0f, 1.0f);
            if (uvs) uvsVec[i] = glm::vec2(uvs[i * 2 + 0], 1.0f - uvs[i * 2 + 1]); // flip V
            else uvsVec[i] = glm::vec2(0.0f, 0.0f);
        }

        // Read indices robustly (handle 16-bit and 32-bit index accessors)
        std::vector<uint32_t> indices;
        if (primitive.contains("indices")) {
            const auto& idxAcc = gltf["accessors"][primitive["indices"].get<int>()];
            int componentType = idxAcc["componentType"].get<int>();
            size_t idxCount = idxAcc["count"].get<size_t>();

            if (componentType == 5123) { // UNSIGNED_SHORT
                uint16_t* idxData16 = GetAccessorData<uint16_t>(gltf, binData, idxAcc);
                indices.resize(idxCount);
                for (size_t ii = 0; ii < idxCount; ++ii) indices[ii] = static_cast<uint32_t>(idxData16[ii]);
            }
            else if (componentType == 5125) { // UNSIGNED_INT
                uint32_t* idxData32 = GetAccessorData<uint32_t>(gltf, binData, idxAcc);
                indices.assign(idxData32, idxData32 + idxCount);
            }
            else {
                std::cerr << "Unsupported index componentType = " << componentType << std::endl;
            }
        }

        // Compute or read tangents
        std::vector<glm::vec3> tangentsVec(vertexCount, glm::vec3(0.0f));
        if (primitive["attributes"].contains("TANGENT")) {
            const auto& tanAcc = gltf["accessors"][primitive["attributes"]["TANGENT"].get<int>()];
            float* tanData = GetAccessorData<float>(gltf, binData, tanAcc); // typically vec4
            for (size_t i = 0; i < vertexCount; ++i) {
                size_t base = i * 4;
                tangentsVec[i] = glm::vec3(tanData[base + 0], tanData[base + 1], tanData[base + 2]);
            }
        }
        else {
            ComputeTangents(positionsVec, uvsVec, normalsVec, indices, tangentsVec);
        }

        // build interleaved vertex buffer including tangents (pos3 + norm3 + uv2 + tan3 = 11 floats)
        std::vector<float> vertexBuffer;
        vertexBuffer.reserve(vertexCount * 11);
        for (size_t i = 0; i < vertexCount; ++i) {
            vertexBuffer.push_back(positionsVec[i].x);
            vertexBuffer.push_back(positionsVec[i].y);
            vertexBuffer.push_back(positionsVec[i].z);

            vertexBuffer.push_back(normalsVec[i].x);
            vertexBuffer.push_back(normalsVec[i].y);
            vertexBuffer.push_back(normalsVec[i].z);

            vertexBuffer.push_back(uvsVec[i].x);
            vertexBuffer.push_back(uvsVec[i].y);

            vertexBuffer.push_back(tangentsVec[i].x);
            vertexBuffer.push_back(tangentsVec[i].y);
            vertexBuffer.push_back(tangentsVec[i].z);
        }

        // Create a SubMesh and upload GPU buffers for this primitive
        SubMesh sub;
        sub.indexCount = indices.size();

        glGenVertexArrays(1, &sub.vao);
        glGenBuffers(1, &sub.vbo);
        glGenBuffers(1, &sub.ebo);

        glBindVertexArray(sub.vao);

        // EBO (store 32-bit indices)
        if (!indices.empty()) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
        }

        // VBO
        glBindBuffer(GL_ARRAY_BUFFER, sub.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(float), vertexBuffer.data(), GL_STATIC_DRAW);

        int strideFloats = (vertexCount > 0) ? static_cast<int>(vertexBuffer.size() / vertexCount) : 0;
        int strideBytes = strideFloats * static_cast<int>(sizeof(float));

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

        // tangent (3 floats) at offset 8
        if (strideFloats >= 11) {
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*)(8 * sizeof(float)));
        }
        else {
            glDisableVertexAttribArray(3);
        }

        glBindVertexArray(0);

        // load material textures for this primitive (if any)
        if (primitive.contains("material")) {
            int materialIndex = primitive["material"];
            sub.textures = loadTextureForMaterial(gltf, materialIndex, gltfPath);
        }
		//###################################### shinyness factor from roughness (for Blinn-Phong fallback) ######################################
        

// try to extract pbrMetallicRoughness factors (defaults handled)
        if (gltf.contains("materials") && gltf["materials"].is_array()) {
            int materialIndex = primitive["material"].get<int>();
            const auto& mat = gltf["materials"][materialIndex];

            // baseColorFactor (array[4]) default [1,1,1,1]
            if (mat.contains("pbrMetallicRoughness") && mat["pbrMetallicRoughness"].contains("baseColorFactor")) {
                auto arr = mat["pbrMetallicRoughness"]["baseColorFactor"];
                sub.baseColorFactor = glm::vec3(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
            }

            // roughnessFactor / metallicFactor
            if (mat.contains("pbrMetallicRoughness")) {
                auto pbr = mat["pbrMetallicRoughness"];
                if (pbr.contains("roughnessFactor")) sub.roughnessFactor = pbr["roughnessFactor"].get<float>();
                if (pbr.contains("metallicFactor"))  sub.metallicFactor = pbr["metallicFactor"].get<float>();
            }

            // check KHR_materials_pbrSpecularGlossiness extension (specularFactor + glossinessFactor)
            if (mat.contains("extensions") && mat["extensions"].contains("KHR_materials_pbrSpecularGlossiness")) {
                auto ext = mat["extensions"]["KHR_materials_pbrSpecularGlossiness"];
                if (ext.contains("specularFactor")) {
                    auto sf = ext["specularFactor"];
                    sub.specularFactor = glm::vec3(sf[0].get<float>(), sf[1].get<float>(), sf[2].get<float>());
                }
                if (ext.contains("glossinessFactor")) {
                    sub.glossinessFactor = ext["glossinessFactor"].get<float>();
                }
            }

            // Derive shininess: prefer glossinessFactor if present; otherwise map roughness -> shininess
            if (sub.glossinessFactor > 0.0f) {
                // map glossiness 0..1 -> shininess (example scale)
                const float maxShininess = 256.0f;
                sub.shininess = glm::clamp(sub.glossinessFactor * maxShininess, 1.0f, maxShininess);
            }
            else {
                // convert roughness (0..1) -> shininess, inverse relationship
                // roughness 0 => very shiny (high shininess), roughness 1 => low shininess
                const float maxShininess = 256.0f;
                sub.shininess = glm::clamp((1.0f - sub.roughnessFactor) * maxShininess, 1.0f, maxShininess);
            }
        }
		// ###################################### end shinyness ######################################
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

    if (materialIndex < 0 || materialIndex >= (int)gltf["materials"].size()) return textures;
    const auto& material = gltf["materials"][materialIndex];

    // PBR Metallic-Roughness Textures
    if (material.contains("pbrMetallicRoughness")) {
        const auto& pbr = material["pbrMetallicRoughness"];

        if (pbr.contains("baseColorTexture")) {
            int index = pbr["baseColorTexture"]["index"];
            int imageIndex = gltf["textures"][index]["source"];
            textures["baseColor"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
            std::cout << " This model contains baseColor texture" << std::endl;
        }
        else {
            std::cout << " This model does not contain baseColor texture" << std::endl;
        }

        if (pbr.contains("metallicRoughnessTexture")) {
            int index = pbr["metallicRoughnessTexture"]["index"];
            int imageIndex = gltf["textures"][index]["source"];
            textures["metallicRoughnessMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
            std::cout << " This model contains metallicRoughnessMap texture" << std::endl;
        }
        else {
            std::cout << " This model does not contain metallicRoughnessTexture texture" << std::endl;
        }
    }

    // Normal Map
    if (material.contains("normalTexture")) {
        int index = material["normalTexture"]["index"];
        int imageIndex = gltf["textures"][index]["source"];
        textures["normalMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
        std::cout << " This model contains normalMap texture" << std::endl;
    }

    // Occlusion Map
    if (material.contains("occlusionTexture")) {
        int index = material["occlusionTexture"]["index"];
        int imageIndex = gltf["textures"][index]["source"];
        textures["occlusionMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
        std::cout << " This model contains occlusionMap texture" << std::endl;
    }

    // Emissive Map
    if (material.contains("emissiveTexture")) {
        int index = material["emissiveTexture"]["index"];
        int imageIndex = gltf["textures"][index]["source"];
        textures["emissiveMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
        std::cout << " This model contains emissiveMap texture" << std::endl;
    }

    return textures;
}

void gltf::DrawGltf()
{
    // Draw each submesh once. Bind baseColor (if any) to unit 0 so your shader's sampler myTexture works.
    for (const auto& sub : m_mesh.submeshes) {
        GLuint baseTex = 0;
        auto it = sub.textures.find("baseColor");
        if (it != sub.textures.end()) baseTex = it->second;

        if (baseTex != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, baseTex);
        }

        if (sub.vao != 0 && sub.indexCount > 0) {
            glBindVertexArray(sub.vao);
            glDrawElements(GL_TRIANGLES, (GLsizei)sub.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        if (baseTex != 0) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glActiveTexture(GL_TEXTURE0);
    }
}

void gltf::DestroyGLTFMesh(GLTFMesh& m_mesh)
{
    // Delete per-submesh GPU buffers and textures
    for (auto& sub : m_mesh.submeshes) {
        if (sub.ebo) { glDeleteBuffers(1, &sub.ebo); sub.ebo = 0; }
        if (sub.vbo) { glDeleteBuffers(1, &sub.vbo); sub.vbo = 0; }
        if (sub.vao) { glDeleteVertexArrays(1, &sub.vao); sub.vao = 0; }

        for (auto& kv : sub.textures) {
            if (kv.second) { glDeleteTextures(1, &kv.second); kv.second = 0; }
        }
        sub.textures.clear();
    }
    m_mesh.submeshes.clear();
}


//#include "gltf.h"
//
//template <typename T>
//inline T* GetAccessorData(const json& gltf, const std::vector<unsigned char>& buffer, const json& accessor) {
//    const auto& bufferView = gltf["bufferViews"][accessor["bufferView"].get<int>()];
//    size_t offset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
//    return reinterpret_cast<T*>(const_cast<unsigned char*>(buffer.data() + offset));
//}
//// ###################################### new ############################################
//static void ComputeTangents(
//    const std::vector<glm::vec3>& positions,
//    const std::vector<glm::vec2>& uvs,
//    const std::vector<glm::vec3>& normals,
//    const std::vector<uint32_t>& indices,
//    std::vector<glm::vec3>& outTangents)
//{
//    size_t vcount = positions.size();
//    outTangents.assign(vcount, glm::vec3(0.0f));
//
//    // accumulate tangents per triangle
//    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
//        uint32_t i0 = indices[i + 0];
//        uint32_t i1 = indices[i + 1];
//        uint32_t i2 = indices[i + 2];
//
//        const glm::vec3& p0 = positions[i0];
//        const glm::vec3& p1 = positions[i1];
//        const glm::vec3& p2 = positions[i2];
//
//        const glm::vec2& uv0 = uvs[i0];
//        const glm::vec2& uv1 = uvs[i1];
//        const glm::vec2& uv2 = uvs[i2];
//
//        glm::vec3 e1 = p1 - p0;
//        glm::vec3 e2 = p2 - p0;
//
//        glm::vec2 duv1 = uv1 - uv0;
//        glm::vec2 duv2 = uv2 - uv0;
//
//        float denom = duv1.x * duv2.y - duv2.x * duv1.y;
//        if (fabs(denom) < 1e-8f) {
//            // degenerative UV triangle; skip tangent contribution
//            continue;
//        }
//        float r = 1.0f / denom;
//
//        glm::vec3 tangent = (e1 * duv2.y - e2 * duv1.y) * r;
//
//        outTangents[i0] += tangent;
//        outTangents[i1] += tangent;
//        outTangents[i2] += tangent;
//    }
//
//    // orthonormalize tangents with respect to normals
//    for (size_t i = 0; i < vcount; ++i) {
//        glm::vec3 t = outTangents[i];
//        glm::vec3 n = (i < normals.size()) ? normals[i] : glm::vec3(0.0f, 0.0f, 1.0f);
//        // Gram-Schmidt orthogonalize
//        glm::vec3 orth = t - n * glm::dot(n, t);
//        if (glm::dot(orth, orth) > 1e-8f) {
//            outTangents[i] = glm::normalize(orth);
//        }
//        else {
//            outTangents[i] = glm::vec3(1.0f, 0.0f, 0.0f); // fallback tangent
//        }
//    }
//}
//
//// #######################################################################################
//
//gltf::gltf(int idx, const std::string& name, int m_modelGltfIdx)
//    : gltfVAO(0), gltfVBO(0), gltfEBO(0), idx(idx), name(name), m_modelGltfIdx(m_modelGltfIdx), m_Loaded(false)
//{
//    entId = idx;
//    entName = this->name;
//    entObjectIndex = m_modelGltfIdx;
//    entTypeID = GLTF_OBJ_MODEL; // ensure defined in globalVar.h
//
//    position = glm::vec3(0.0f);
//    scale = glm::vec3(1.0f);
//    rotation = glm::vec3(0.0f);
//    modelMatrix = glm::mat4(1.0f);
//}
//
//gltf::~gltf()
//{
//}
//
//bool gltf::LoadGLTF(const std::string& gltfPath, const std::string& binPath)
//{
//    try {
//        m_mesh = LoadGLTFMesh(gltfPath, binPath); // From your gltf.h
//		m_Loaded = true;
//        return true;
//    }
//    catch (const std::exception& e) {
//        std::cerr << "GLTF Load Error: " << e.what() << std::endl;
//        m_Loaded = false;
//        return false;
//    }
//}
//
//std::string gltf::ReadGltfTextFile(const std::string& path)
//{
//    std::ifstream file(path);
//    if (!file) throw std::runtime_error("Failed to read file: " + path);
//    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
//}
//
//std::vector<unsigned char> gltf::ReadGltfBinaryFile(const std::string& path)
//{
//    std::ifstream file(path, std::ios::binary | std::ios::ate);
//    if (!file) throw std::runtime_error("Failed to read binary: " + path);
//    std::vector<unsigned char> buffer(file.tellg());
//    file.seekg(0);
//    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
//    return buffer;
//}
//
//GLTFMesh gltf::LoadGLTFMesh(const std::string& gltfPath, const std::string& binPath)
//{
//    json gltf = json::parse(ReadGltfTextFile(gltfPath));
//    std::vector<unsigned char> binData = ReadGltfBinaryFile(binPath);
//
//    GLTFMesh result;
//
//    // ###################                 #################### 
//    // ################### Not Working ####################
//// Safe name extraction
//    if (gltf.contains("name") && gltf["name"].is_string()) {
//        std::string gltfname = gltf["name"].get<std::string>();
//        std::cout << "Mesh Name: " << gltfname << std::endl;
//    }
//    else {
//        std::cerr << "Mesh does not have a name." << std::endl;
//    }
//
//    // process primitives
//    const auto& primitives = gltf["meshes"][0]["primitives"];
//    for (const auto& primitive : primitives) {
//        const auto& posAcc = gltf["accessors"][primitive["attributes"]["POSITION"].get<int>()];
//        float* positions = GetAccessorData<float>(gltf, binData, posAcc);
//
//        // normals
//        const auto& normAcc = primitive["attributes"].contains("NORMAL")
//            ? gltf["accessors"][primitive["attributes"]["NORMAL"].get<int>()]
//            : json{};
//        float* normals = normAcc.is_null() ? nullptr : GetAccessorData<float>(gltf, binData, normAcc);
//
//        // uvs
//        const auto& uvAcc = primitive["attributes"].contains("TEXCOORD_0")
//            ? gltf["accessors"][primitive["attributes"]["TEXCOORD_0"].get<int>()]
//            : json{};
//        float* uvs = uvAcc.is_null() ? nullptr : GetAccessorData<float>(gltf, binData, uvAcc);
//
//        //std::vector<uint32_t> indices;
//
//		// ################################# New Vertex Buffer Construction #################################
//		// Added for Tangents: we need to build a vertex buffer with interleaved attributes (pos, normal, uv, tangent)
//
//        // build simple arrays
//        size_t vertexCount = posAcc["count"];
//        std::vector<glm::vec3> positionsVec(vertexCount);
//        std::vector<glm::vec3> normalsVec(vertexCount);
//        std::vector<glm::vec2> uvsVec(vertexCount);
//        for (size_t i = 0; i < vertexCount; ++i) {
//            positionsVec[i] = glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
//            if (normals) normalsVec[i] = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
//            else normalsVec[i] = glm::vec3(0.0f, 0.0f, 1.0f);
//            if (uvs) uvsVec[i] = glm::vec2(uvs[i * 2 + 0], 1.0f - uvs[i * 2 + 1]);
//            else uvsVec[i] = glm::vec2(0.0f, 0.0f);
//        }
//
//        // Read indices robustly (handle 16-bit and 32-bit index accessors)
//        std::vector<uint32_t> indices;
//        if (primitive.contains("indices")) {
//            const auto& idxAcc = gltf["accessors"][primitive["indices"].get<int>()];
//            int componentType = idxAcc["componentType"].get<int>();
//            size_t idxCount = idxAcc["count"].get<size_t>();
//
//            if (componentType == 5123) { // UNSIGNED_SHORT
//                uint16_t* idxData16 = GetAccessorData<uint16_t>(gltf, binData, idxAcc);
//                indices.resize(idxCount);
//                for (size_t ii = 0; ii < idxCount; ++ii) indices[ii] = static_cast<uint32_t>(idxData16[ii]);
//            }
//            else if (componentType == 5125) { // UNSIGNED_INT
//                uint32_t* idxData32 = GetAccessorData<uint32_t>(gltf, binData, idxAcc);
//                indices.assign(idxData32, idxData32 + idxCount);
//            }
//            else {
//                std::cerr << "Unsupported index componentType = " << componentType << std::endl;
//            }
//        }
//
//        // Compute or read tangents
//        std::vector<glm::vec3> tangentsVec(vertexCount, glm::vec3(0.0f));
//        if (primitive["attributes"].contains("TANGENT")) {
//            const auto& tanAcc = gltf["accessors"][primitive["attributes"]["TANGENT"].get<int>()];
//            // TANGENT in glTF is typically vec4 (x,y,z,w). We only use xyz here.
//            float* tanData = GetAccessorData<float>(gltf, binData, tanAcc);
//            for (size_t i = 0; i < vertexCount; ++i) {
//                size_t base = i * 4;
//                tangentsVec[i] = glm::vec3(tanData[base + 0], tanData[base + 1], tanData[base + 2]);
//            }
//        }
//        else {
//            // compute tangents using UVs/positions/normals and indices
//            ComputeTangents(positionsVec, uvsVec, normalsVec, indices, tangentsVec);
//        }
//
//        // build interleaved vertex buffer including tangents (pos3 + norm3 + uv2 + tan3 = 11 floats)
//        std::vector<float> vertexBuffer;
//        vertexBuffer.reserve(vertexCount * 11);
//        for (size_t i = 0; i < vertexCount; ++i) {
//            vertexBuffer.push_back(positionsVec[i].x);
//            vertexBuffer.push_back(positionsVec[i].y);
//            vertexBuffer.push_back(positionsVec[i].z);
//
//            vertexBuffer.push_back(normalsVec[i].x);
//            vertexBuffer.push_back(normalsVec[i].y);
//            vertexBuffer.push_back(normalsVec[i].z);
//
//            vertexBuffer.push_back(uvsVec[i].x);
//            vertexBuffer.push_back(uvsVec[i].y);
//
//            vertexBuffer.push_back(tangentsVec[i].x);
//            vertexBuffer.push_back(tangentsVec[i].y);
//            vertexBuffer.push_back(tangentsVec[i].z);
//        }
//
//        // ############################################# End New ########################################################
//
//        // Read indices robustly (handle 16-bit and 32-bit index accessors)
//        // std::vector<uint32_t> indices;
//        if (primitive.contains("indices")) {
//            const auto& idxAcc = gltf["accessors"][primitive["indices"].get<int>()];
//            int componentType = idxAcc["componentType"].get<int>();
//
//            size_t idxCount = idxAcc["count"].get<size_t>();
//            // get bufferView+offset and then cast appropriately
//            if (componentType == 5123) { // UNSIGNED_SHORT
//                uint16_t* idxData16 = GetAccessorData<uint16_t>(gltf, binData, idxAcc);
//                indices.resize(idxCount);
//                for (size_t i = 0; i < idxCount; ++i) indices[i] = static_cast<uint32_t>(idxData16[i]);
//            }
//            else if (componentType == 5125) { // UNSIGNED_INT
//                uint32_t* idxData32 = GetAccessorData<uint32_t>(gltf, binData, idxAcc);
//                indices.assign(idxData32, idxData32 + idxCount);
//            }
//            else {
//                std::cerr << "Unsupported index componentType = " << componentType << std::endl;
//            }
//        }
//
//        // Create VAO/VBO/EBO for this primitive (keep them in members for now)
//        glGenVertexArrays(1, &gltfVAO);
//        glGenBuffers(1, &gltfVBO);
//        glGenBuffers(1, &gltfEBO);
//
//        glBindVertexArray(gltfVAO);
//
//        // Upload index buffer using the correct size (uint32_t)
//        if (!indices.empty()) {
//            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gltfEBO);
//            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
//        }
//
//        // Upload vertex buffer
//        glBindBuffer(GL_ARRAY_BUFFER, gltfVBO);
//        glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(float), vertexBuffer.data(), GL_STATIC_DRAW);
//
//        // compute integer stride in floats
//        int strideFloats = (vertexCount > 0) ? static_cast<int>(vertexBuffer.size() / vertexCount) : 0;
//        int strideBytes = strideFloats * static_cast<int>(sizeof(float));
//
//        // basic validation
//        if (strideFloats < 5) {
//            std::cerr << "Warning: unexpected floats-per-vertex: " << strideFloats << std::endl;
//        }
//
//        // position (3 floats)
//        glEnableVertexAttribArray(0);
//        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*)(0));
//
//        // normal (3 floats) at offset 3
//        if (strideFloats >= 6) {
//            glEnableVertexAttribArray(1);
//            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*)(3 * sizeof(float)));
//        }
//        else {
//            glDisableVertexAttribArray(1);
//        }
//
//        // texcoord (2 floats) at offset 6
//        if (strideFloats >= 8) {
//            glEnableVertexAttribArray(2);
//            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, strideBytes, (void*)(6 * sizeof(float)));
//        }
//        else {
//            glDisableVertexAttribArray(2);
//        }
//
//        // tangent (optional) at offset 8 if present (pos3 + norm3 + uv2 + tangent3 = 11 floats)
//        if (strideFloats >= 11) {
//            glEnableVertexAttribArray(3);
//            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*)(8 * sizeof(float)));
//        }
//        else {
//            glDisableVertexAttribArray(3);
//        }
//
//        // Create SubMesh entry
//        SubMesh sub;
//        sub.indexCount = indices.size();
//        sub.textureID = 0;
//
//        std::cout << "Total floats: " << vertexBuffer.size() << std::endl;
//        std::cout << "Total vertices: " << vertexCount << std::endl;
//        std::cout << "Floats per vertex: " << (float)vertexBuffer.size() / vertexCount << std::endl;
//
//        if (primitive.contains("material")) {
//            int materialIndex = primitive["material"];
//            sub.textures = loadTextureForMaterial(gltf, materialIndex, gltfPath);
//        }
//
//        glBindVertexArray(0);
//
//        result.submeshes.push_back(sub);
//    }
//    return result;
//        
//}
//
//GLuint gltf::loadTextureFromImageIndex(const json& gltf, int imageIndex, const std::string& gltfPath)
//{
//    if (imageIndex >= (int)gltf["images"].size()) return 0;
//
//    std::string imagePath = gltf["images"][imageIndex]["uri"];
//    std::string baseDir = gltfPath.substr(0, gltfPath.find_last_of("/\\") + 1);
//    std::string fullImagePath = baseDir + imagePath;
//
//    // Ensure consistent vertical flip behavior across your loaders
//    stbi_set_flip_vertically_on_load(true);
//
//    int width = 0, height = 0, nrComponents = 0;
//    unsigned char* imageData = stbi_load(fullImagePath.c_str(), &width, &height, &nrComponents, 0);
//    if (!imageData) {
//        std::cerr << "Failed to load texture: " << fullImagePath << std::endl;
//        return 0;
//    }
//
//    GLenum format = GL_RGB;
//    GLenum internalFormat = GL_RGB8;
//    if (nrComponents == 1) {
//        format = GL_RED; internalFormat = GL_R8;
//    }
//    else if (nrComponents == 3) {
//        format = GL_RGB; internalFormat = GL_RGB8;
//    }
//    else if (nrComponents == 4) {
//        format = GL_RGBA; internalFormat = GL_RGBA8;
//    }
//    else {
//        // unexpected, default to RGBA
//        format = GL_RGBA; internalFormat = GL_RGBA8;
//    }
//
//    GLuint textureID = 0;
//    glGenTextures(1, &textureID);
//    glBindTexture(GL_TEXTURE_2D, textureID);
//
//    // Use correct internal/format pair
//    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, imageData);
//    glGenerateMipmap(GL_TEXTURE_2D);
//
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//    glBindTexture(GL_TEXTURE_2D, 0);
//    stbi_image_free(imageData);
//
//    return textureID;
//
//}
//
//std::map<std::string, GLuint> gltf::loadTextureForMaterial(const json& gltf, int materialIndex, const std::string& gltfPath)
//{
//    std::map<std::string, GLuint> textures;
//
//    if (materialIndex < 0 || materialIndex >= gltf["materials"].size()) return textures;
//    const auto& material = gltf["materials"][materialIndex];
//
//    // PBR Metallic-Roughness Textures
//    if (material.contains("pbrMetallicRoughness")) {
//        const auto& pbr = material["pbrMetallicRoughness"];
//
//        if (pbr.contains("baseColorTexture")) {
//            int index = pbr["baseColorTexture"]["index"];
//            int imageIndex = gltf["textures"][index]["source"];
//            textures["baseColor"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
//            std::cout << " This model contain baseColor texture" << std::endl;
//        }
//        else {
//            std::cout << " This model dose not contain baseColor texture" << std::endl;
//        }
//
//        if (pbr.contains("metallicRoughnessTexture")) {
//            int index = pbr["metallicRoughnessTexture"]["index"];
//            int imageIndex = gltf["textures"][index]["source"];
//            textures["metallicRoughnessMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
//            std::cout << " This model contain metallicRoughnessMap texture" << std::endl;
//        }
//        else {
//            std::cout << " This model dose not contain metallicRoughnessTexture texture" << std::endl;
//        }
//        // } moved down
//
//        // Normal Map
//        if (material.contains("normalTexture")) {
//            int index = material["normalTexture"]["index"];
//            int imageIndex = gltf["textures"][index]["source"];
//            textures["normalMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
//            std::cout << " This model contain normalMap texture" << std::endl;
//        }
//        else {
//            std::cout << " This model dose not contain narmalMap  texture" << std::endl;
//        }
//
//        // Occlusion Map
//        if (material.contains("occlusionTexture")) {
//            int index = material["occlusionTexture"]["index"];
//            int imageIndex = gltf["textures"][index]["source"];
//            textures["occlusionMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
//            std::cout << " This model contain occlusionMap texture" << std::endl;
//        }
//        else {
//            std::cout << " This model dose not contain occlutionMap texture" << std::endl;
//        }
//
//        // Emissive Map
//        if (material.contains("emissiveTexture")) {
//            int index = material["emissiveTexture"]["index"];
//            int imageIndex = gltf["textures"][index]["source"];
//            textures["emissiveMap"] = loadTextureFromImageIndex(gltf, imageIndex, gltfPath);
//            std::cout << " This model an contain emissiveMap texture" << std::endl;
//        }
//        else {
//            std::cout << " This model dose not contain emissiveMap texture" << std::endl;
//        }
//
//    } // moved
//    return textures;
//}
//
////void gltf::DrawGltf(Shader& shader, Camera& camera)
//void gltf::DrawGltf()
//{
//    // Draw each submesh once. Bind baseColor (if any) to unit 0 so your shader's sampler myTexture works.
//    for (const auto& sub : m_mesh.submeshes) {
//        // bind baseColor to texture unit 0 if present
//        GLuint baseTex = 0;
//        auto it = sub.textures.find("baseColor");
//        if (it != sub.textures.end()) baseTex = it->second;
//
//        if (baseTex != 0) {
//            glActiveTexture(GL_TEXTURE0);
//            glBindTexture(GL_TEXTURE_2D, baseTex);
//        }
//
//        if (gltfVAO != 0 && sub.indexCount > 0) {
//            glBindVertexArray(gltfVAO);
//            // Use GL_UNSIGNED_INT since we uploaded uint32_t indices
//            glDrawElements(GL_TRIANGLES, (GLsizei)sub.indexCount, GL_UNSIGNED_INT, 0);
//            glBindVertexArray(0);
//        }
//
//        if (baseTex != 0) {
//            glBindTexture(GL_TEXTURE_2D, 0);
//        }
//
//        // Ensure active texture unit reset
//        glActiveTexture(GL_TEXTURE0);
//    }
//
//}
//
//void gltf::DestroyGLTFMesh(GLTFMesh& m_mesh)
//{
//    for (auto& subMesh : m_mesh.submeshes) {
//        if (gltfEBO) glDeleteBuffers(1, &gltfEBO);
//        if (gltfVBO) glDeleteBuffers(1, &gltfVBO);
//        if (gltfVAO) glDeleteVertexArrays(1, &gltfVAO);
//        if (subMesh.textureID) glDeleteTextures(1, &subMesh.textureID);
//    }
//    for (auto& subMesh : m_mesh.submeshes) {
//        for (auto& kv : subMesh.textures) {
//            if (kv.second) { glDeleteTextures(1, &kv.second); }
//        }
//    }
//   
//    m_mesh.submeshes.clear();
//}
