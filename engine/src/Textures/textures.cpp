#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "textures.h"
#include "stb/stb_image.h"
#include "stb/stb_image_resize.h" 
#include "../include/log.h"
#include <unordered_map>
#include <filesystem>
#include <mutex>
#include <utility>
namespace fs = std::filesystem;
// Internal cache: path -> (texID, refcount)
namespace {
    static std::unordered_map<std::string, std::pair<GLuint, int>> s_cache;
    static std::mutex s_cacheMutex;
}

static bool IsImageExt(const std::string& ext) {
    if (ext.empty()) return false;
    std::string e = ext;
    for (auto& c : e) c = (char)tolower(c);
    return (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" || e == ".tga");
}

// ###################### Helper to build a preview cache key ################################
static std::string MakePreviewKey(const std::string& path, int maxSide) {
    std::ostringstream ss;
    ss << path << "|preview:" << maxSide;
    return ss.str();
}

static GLuint UploadTextureRGBA8(const unsigned char* pixels, int width, int height, int channels)
{
    if (!pixels || width <= 0 || height <= 0) return 0;
    GLenum internalFormat = (channels == 4) ? GL_RGBA8 : GL_RGB8;
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}




GLuint TextureManager::Load(const std::string& path) {
    {
        std::lock_guard<std::mutex> lk(s_cacheMutex);
        auto it = s_cache.find(path);
        if (it != s_cache.end()) {
            // Increment refcount and return existing texture id
            it->second.second += 1;
            return it->second.first;
        }
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
        s_cache.emplace(path, std::make_pair(tex, 1));
    }

    LOG_INFO("TextureManager: Loaded texture " + path);
    return tex;
}

GLuint TextureManager::LoadPreview(const std::string& path, int maxSide)
{
    if (path.empty() || maxSide <= 0) return 0;

    std::string key = MakePreviewKey(path, maxSide);

    // check cache
    {
        std::lock_guard<std::mutex> lk(s_cacheMutex);
        auto it = s_cache.find(key);
        if (it != s_cache.end()) {
            it->second.second += 1;
            return it->second.first;
        }
    }

    // Load full image with stb_image (force 4 channels)
    stbi_set_flip_vertically_on_load(true);
    int w = 0, h = 0, nch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &nch, 4);
    if (!data) {
        LOG_WARNING("TextureManager::LoadPreview: Failed to load image: " << path);
        return 0;
    }

    int outW = w;
    int outH = h;
    unsigned char* outData = nullptr;
    bool resized = false;

    int maxDim = std::max(w, h);
    if (maxDim > maxSide) {
        // compute output size preserving aspect ratio
        if (w >= h) {
            outW = maxSide;
            outH = (h * maxSide) / w;
        }
        else {
            outH = maxSide;
            outW = (w * maxSide) / h;
        }
        outData = (unsigned char*)malloc(outW * outH * 4);
        if (!outData) {
            LOG_WARNING("TextureManager::LoadPreview: malloc failed for preview buffer");
            stbi_image_free(data);
            return 0;
        }
        int ok = stbir_resize_uint8(data, w, h, 0, outData, outW, outH, 0, 4);
        if (!ok) {
            LOG_WARNING("TextureManager::LoadPreview: stbir_resize_uint8 failed for " << path);
            free(outData);
            stbi_image_free(data);
            return 0;
        }
        resized = true;
    }
    else {
        // use original data directly (no resize)
        outData = data;
    }

    // Upload preview texture
    GLuint tex = UploadTextureRGBA8(outData, outW, outH, 4);

    // free buffers
    if (resized && outData) free(outData);
    if (!resized && data) stbi_image_free(data);

    if (tex == 0) {
        LOG_WARNING("TextureManager::LoadPreview: UploadTextureRGBA8 failed for " << path);
        return 0;
    }

    // cache it under preview key
    {
        std::lock_guard<std::mutex> lk(s_cacheMutex);
        s_cache.emplace(key, std::make_pair(tex, 1));
    }

    LOG_INFO("TextureManager::LoadPreview: Loaded preview " << path << " -> " << outW << "x" << outH);
    return tex;
}

std::vector<TexTexture> TextureManager::LoadTexturesFromDirectory(const std::string& texFolderPath)
{
    std::vector<TexTexture> out;
    if (texFolderPath.empty()) return out;

    fs::path p(texFolderPath);

    // If single file provided, load just that preview
    if (fs::exists(p) && fs::is_regular_file(p)) {
        if (!IsImageExt(p.extension().string())) return out;
        GLuint preview = TextureManager::LoadPreview(p.string(), 256); // default preview size
        if (preview) out.push_back({ 0u, p.string(), preview });
        return out;
    }

    // Otherwise treat as directory
    if (!fs::exists(p) || !fs::is_directory(p)) {
        std::cerr << "LoadTexturesFromDirectory: path not found or not a directory: " << texFolderPath << "\n";
        return out;
    }

    for (const auto& entry : fs::directory_iterator(p)) {
        if (!entry.is_regular_file()) continue;
        if (!IsImageExt(entry.path().extension().string())) continue;

        std::string filepath = entry.path().string();
        GLuint preview = TextureManager::LoadPreview(filepath, 256);
        if (preview) {
            // do not load full texture here; only preview for UI. Full load happens on apply.
            out.push_back({ 0u, filepath, preview });
        }
        else {
            std::cerr << "LoadTexturesFromDirectory: failed to load preview: " << filepath << "\n";
        }
    }

    return out;

    //std::vector<TexTexture> out;
    //if (texFolderPath.empty()) return out;

    //fs::path p(texFolderPath);

    //// If single file provided, load just that one (TextureManager handles actual image load)
    //if (fs::exists(p) && fs::is_regular_file(p)) {
    //    if (!IsImageExt(p.extension().string())) return out;
    //    GLuint tex = TextureManager::Load(p.string());
    //    if (tex) out.push_back({ tex, p.string(), tex });
    //    return out;
    //}

    //// Otherwise treat as directory
    //if (!fs::exists(p) || !fs::is_directory(p)) {
    //    std::cerr << "LoadTexturesFromDirectory: path not found or not a directory: " << texFolderPath << "\n";
    //    return out;
    //}

    //for (const auto& entry : fs::directory_iterator(p)) {
    //    if (!entry.is_regular_file()) continue;
    //    if (!IsImageExt(entry.path().extension().string())) continue;

    //    std::string filepath = entry.path().string();
    //    GLuint tex = TextureManager::Load(filepath);
    //    if (tex) {
    //        // Use same tex ID for id and frontFaceTexID for simple preview usage
    //        out.push_back({ tex, filepath, tex });
    //    }
    //    else {
    //        std::cerr << "LoadTexturesFromDirectory: failed to load: " << filepath << "\n";
    //    }
    //}

    //return out;
}



bool TextureManager::IsLoaded(const std::string& path) {
    std::lock_guard<std::mutex> lk(s_cacheMutex);
    return s_cache.find(path) != s_cache.end();
}

bool TextureManager::Unload(const std::string& path) {
    std::lock_guard<std::mutex> lk(s_cacheMutex);
    auto it = s_cache.find(path);
    if (it == s_cache.end()) {
        return false;
    }

    // decrement refcount
    it->second.second -= 1;
    if (it->second.second <= 0) {
        GLuint tex = it->second.first;
        if (tex != 0) {
            glDeleteTextures(1, &tex);
        }
        s_cache.erase(it);
        LOG_INFO("TextureManager: Unloaded texture " + path);
    }
    else {
        LOG_INFO("TextureManager: Decremented refcount for " + path + " -> " + std::to_string(it->second.second));
    }
    return true;
}

bool TextureManager::Unload(GLuint texID) {
    std::lock_guard<std::mutex> lk(s_cacheMutex);
    for (auto it = s_cache.begin(); it != s_cache.end(); ++it) {
        if (it->second.first == texID) {
            // found entry
            it->second.second -= 1;
            const std::string path = it->first;
            if (it->second.second <= 0) {
                if (texID != 0) {
                    glDeleteTextures(1, &texID);
                }
                s_cache.erase(it);
                LOG_INFO("TextureManager: Unloaded texture (by ID) " + path);
            }
            else {
                LOG_INFO("TextureManager: Decremented refcount for (by ID) " + path + " -> " + std::to_string(it->second.second));
            }
            return true;
        }
    }
    return false;
}

void TextureManager::UnloadAll() {
    std::lock_guard<std::mutex> lk(s_cacheMutex);
    for (auto& entry : s_cache) {
        GLuint tex = entry.second.first;
        if (tex != 0) {
            glDeleteTextures(1, &tex);
        }
    }
    s_cache.clear();
    LOG_INFO("TextureManager: Unloaded all textures");
}


