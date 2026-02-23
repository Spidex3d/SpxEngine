#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring> // for std::memcpy used in VertexHash
#include <unordered_map>

#include <stb/stb_image.h>
#include <iostream>
#include <sstream>
#include <fstream>

#include "../include/entity.h"

// Simple OBJ loader that inherits GameObj so loaded models can be stored in the same container.
class objLoader : public GameObj
{
public:
    struct Material {
        std::string name;

        glm::vec3 ambient;      // Ka
        glm::vec3 diffuse;      // Kd
        glm::vec3 specular;     // Ks
        float shininess;        // Ns

        float transparency;     // d or Tr
        float opticalDensity;   // Ni
        int illumModel;         // illum

        // Texture maps
        std::string diffuseMap;     // map_Kd
        std::string specularMap;    // map_Ks
        std::string normalMap;      // map_bump or bump
        std::string alphaMap;       // map_d or map_opacity

        GLuint diffuseTexID = 0;
        GLuint specularTexID = 0;
        GLuint normalTexID = 0;
        GLuint alphaTexID = 0;

        Material()
            : ambient(0.2f), diffuse(0.8f), specular(1.0f),
            shininess(32.0f), transparency(1.0f), opticalDensity(1.0f), illumModel(2) {
        }
    };

    // ctor/dtor
    objLoader(int idx, const std::string& name, int modelObjIdx);
    ~objLoader();

    // core API
    bool Loadobj(const std::string& filename); // loads geometry + records material file
    bool LoadMTL(const std::string& filename); // load referenced MTL (modelPathObj + filename)
    void objModels();     // upload vertex + index buffers to GPU (VAO/VBO/EBO)
    void objDrawModels(); // draw using glDrawElements per-material submesh

    // helpers
    GLuint LoadObjTexture(const std::string& filename);
    //void LoadMaterialTextures(Material& mat, const std::string& filename);
    bool IsLoaded() const { return m_Loaded; }
    const std::string& GetModelDirectory() const { return modelPathObj; }

    bool HasDiffuseTexture() const;
    GLuint GetFirstDiffuseTexture() const;

private:
    // vertex representation used for deduplication + GPU upload
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
        glm::vec3 tangent;

        bool operator==(Vertex const& o) const noexcept {
            return position == o.position && normal == o.normal && texCoords == o.texCoords;
        }
    };

    // hash for Vertex using float bit patterns
    struct VertexHash {
        std::size_t operator()(Vertex const& v) const noexcept {
            auto floatBits = [](float f)->uint32_t {
                uint32_t b;
                std::memcpy(&b, &f, sizeof(f));
                return b;
            };
            // combine bit patterns
            uint64_t h1 = ((uint64_t)floatBits(v.position.x) << 32) ^ floatBits(v.position.y);
            uint64_t h2 = ((uint64_t)floatBits(v.position.z) << 32) ^ floatBits(v.normal.x);
            uint64_t h3 = ((uint64_t)floatBits(v.normal.y) << 32) ^ floatBits(v.normal.z);
            uint64_t h4 = ((uint64_t)floatBits(v.texCoords.x) << 32) ^ floatBits(v.texCoords.y);
            // mix them
            uint64_t h = h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
            h ^= (h3 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
            h ^= (h4 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
            return (std::size_t)h;
        }
    };

    // GPU resources
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    int idx = 0;
    std::string name;
    int modelObjIdx = 0;  // index provided by engine (for bookkeeping)

    bool m_Loaded = false;
    std::string modelPathObj; // directory where OBJ/MTL/textures live (ends with '/' or '\')

    // final indexed buffers uploaded to GPU
    std::vector<Vertex> m_UniqueVertices;
    std::vector<uint32_t> m_Indices; // flattened index buffer (all submeshes concatenated)

    // materials parsed from MTL
    std::unordered_map<std::string, Material> materials;

    // submesh: material name + index offset/count inside m_Indices
    struct SubMesh {
        std::string materialName;
        size_t indexOffset = 0;
        size_t indexCount = 0;
    };
    std::vector<SubMesh> submeshes;
};



//#pragma once
//#include <glad/glad.h>
//#include <glm/glm.hpp>
//#include <vector>
//#include <string>
//
//#include <stb/stb_image.h>
//
//#include <iostream>
//#include <sstream>
//#include <fstream>
//#include <unordered_map>
//
//#include "../include/entity.h"
//
//// Simple OBJ loader that inherits GameObj so loaded models can be stored in the same container.
//class objLoader : public GameObj
//{
//public:
//    struct Material {
//        std::string name;
//
//        glm::vec3 ambient;      // Ka
//        glm::vec3 diffuse;      // Kd
//        glm::vec3 specular;     // Ks
//        float shininess;        // Ns
//
//        float transparency;     // d or Tr
//        float opticalDensity;   // Ni
//        int illumModel;         // illum
//
//        // Texture maps
//        std::string diffuseMap;     // map_Kd
//        std::string specularMap;    // map_Ks
//        std::string normalMap;      // map_bump or bump
//        std::string alphaMap;       // map_d or map_opacity
//
//        GLuint diffuseTexID = 0;
//        GLuint specularTexID = 0;
//        GLuint normalTexID = 0;
//        GLuint alphaTexID = 0;
//
//        Material()
//            : ambient(0.2f), diffuse(0.8f), specular(1.0f),
//            shininess(32.0f), transparency(1.0f), opticalDensity(1.0f), illumModel(2) {
//        }
//    };
//
//    objLoader(int idx, const std::string& name, int modelObjIdx);
//    ~objLoader();
//
//    void Initialize();
//
//    std::vector<std::string> split(const std::string& s, const std::string& delimiter);
//    bool Loadobj(const std::string& filename); // loads OBJ file (sets m_Vertices and material file name)
//    bool LoadMTL(const std::string& filename); // loads MTL referenced by OBJ (uses modelPathObj + filename)
//    GLuint LoadObjTexture(const std::string& filename);
//    void objModels();     // upload vertex data to GPU (create VAO/VBO/EBO if needed)
//    void objDrawModels(); // draw the model (bind textures and call glDrawArrays/elements)
//    void LoadMaterialTextures(Material& mat, const std::string& filename);
//
//    // Optional helpers
//    bool IsLoaded() const { return m_Loaded; }
//    const std::string& GetModelDirectory() const { return modelPathObj; }
//
//private:
//    // GPU resources
//    GLuint VAO = 0;
//    GLuint VBO = 0;
//    GLuint EBO = 0; // reserved for future indexed implementation
//
//    int idx = 0;
//    std::string name;
//    int modelObjIdx = 0;  // index provided by engine (for bookkeeping)
//
//    bool m_Loaded = false;
//    std::string modelPathObj; // directory where OBJ/MTL/textures live (ends with '/' or '\')
//
//    struct Vertex {
//        glm::vec3 position;
//        glm::vec3 normal;
//        glm::vec2 texCoords;
//    };
//    std::vector<Vertex> m_Vertices;
//
//    std::unordered_map<std::string, Material> materials;  // materials parsed from MTL
//};
//
