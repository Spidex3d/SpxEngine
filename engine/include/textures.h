#pragma once
#include <string>
#include <unordered_map>

#include <glad/glad.h>

// Simple texture cache: load a texture once, keep GLuint handle for reuse.
// Note: this returns an OpenGL texture id; textures are kept until program exit.
// If you need explicit unloads, add an Unload or Clear API.
namespace TextureManager {
    // Load texture from disk (path). Returns 0 on failure, otherwise GL texture id.
    GLuint Load(const std::string& path);

    // Optional: query without loading
    bool IsLoaded(const std::string& path);

    bool Unload(const std::string& path);
    bool Unload(GLuint texID);
    void UnloadAll();

}


