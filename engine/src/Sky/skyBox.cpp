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
	DestroyGLTFMesh(m_skymesh);
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

std::vector<SkyTexture> LoadSkybox::loadSkyTextureFromFolder(const std::string& folderPath)
{
    std::vector<SkyTexture> sky_textures;

    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext != ".png" && ext != ".jpg" && ext != ".bmp") continue;

        std::string imagePath = entry.path().string();
        int width, height, channels;
        unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
        if (!data) {
            std::cerr << "Failed to load image: " << imagePath << "\n";
            continue;
        }

        const int faceSize = 512;
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

        GLuint cubeMapID;
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

            // Also create preview texture from FRONT face
            if (i == 3) { // Front face (index 2) right face (index 3)
                glGenTextures(1, &previewTexID);
                glBindTexture(GL_TEXTURE_2D, previewTexID);
                glTexImage2D(GL_TEXTURE_2D, 0,
                    channels == 4 ? GL_RGBA : GL_RGB,
                    faceSize, faceSize, 0,
                    channels == 4 ? GL_RGBA : GL_RGB,
                    GL_UNSIGNED_BYTE, face);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }

            delete[] face;
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        stbi_image_free(data);

        sky_textures.push_back({ cubeMapID, imagePath, previewTexID });
    }

	return sky_textures; // = 25 the number of sky textures found in the folder
}


bool LoadSkybox::LoadFromFolder(const std::string& folderPath)
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



void LoadSkybox::DestroyGLTFMesh(SkyMesh& m_skymesh)
{
    for (auto& sub : m_skymesh.submeshes) {
        if (sub.ebo) { glDeleteBuffers(1, &sub.ebo); sub.ebo = 0; }
        if (sub.vbo) { glDeleteBuffers(1, &sub.vbo); sub.vbo = 0; }
        if (sub.vao) { glDeleteVertexArrays(1, &sub.vao); sub.vao = 0; }

    }
	m_skymesh.submeshes.clear();
}
