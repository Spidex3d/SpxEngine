#include "skyBox.h"
#include <glad\glad.h>
#include "stb/stb_image.h"

LoadSkybox::LoadSkybox(int idx_, const std::string& name_, int m_skyIdx_)
    : idx(idx_), name(name_), m_skyIdx(m_skyIdx_)
{
    entId = idx;
    entName = this->name;
    entObjectIndex = m_skyIdx;
    entTypeID = SKY_OBJ;

    position = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
    rotation = glm::vec3(0.0f);
    modelMatrix = glm::mat4(1.0f);
}

LoadSkybox::~LoadSkybox()
{
	DestroySkyMesh(m_skymesh);

    if (frontFaceTexID) {
        glDeleteTextures(1, &frontFaceTexID);
        frontFaceTexID = 0;
    }
    if (sky_textureID) {
        glDeleteTextures(1, &sky_textureID);
        sky_textureID = 0;
    }
}

void LoadSkybox::SkyBox()
{
    SkySubMesh sub;
    float SkyBoxVertices[] =
    {
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f
    };

    unsigned int SkyBoxIndices[] =
    {
        // Right
        1, 2, 6,
        6, 5, 1,
        // Left
        0, 4, 7,
        7, 3, 0,
        // Top
        4, 5, 6,
        6, 7, 4,
        // Bottom
        0, 3, 2,
        2, 1, 0,
        // Back
        0, 1, 5,
        5, 4, 0,
        // Front
        3, 7, 6,
        6, 2, 3
    };

    glGenVertexArrays(1, &sub.vao);
    glGenBuffers(1, &sub.vbo);
    glGenBuffers(1, &sub.ebo);
    glBindVertexArray(sub.vao);
    glBindBuffer(GL_ARRAY_BUFFER, sub.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(SkyBoxVertices), SkyBoxVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(SkyBoxIndices), SkyBoxIndices, GL_STATIC_DRAW);

    // Note: last two bool params are GL_FALSE and the stride is 3*sizeof(float)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // unbind VAO (EBO stays bound to VAO, do not unbind EBO until VAO unbound)
    glBindVertexArray(0);

    // store indexCount for later draw
    // we created 36 indices
    m_skymesh.submeshes.push_back(sub);

    // Prepare a cubemap texture object (faces will be provided by LoadFromFolder)
    glGenTextures(1, &sky_textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, sky_textureID);
    // basic sampling params
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // seam handling
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}


void LoadSkybox::DrawSkyBox(Shader* shader, const glm::mat4& view, const glm::mat4& projection)
{
    if (!m_SkyLoaded) return;
    if (!shader) return;

    // Prepare state: draw skybox with depth writes disabled and depth func LEQUAL
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    shader->Use();
    // Important: remove translation from view matrix for skybox
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
    shader->setMat4("view", viewNoTrans);
    shader->setMat4("projection", projection);

    // set sampler to texture unit 0 (ensure shader uses same sampler name)
    shader->SetUniformInt("skybox", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, sky_textureID);

    // draw all submeshes
    for (const auto& sub : m_skymesh.submeshes) {
        if (sub.vao == 0) continue;
        glBindVertexArray(sub.vao);
        // we used 36 indices above
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // restore state
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

inline unsigned char* extract_face(const unsigned char* src, int srcWidth, int srcHeight, int channels,
    int faceRow, int faceCol, int faceSize = 512) {
    unsigned char* face = new unsigned char[faceSize * faceSize * channels];
    for (int y = 0; y < faceSize; ++y) {
        for (int x = 0; x < faceSize; ++x) {
            int srcX = faceCol * faceSize + x;
            int srcY = faceRow * faceSize + y;
            for (int c = 0; c < channels; ++c) {
                face[(y * faceSize + x) * channels + c] =
                    src[(srcY * srcWidth + srcX) * channels + c];
            }
        }
    }
    return face;
}

// Replacement for LoadSkybox::loadSkyTextureFromFolder to accept either a directory OR a full file path.
// It avoids calling directory_iterator on a file path (which throws).
std::vector<SkyTexture> LoadSkybox::loadSkyTextureFromFolder(const std::string& folderPath)
{
    std::vector<SkyTexture> sky_textures;
    if (folderPath.empty()) return sky_textures;

    std::filesystem::path p(folderPath);

    auto is_image_ext = [](const std::string& ext) {
        if (ext.empty()) return false;
        std::string e = ext;
        for (auto& c : e) c = (char)std::tolower(c);
        return (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp");
    };

    // Helper: process a single image file (sheet) and append its cubemap+preview to sky_textures.
    auto processImage = [&](const std::string& imagePath) {
        int width = 0, height = 0, channels = 0;
        unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
        if (!data) {
            std::cerr << "Failed to load image: " << imagePath << "\n";
            return;
        }

        // Try to infer faceSize from image layout (3 rows x 4 cols typical), fallback to 512
        int faceSizeW = (width / 4);
        int faceSizeH = (height / 3);
        int faceSize = std::min(faceSizeW, faceSizeH);
        if (faceSize <= 0) faceSize = 512;

        std::vector<std::pair<int, int>> facePositions = {
            {0, 1}, // Top
            {1, 0}, // Left
            {1, 1}, // Front
            {1, 2}, // Right
            {1, 3}, // Back
            {2, 1}  // Bottom
        };

        GLenum faceTargets[6] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
        };

        GLuint cubeMapID = 0;
        glGenTextures(1, &cubeMapID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapID);

        GLuint previewTexID = 0;

        for (int i = 0; i < 6; ++i) {
            auto [row, col] = facePositions[i];
            unsigned char* face = extract_face(data, width, height, channels, row, col, faceSize);

            // Upload to cubemap
            glTexImage2D(faceTargets[i], 0,
                channels == 4 ? GL_RGBA : GL_RGB,
                faceSize, faceSize, 0,
                channels == 4 ? GL_RGBA : GL_RGB,
                GL_UNSIGNED_BYTE, face);

            // Also create preview texture from RIGHT face (index 3) as before
            if (i == 3) {
                glGenTextures(1, &previewTexID);
                glBindTexture(GL_TEXTURE_2D, previewTexID);
                glTexImage2D(GL_TEXTURE_2D, 0,
                    channels == 4 ? GL_RGBA : GL_RGB,
                    faceSize, faceSize, 0,
                    channels == 4 ? GL_RGBA : GL_RGB,
                    GL_UNSIGNED_BYTE, face);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            delete[] face;
        }

        // Cubemap sampling/wrap params
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        stbi_image_free(data);

        sky_textures.push_back({ cubeMapID, imagePath, previewTexID });
    };

    // CASE 1: If the provided path is a regular file, just process that file
    if (std::filesystem::exists(p) && std::filesystem::is_regular_file(p)) {
        if (is_image_ext(p.extension().string())) {
            processImage(p.string());
        }
        else {
            std::cerr << "loadSkyTextureFromFolder: provided file is not an image: " << folderPath << "\n";
        }
        return sky_textures;
    }

    // CASE 2: If it's not a file, but a directory, iterate the directory (existing behavior)
    if (!std::filesystem::exists(p) || !std::filesystem::is_directory(p)) {
        std::cerr << "loadSkyTextureFromFolder: path does not exist or is not a directory: " << folderPath << "\n";
        return sky_textures;
    }

    for (const auto& entry : std::filesystem::directory_iterator(p)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (!is_image_ext(ext)) continue;

        processImage(entry.path().string());
    }

    return sky_textures; // number of sky textures found (e.g., 25)
}


bool LoadSkybox::LoadFromFolder(const std::string& folderPath)
//bool LoadSkybox::LoadFromFolder(const std::string& folderPath, const std::string& skyFile)
{
    // use the helper you already wrote to create cube maps from files in the folder
    auto sky_textures = loadSkyTextureFromFolder(folderPath);
    if (sky_textures.empty()) {
        std::cerr << "LoadSkybox::LoadFromFolder: no sky textures found in " << folderPath << std::endl;
        return false;
    }

    // For simplicity pick the first cubemap discovered
    sky_textureID = sky_textures.front().id;
    frontFaceTexID = sky_textures.front().frontFaceTexID;
    m_SkyLoaded = true;
    return true;
}



void LoadSkybox::DestroySkyMesh(SkyMesh& m_skymesh)
{
    for (auto& sub : m_skymesh.submeshes) {
        if (sub.ebo) { glDeleteBuffers(1, &sub.ebo); sub.ebo = 0; }
        if (sub.vbo) { glDeleteBuffers(1, &sub.vbo); sub.vbo = 0; }
        if (sub.vao) { glDeleteVertexArrays(1, &sub.vao); sub.vao = 0; }

    }
	m_skymesh.submeshes.clear();
}


