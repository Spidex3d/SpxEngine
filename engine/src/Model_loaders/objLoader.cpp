#include "objLoader.h"
#include <cstring> // memcpy

// ctor/dtor
objLoader::objLoader(int idx_, const std::string& name_, int modelObjIdx_)
    : VAO(0), VBO(0), EBO(0), idx(idx_), name(name_), modelObjIdx(modelObjIdx_), m_Loaded(false)
{
    entId = idx;
    entName = this->name;
    entObjectIndex = modelObjIdx;
    entTypeID = OBJ_OBJ_MODEL; // ensure defined in globalVar.h

    position = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
    rotation = glm::vec3(0.0f);
    modelMatrix = glm::mat4(1.0f);
}

objLoader::~objLoader()
{
    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    if (VBO) { glDeleteBuffers(1, &VBO); VBO = 0; }
    if (EBO) { glDeleteBuffers(1, &EBO); EBO = 0; }

    for (auto& kv : materials) {
        Material& m = kv.second;
        if (m.diffuseTexID) { glDeleteTextures(1, &m.diffuseTexID);  m.diffuseTexID = 0; }
        if (m.specularTexID) { glDeleteTextures(1, &m.specularTexID); m.specularTexID = 0; }
        if (m.normalTexID) { glDeleteTextures(1, &m.normalTexID);   m.normalTexID = 0; }
        if (m.alphaTexID) { glDeleteTextures(1, &m.alphaTexID);    m.alphaTexID = 0; }
    }
}

// Helper: split utility (kept minimal)
static std::vector<std::string> split(const std::string& s, const std::string& delimiter) {
    size_t start = 0;
    size_t end = s.find(delimiter);
    std::vector<std::string> tokens;
    while (end != std::string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + delimiter.length();
        end = s.find(delimiter, start);
    }
    tokens.push_back(s.substr(start, end));
    return tokens;
}

// Load OBJ: build unique vertex list + indices grouped by material (usemtl)
bool objLoader::Loadobj(const std::string& filename)
{
    m_UniqueVertices.clear();
    m_Indices.clear();
    submeshes.clear();
    materials.clear();
    modelPathObj.clear();
    m_Loaded = false;

    if (filename.find(".obj") == std::string::npos) {
        LOG_WARNING("objLoader::Loadobj: not an .obj file: " << filename);
        return false;
    }

    std::ifstream fin(filename);
    if (!fin.is_open()) {
        LOG_ERROR("objLoader::Loadobj: cannot open " << filename);
        return false;
    }

    // compute model directory for resolving MTL / texture paths
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) modelPathObj = filename.substr(0, pos + 1);
    else modelPathObj = "";

    LOG_INFO("objLoader: Loading OBJ file " << filename << " (dir='" << modelPathObj << "')");

    std::vector<glm::vec3> tempVertices;
    std::vector<glm::vec2> tempUVs;
    std::vector<glm::vec3> tempNormals;

    // Map to deduplicate vertices
    std::unordered_map<Vertex, uint32_t, VertexHash> vertexToIndex;

    // Temporary per-material index lists
    std::unordered_map<std::string, std::vector<uint32_t>> materialIndices;
    std::string currentMaterial = ""; // default material name (empty = no material)
    std::string materialFile;

    std::string line;
    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "v") {
            glm::vec3 v; ss >> v.x >> v.y >> v.z;
            tempVertices.push_back(v);
        }
        else if (cmd == "vt") {
            glm::vec2 uv; ss >> uv.x >> uv.y;
            tempUVs.push_back(uv);
        }
        else if (cmd == "vn") {
            glm::vec3 n; ss >> n.x >> n.y >> n.z;
            tempNormals.push_back(glm::normalize(n));
        }
        else if (cmd == "f") {
            std::vector<std::string> tokens;
            std::string token;
            while (ss >> token) tokens.push_back(token);
            if (tokens.size() < 3) continue;

            // Triangulate polygon with triangle fan
            std::vector<int> vIdxs, vtIdxs, vnIdxs;
            for (auto const& faceData : tokens) {
                int v = 0, vt = 0, vn = 0;
                // supports v/vt/vn, v//vn, v/vt, v
                if (sscanf_s(faceData.c_str(), "%d/%d/%d", &v, &vt, &vn) == 3) {}
                else if (sscanf_s(faceData.c_str(), "%d//%d", &v, &vn) == 2) {}
                else if (sscanf_s(faceData.c_str(), "%d/%d", &v, &vt) == 2) {}
                else if (sscanf_s(faceData.c_str(), "%d", &v) == 1) {}

                if (v < 0) v += (int)tempVertices.size() + 1;
                if (vt < 0) vt += (int)tempUVs.size() + 1;
                if (vn < 0) vn += (int)tempNormals.size() + 1;

                vIdxs.push_back(v);
                vtIdxs.push_back(vt);
                vnIdxs.push_back(vn);
            }

            for (size_t i = 1; i + 1 < vIdxs.size(); ++i) {
                int idxs[3] = { vIdxs[0], vIdxs[i], vIdxs[i + 1] };
                int uidxs[3] = { vtIdxs[0], vtIdxs[i], vtIdxs[i + 1] };
                int nidxs[3] = { vnIdxs[0], vnIdxs[i], vnIdxs[i + 1] };

                for (int k = 0; k < 3; ++k) {
                    Vertex vert{};
                    if (idxs[k] > 0 && idxs[k] <= (int)tempVertices.size()) vert.position = tempVertices[idxs[k] - 1];
                    else vert.position = glm::vec3(0.0f);
                    if (nidxs[k] > 0 && nidxs[k] <= (int)tempNormals.size()) vert.normal = tempNormals[nidxs[k] - 1];
                    else vert.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                    if (uidxs[k] > 0 && uidxs[k] <= (int)tempUVs.size()) vert.texCoords = tempUVs[uidxs[k] - 1];
                    else vert.texCoords = glm::vec2(0.0f, 0.0f);

                    auto it = vertexToIndex.find(vert);
                    uint32_t index;
                    if (it == vertexToIndex.end()) {
                        index = static_cast<uint32_t>(m_UniqueVertices.size());
                        m_UniqueVertices.push_back(vert);
                        vertexToIndex.emplace(vert, index);
                    }
                    else {
                        index = it->second;
                    }

                    // append to current material indices
                    materialIndices[currentMaterial].push_back(index);
                }
            }
        }
        else if (cmd == "mtllib") {
            ss >> materialFile;
        }
        else if (cmd == "usemtl") {
            ss >> currentMaterial;
            // ensure entry exists
            if (materialIndices.find(currentMaterial) == materialIndices.end()) {
                materialIndices[currentMaterial] = std::vector<uint32_t>();
            }
        }
        // ignore other directives for now
    }

    fin.close();

    // Load MTL if present
    if (!materialFile.empty()) {
        LoadMTL(materialFile);
    }

    // Flatten materialIndices into m_Indices and build submeshes vector
    m_Indices.clear();
    submeshes.clear();
    for (auto& kv : materialIndices) {
        SubMesh sm;
        sm.materialName = kv.first; // material name (may be empty string)
        sm.indexOffset = m_Indices.size();
        sm.indexCount = kv.second.size();
        // append all indices for this material
        m_Indices.insert(m_Indices.end(), kv.second.begin(), kv.second.end());
        submeshes.push_back(sm);
    }

    // If there were no 'usemtl' lines, materialIndices will have a single entry keyed by "" (empty)
    // In that case submeshes[0] covers all indices.

    m_Loaded = true;
    LOG_INFO("objLoader: loaded vertices=" << m_UniqueVertices.size() << " indices=" << m_Indices.size() << " submeshes=" << submeshes.size());
    return true;
}

// MTL loading (unchanged semantics, uses modelPathObj prefix)
bool objLoader::LoadMTL(const std::string& filename)
{
    std::string fullpath = modelPathObj + filename;
    std::ifstream fin(fullpath);
    if (!fin.is_open()) {
        LOG_WARNING("objLoader::LoadMTL: Cannot open MTL file: " << fullpath);
        return false;
    }

    LOG_INFO("objLoader: Loading MTL file: " << fullpath);

    std::string lineBuffer;
    Material* currentMaterial = nullptr;

    while (std::getline(fin, lineBuffer)) {
        std::stringstream ss(lineBuffer);
        std::string cmd;
        ss >> cmd;
        if (cmd == "newmtl") {
            std::string materialName; ss >> materialName;
            materials[materialName] = Material();
            currentMaterial = &materials[materialName];
            currentMaterial->name = materialName;
        }
        else if (cmd == "Ka" && currentMaterial) { ss >> currentMaterial->ambient.r >> currentMaterial->ambient.g >> currentMaterial->ambient.b; }
        else if (cmd == "Kd" && currentMaterial) { ss >> currentMaterial->diffuse.r >> currentMaterial->diffuse.g >> currentMaterial->diffuse.b; }
        else if (cmd == "Ks" && currentMaterial) { ss >> currentMaterial->specular.r >> currentMaterial->specular.g >> currentMaterial->specular.b; }
        else if (cmd == "Ns" && currentMaterial) { ss >> currentMaterial->shininess; }
        else if ((cmd == "d" || cmd == "Tr") && currentMaterial) { ss >> currentMaterial->transparency; }
        else if (cmd == "Ni" && currentMaterial) { ss >> currentMaterial->opticalDensity; }
        else if (cmd == "illum" && currentMaterial) { ss >> currentMaterial->illumModel; }
        else if (cmd == "map_Kd" && currentMaterial) {
            ss >> currentMaterial->diffuseMap;
            currentMaterial->diffuseTexID = LoadObjTexture(modelPathObj + currentMaterial->diffuseMap);
        }
        else if (cmd == "map_Ks" && currentMaterial) {
            ss >> currentMaterial->specularMap;
            currentMaterial->specularTexID = LoadObjTexture(modelPathObj + currentMaterial->specularMap);
        }
        else if ((cmd == "map_bump" || cmd == "bump") && currentMaterial) {
            ss >> currentMaterial->normalMap;
            currentMaterial->normalTexID = LoadObjTexture(modelPathObj + currentMaterial->normalMap);
        }
        else if (cmd == "map_d" && currentMaterial) {
            ss >> currentMaterial->alphaMap;
            currentMaterial->alphaTexID = LoadObjTexture(modelPathObj + currentMaterial->alphaMap);
        }
    }

    fin.close();
    return true;
}

GLuint objLoader::LoadObjTexture(const std::string& filename)
{
    if (filename.empty()) return 0;

    int width = 0, height = 0, nrComponents = 0;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (!data) {
        LOG_WARNING("objLoader::LoadObjTexture: failed to load " << filename);
        return 0;
    }

    GLenum format = GL_RGB;
    if (nrComponents == 1) format = GL_RED;
    else if (nrComponents == 3) format = GL_RGB;
    else if (nrComponents == 4) format = GL_RGBA;

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, (format == GL_RGBA ? GL_RGBA8 : GL_RGB8), width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

// Upload indexed buffers to GPU (VAO/VBO/EBO)
void objLoader::objModels()
{
    if (!m_Loaded || m_UniqueVertices.empty() || m_Indices.empty()) return;

    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);
    if (EBO == 0) glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, m_UniqueVertices.size() * sizeof(Vertex), m_UniqueVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(uint32_t), m_Indices.data(), GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    // texCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

// Draw submeshes with their material textures
void objLoader::objDrawModels()
{
    if (!m_Loaded || VAO == 0 || m_Indices.empty()) return;

    glBindVertexArray(VAO);

    // If there are submeshes, draw each with its material (texture).
    if (!submeshes.empty()) {
        for (const auto& sm : submeshes) {
            GLuint tex = 0;
            auto it = materials.find(sm.materialName);
            if (it != materials.end()) tex = it->second.diffuseTexID;

            if (tex != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
            }

            // offset in bytes for EBO
            const void* offsetPtr = (const void*)(sm.indexOffset * sizeof(uint32_t));
            glDrawElements(GL_TRIANGLES, (GLsizei)sm.indexCount, GL_UNSIGNED_INT, offsetPtr);

            if (tex != 0) glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    else {
        // fallback: draw everything in one call
        GLuint tex = 0;
        if (!materials.empty()) {
            // pick first available texture
            for (const auto& kv : materials) {
                if (kv.second.diffuseTexID) { tex = kv.second.diffuseTexID; break; }
            }
        }
        if (tex != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
        }
        glDrawElements(GL_TRIANGLES, (GLsizei)m_Indices.size(), GL_UNSIGNED_INT, (const void*)0);
        if (tex != 0) glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(0);
}
bool objLoader::HasDiffuseTexture() const
{
    for (const auto& kv : materials) {
        if (kv.second.diffuseTexID != 0) return true;
    }
    return false;
}

GLuint objLoader::GetFirstDiffuseTexture() const
{
    for (const auto& kv : materials) {
        if (kv.second.diffuseTexID != 0) return kv.second.diffuseTexID;
    }
    return 0u;
}





//#include "objLoader.h"
//
//// initialize stuff if we need to
//void objLoader::Initialize()
//{
//    // reserved for future initialization
//}
//
//objLoader::objLoader(int idx, const std::string& name, int modelObjIdx)
//    : idx(idx), name(name), modelObjIdx(modelObjIdx), VAO(0), VBO(0), EBO(0), m_Loaded(false)
//{
//    entId = idx;
//    entName = this->name;
//    entObjectIndex = modelObjIdx;
//    entTypeID = OBJ_OBJ_MODEL; // ensure OBJ_OBJ_MODEL is defined in globalVar.h
//
//    // default transform
//    position = glm::vec3(0.0f);
//    scale = glm::vec3(1.0f);
//    rotation = glm::vec3(0.0f);
//    modelMatrix = glm::mat4(1.0f);
//}
//
//objLoader::~objLoader()
//{
//    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
//    if (VBO) { glDeleteBuffers(1, &VBO); VBO = 0; }
//    if (EBO) { glDeleteBuffers(1, &EBO); EBO = 0; }
//
//    // Free material textures
//    for (auto& kv : materials) {
//        Material& m = kv.second;
//        if (m.diffuseTexID) { glDeleteTextures(1, &m.diffuseTexID);  m.diffuseTexID = 0; }
//        if (m.specularTexID) { glDeleteTextures(1, &m.specularTexID); m.specularTexID = 0; }
//        if (m.normalTexID) { glDeleteTextures(1, &m.normalTexID);   m.normalTexID = 0; }
//        if (m.alphaTexID) { glDeleteTextures(1, &m.alphaTexID);    m.alphaTexID = 0; }
//    }
//}
//
//std::vector<std::string> objLoader::split(const std::string& s, const std::string& delimiter)
//{
//    size_t start = 0;
//    size_t end = s.find(delimiter);
//    std::vector<std::string> tokens;
//    while (end != std::string::npos) {
//        tokens.push_back(s.substr(start, end - start));
//        start = end + delimiter.length();
//        end = s.find(delimiter, start);
//    }
//    tokens.push_back(s.substr(start, end));
//    return tokens;
//}
//
//// Load obj file (simple parser, triangulates polygons using fan)
//bool objLoader::Loadobj(const std::string& filename)
//{
//    m_Vertices.clear();
//    materials.clear();
//    m_Loaded = false;
//    modelPathObj.clear();
//
//    if (filename.find(".obj") == std::string::npos) {
//        LOG_WARNING("objLoader::Loadobj: not an .obj file: " << filename);
//        return false;
//    }
//
//    std::ifstream fin(filename);
//    if (!fin.is_open()) {
//        LOG_ERROR("objLoader::Loadobj: cannot open " << filename);
//        return false;
//    }
//
//    // remember model directory for resolving MTL and texture paths
//    size_t pos = filename.find_last_of("/\\");
//    if (pos != std::string::npos) modelPathObj = filename.substr(0, pos + 1);
//    else modelPathObj = ""; // current directory
//
//    LOG_INFO("objLoader: Loading OBJ file " << filename << " (dir='" << modelPathObj << "')");
//
//    std::string line;
//    std::string materialFile;
//    std::vector<glm::vec3> tempVertices;
//    std::vector<glm::vec2> tempUVs;
//    std::vector<glm::vec3> tempNormals;
//
//    while (std::getline(fin, line)) {
//        std::stringstream ss(line);
//        std::string cmd;
//        ss >> cmd;
//        if (cmd == "v") {
//            glm::vec3 v; ss >> v.x >> v.y >> v.z;
//            tempVertices.push_back(v);
//        }
//        else if (cmd == "vt") {
//            glm::vec2 uv; ss >> uv.x >> uv.y;
//            tempUVs.push_back(uv);
//        }
//        else if (cmd == "vn") {
//            glm::vec3 n; ss >> n.x >> n.y >> n.z;
//            tempNormals.push_back(glm::normalize(n));
//        }
//        else if (cmd == "f") {
//            std::vector<std::string> tokens;
//            std::string token;
//            while (ss >> token) tokens.push_back(token);
//            if (tokens.size() < 3) continue;
//
//            std::vector<int> vIdxs, vtIdxs, vnIdxs;
//            for (auto const& faceData : tokens) {
//                int v = 0, vt = 0, vn = 0;
//                // support v/vt/vn, v//vn, v/vt, v
//                if (sscanf_s(faceData.c_str(), "%d/%d/%d", &v, &vt, &vn) == 3) {}
//                else if (sscanf_s(faceData.c_str(), "%d//%d", &v, &vn) == 2) {}
//                else if (sscanf_s(faceData.c_str(), "%d/%d", &v, &vt) == 2) {}
//                else if (sscanf_s(faceData.c_str(), "%d", &v) == 1) {}
//
//                if (v < 0) v += (int)tempVertices.size() + 1;
//                if (vt < 0) vt += (int)tempUVs.size() + 1;
//                if (vn < 0) vn += (int)tempNormals.size() + 1;
//
//                vIdxs.push_back(v);
//                vtIdxs.push_back(vt);
//                vnIdxs.push_back(vn);
//            }
//
//            // triangulate polygon as triangle fan
//            for (size_t i = 1; i + 1 < vIdxs.size(); ++i) {
//                int idxs[3] = { vIdxs[0], vIdxs[i], vIdxs[i + 1] };
//                int uidxs[3] = { vtIdxs[0], vtIdxs[i], vtIdxs[i + 1] };
//                int nidxs[3] = { vnIdxs[0], vnIdxs[i], vnIdxs[i + 1] };
//
//                for (int k = 0; k < 3; ++k) {
//                    Vertex vert{};
//                    if (idxs[k] > 0 && idxs[k] <= (int)tempVertices.size()) vert.position = tempVertices[idxs[k] - 1];
//                    if (nidxs[k] > 0 && nidxs[k] <= (int)tempNormals.size()) vert.normal = tempNormals[nidxs[k] - 1];
//                    if (uidxs[k] > 0 && uidxs[k] <= (int)tempUVs.size()) vert.texCoords = tempUVs[uidxs[k] - 1];
//                    m_Vertices.push_back(vert);
//                }
//            }
//        }
//        else if (cmd == "mtllib") {
//            ss >> materialFile;
//        }
//        // ignore other directives for now (useful: use, o, g, s, etc.)
//    }
//
//    fin.close();
//
//    if (!materialFile.empty()) {
//        LoadMTL(materialFile); // LoadMTL will use modelPathObj + materialFile
//    }
//
//    m_Loaded = true;
//    LOG_INFO("objLoader: Loaded " << m_Vertices.size() << " vertices");
//    return true;
//}
//
//bool objLoader::LoadMTL(const std::string& filename)
//{
//    std::string fullpath = modelPathObj + filename;
//    std::ifstream fin(fullpath);
//    if (!fin.is_open()) {
//        LOG_WARNING("objLoader::LoadMTL: Cannot open MTL file: " << fullpath);
//        return false;
//    }
//
//    LOG_INFO("objLoader: Loading MTL file: " << fullpath);
//
//    std::string lineBuffer;
//    Material* currentMaterial = nullptr;
//
//    while (std::getline(fin, lineBuffer)) {
//        std::stringstream ss(lineBuffer);
//        std::string cmd;
//        ss >> cmd;
//        if (cmd == "newmtl") {
//            std::string materialName; ss >> materialName;
//            materials[materialName] = Material();
//            currentMaterial = &materials[materialName];
//            currentMaterial->name = materialName;
//        }
//        else if (cmd == "Ka" && currentMaterial) { ss >> currentMaterial->ambient.r >> currentMaterial->ambient.g >> currentMaterial->ambient.b; }
//        else if (cmd == "Kd" && currentMaterial) { ss >> currentMaterial->diffuse.r >> currentMaterial->diffuse.g >> currentMaterial->diffuse.b; }
//        else if (cmd == "Ks" && currentMaterial) { ss >> currentMaterial->specular.r >> currentMaterial->specular.g >> currentMaterial->specular.b; }
//        else if (cmd == "Ns" && currentMaterial) { ss >> currentMaterial->shininess; }
//        else if ((cmd == "d" || cmd == "Tr") && currentMaterial) { ss >> currentMaterial->transparency; }
//        else if (cmd == "Ni" && currentMaterial) { ss >> currentMaterial->opticalDensity; }
//        else if (cmd == "illum" && currentMaterial) { ss >> currentMaterial->illumModel; }
//        else if (cmd == "map_Kd" && currentMaterial) {
//            ss >> currentMaterial->diffuseMap;
//            currentMaterial->diffuseTexID = LoadObjTexture(modelPathObj + currentMaterial->diffuseMap);
//        }
//        else if (cmd == "map_Ks" && currentMaterial) {
//            ss >> currentMaterial->specularMap;
//            currentMaterial->specularTexID = LoadObjTexture(modelPathObj + currentMaterial->specularMap);
//        }
//        else if ((cmd == "map_bump" || cmd == "bump") && currentMaterial) {
//            ss >> currentMaterial->normalMap;
//            currentMaterial->normalTexID = LoadObjTexture(modelPathObj + currentMaterial->normalMap);
//        }
//        else if (cmd == "map_d" && currentMaterial) {
//            ss >> currentMaterial->alphaMap;
//            currentMaterial->alphaTexID = LoadObjTexture(modelPathObj + currentMaterial->alphaMap);
//        }
//    }
//
//    fin.close();
//
//    // Optionally load any textures referenced by materials (already done above)
//    return true;
//}
//
//GLuint objLoader::LoadObjTexture(const std::string& filename)
//{
//    if (filename.empty()) return 0;
//
//    int width = 0, height = 0, nrComponents = 0;
//    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
//    if (!data) {
//        LOG_WARNING("objLoader::LoadObjTexture: failed to load " << filename);
//        return 0;
//    }
//
//    GLenum format = GL_RGB;
//    if (nrComponents == 1) format = GL_RED;
//    else if (nrComponents == 3) format = GL_RGB;
//    else if (nrComponents == 4) format = GL_RGBA;
//
//    GLuint textureID = 0;
//    glGenTextures(1, &textureID);
//    glBindTexture(GL_TEXTURE_2D, textureID);
//    glTexImage2D(GL_TEXTURE_2D, 0, (format == GL_RGBA ? GL_RGBA8 : GL_RGB8), width, height, 0, format, GL_UNSIGNED_BYTE, data);
//    glGenerateMipmap(GL_TEXTURE_2D);
//
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//    stbi_image_free(data);
//    glBindTexture(GL_TEXTURE_2D, 0);
//    return textureID;
//}
//
//// upload vertex data to GPU (non-indexed)
//void objLoader::objModels()
//{
//    if (m_Vertices.empty()) return;
//
//    if (VAO == 0) glGenVertexArrays(1, &VAO);
//    if (VBO == 0) glGenBuffers(1, &VBO);
//
//    glBindVertexArray(VAO);
//    glBindBuffer(GL_ARRAY_BUFFER, VBO);
//    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), m_Vertices.data(), GL_STATIC_DRAW);
//
//    // position
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
//    glEnableVertexAttribArray(0);
//    // normal
//    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
//    glEnableVertexAttribArray(1);
//    // texCoord
//    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
//    glEnableVertexAttribArray(2);
//
//    glBindVertexArray(0);
//    glBindBuffer(GL_ARRAY_BUFFER, 0);
//}
//
//// draw the model (simple: bind first diffuse texture if present and draw all vertices)
//void objLoader::objDrawModels()
//{
//    if (!m_Loaded || VAO == 0) return;
//
//    glBindVertexArray(VAO);
//
//    // If materials exist, try binding the first diffuse texture found (simple heuristic).
//    // Later we will support per-material draw ranges.
//    GLuint tex = 0;
//    for (const auto& kv : materials) {
//        const Material& mat = kv.second;
//        if (mat.diffuseTexID != 0) { tex = mat.diffuseTexID; break; }
//    }
//    if (tex != 0) {
//        glActiveTexture(GL_TEXTURE0);
//        glBindTexture(GL_TEXTURE_2D, tex);
//    }
//
//    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_Vertices.size());
//
//    glBindVertexArray(0);
//    if (tex != 0) {
//        glBindTexture(GL_TEXTURE_2D, 0);
//    }
//}
//
//void objLoader::LoadMaterialTextures(Material& mat, const std::string& filename)
//{
//    if (!mat.diffuseMap.empty())  mat.diffuseTexID = LoadObjTexture(filename + mat.diffuseMap);
//    if (!mat.specularMap.empty()) mat.specularTexID = LoadObjTexture(filename + mat.specularMap);
//    if (!mat.normalMap.empty())   mat.normalTexID = LoadObjTexture(filename + mat.normalMap);
//    if (!mat.alphaMap.empty())    mat.alphaTexID = LoadObjTexture(filename + mat.alphaMap);
//}
//
