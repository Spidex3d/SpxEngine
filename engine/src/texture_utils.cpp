#include "entity.h"
#include "../include/textures.h"
#include <iostream>

// Sets texture for object from disk path. Calls TextureManager::Load and updates GameObj.
// If path empty, clears texture (unloads the old one via TextureManager).
bool SetTextureForGameObj(GameObj* obj, const std::string& path) {
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
            std::cerr << "SetTextureForGameObj: Failed to load " << path << std::endl;
            return false;
        }
        obj->tex_ID = tex;
        obj->texPath = path;
    }

    return true;
}