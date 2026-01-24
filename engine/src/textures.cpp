//#define STB_IMAGE_IMPLEMENTATION
#include "../include/textures.h"
#include "stb/stb_image.h"
#include "../include/log.h"
#include <mutex>

namespace {
    static std::unordered_map<std::string, GLuint> s_cache;
    static std::mutex s_cacheMutex;
}

GLuint TextureManager::Load(const std::string& path) {
    {
        std::lock_guard<std::mutex> lk(s_cacheMutex);
        auto it = s_cache.find(path);
        if (it != s_cache.end()) return it->second;
    }

    stbi_set_flip_vertically_on_load(true);
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) {
        LOG_WARNING("TextureManager: Failed to load image: " << path.c_str());
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    {
        std::lock_guard<std::mutex> lk(s_cacheMutex);
        s_cache.emplace(path, tex);
    }

    LOG_INFO("TextureManager: Loaded texture %s -> %u" + path);
    return tex;
}

bool TextureManager::IsLoaded(const std::string& path) {
    std::lock_guard<std::mutex> lk(s_cacheMutex);
    return s_cache.find(path) != s_cache.end();
}






//#define STB_IMAGE_IMPLEMENTATION
//#include "../include/textures.h"
//#include "../include/log.h"
//#include "stb/stb_image.h"
//#include <iostream>
//
//
//Texture::Texture(const std::string& path) {
//    stbi_set_flip_vertically_on_load(true);
//
//    //unsigned char* data = stbi_load(path.c_str(), &width_, &height_, &channels_, 0);
//    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nr_Channels, 0);
//    if (!data) {
//        LOG_DEBUG("Texture failed to load at path: " + path + " : " + stbi_failure_reason());
//        tex_ID = 0;
//        return;
//    }
//    else {
//        LOG_TRACE("Texture loaded successfully from path: " + path);
//    }
//
//    GLenum i_format = GL_RGB;
//    if (nr_Channels == 1) i_format = GL_RED;
//    else if (nr_Channels == 3) i_format = GL_RGB;
//    else if (nr_Channels == 4) i_format = GL_RGBA; // PNG with alpha
//
//    glGenTextures(1, &tex_ID);
//    glBindTexture(GL_TEXTURE_2D, tex_ID);
//
//    // Upload the texture to GPU
//    glTexImage2D(GL_TEXTURE_2D, 0, i_format, width, height, 0, i_format, GL_UNSIGNED_BYTE, data);
//
//    // Generate mipmaps (nice default)
//    glGenerateMipmap(GL_TEXTURE_2D);
//
//    // Simple, sensible defaults for wrapping/filtering for beginners
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//    // Unbind and free image memory
//    glBindTexture(GL_TEXTURE_2D, 0);
//    stbi_image_free(data);
//}
//
//Texture::~Texture() {
//    if (tex_ID != 0) { // only delete if valid
//        glDeleteTextures(1, &tex_ID); // delete the OpenGL texture
//        tex_ID = 0; // set to 0 after deletion
//    }
//}
//
//
//
//void Texture::Bind(unsigned int unit) const {
//    glActiveTexture(GL_TEXTURE0 + unit); // adds offset to texture unit GL_TEXTURE0 1,2,3...
//    glBindTexture(GL_TEXTURE_2D, tex_ID); // bind the texture to the active texture unit
//}
//
//void Texture::Unbind(unsigned int unit) {
//    glActiveTexture(GL_TEXTURE0 + unit);
//    glBindTexture(GL_TEXTURE_2D, 0);
//}