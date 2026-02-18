#include "icon_loader.h"
#include "stb/stb_image.h"
#include <iostream>

GLuint LoadIconFromFile(const std::string& filename, int& out_width, int& out_height)
{
    int channels = 0;
    // stbi loads 4 channels (RGBA) so alpha works
    unsigned char* data = stbi_load(filename.c_str(), &out_width, &out_height, &channels, 4);
    if (!data) {
        std::cerr << "LoadIconFromFile: failed to load " << filename << "\n";
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Upload as RGBA8
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, out_width, out_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Basic sampling params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // optional: clamp to edge
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return tex;
}