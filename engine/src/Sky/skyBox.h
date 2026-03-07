#pragma once
#include "../include/entity.h"
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

struct SkySubMesh {
    // GPU objects for this primitive
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
};

struct SkyMesh {
    std::vector<SkySubMesh> submeshes;
};


struct SkyTexture {
    GLuint id;
    std::string path;
    GLuint frontFaceTexID;
    
};


class LoadSkybox : public GameObj {

public:
	

    SkyMesh m_skymesh;
    bool m_SkyLoaded = false;

    LoadSkybox(int idx, const std::string& name, int m_skyIdx);
    ~LoadSkybox();

    void SkyBox();
    void DrawSkyBox(Shader* shader, const glm::mat4& view, const glm::mat4& projection);

    std::vector<SkyTexture> loadSkyTextureFromFolder(const std::string& folderPath);
   // std::vector<SkyTexture> loadSkyTextureFromFolder(const std::string& folderPath, const std::string& skyFile);
    

    // NEW: load cubemap images from a folder (expects a strip or layout compatible with extract_face)
     bool LoadFromFolder(const std::string& folderPath);
    //bool LoadFromFolder(const std::string& folderPath, const std::string& skyFile);

    GLuint getTextureID() const { return sky_textureID; }
    GLuint getFrontFaceTextureID() const { return frontFaceTexID; }

    void DestroySkyMesh(SkyMesh& m_skymesh);

    


private:
    GLuint sky_textureID = 0;
    GLuint frontFaceTexID = 0;

    int idx = 0;
    std::string name;
    int m_skyIdx = 0;
};
