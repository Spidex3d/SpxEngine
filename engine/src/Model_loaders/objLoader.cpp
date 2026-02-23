#include "objLoader.h"
#include <cstring> // memcpy

// Add this helper near the top of objLoader.cpp (after includes)

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
    //############################### New Tangent ##################################
    // Build simple arrays for ComputeTangents
    std::vector<glm::vec3> positions; positions.reserve(m_UniqueVertices.size());
    std::vector<glm::vec2> uvs;       uvs.reserve(m_UniqueVertices.size());
    std::vector<glm::vec3> normals;   normals.reserve(m_UniqueVertices.size());
    for (const auto& v : m_UniqueVertices) {
        positions.push_back(v.position);
        uvs.push_back(v.texCoords);
        normals.push_back(v.normal);
    }
    std::vector<glm::vec3> tangents;
    ComputeTangents(positions, uvs, normals, m_Indices, tangents);
    // copy back into vertices
    for (size_t i = 0; i < m_UniqueVertices.size() && i < tangents.size(); ++i) {
        m_UniqueVertices[i].tangent = tangents[i];
    }
    // ##########################################################################
    // 
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

    // NEW: tangent vec3 at location 3
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    glEnableVertexAttribArray(3);

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

