#include "ShaderManager.h"
#include "../include/asset_path.h" // if you want to resolve asset paths here (optional)
#include "log.h"

std::unique_ptr<Shader> ShaderManager::defaultShader = nullptr;
std::unique_ptr<Shader> ShaderManager::lightSpriteShader = nullptr; // <-- added this for light sprite shader
std::unique_ptr<Shader> ShaderManager::skyShader = nullptr;

void ShaderManager::SetupShaders(const std::string& shaderDir)
{
    // Accept either a directory or full paths; here we build expected paths from shaderDir
    std::string vertDefault = shaderDir + "default.vert";
    std::string fragDefault = shaderDir + "default.frag";
    defaultShader = std::make_unique<Shader>(vertDefault, fragDefault);
    if (!defaultShader) {
        LOG_WARNING("ShaderManager: failed to create default shader");
    }
    else {
        LOG_INFO("ShaderManager: default shader loaded: vert "<< vertDefault.c_str() << " frag " << fragDefault.c_str());
    }
    // This is the shader for drawing the sprite for the lights
    std::string vertSprite = shaderDir + "sprite.vert";
    std::string fragSprite = shaderDir + "sprite.frag";
    lightSpriteShader = std::make_unique<Shader>(vertSprite, fragSprite);
    if (!lightSpriteShader) {
        LOG_WARNING("ShaderManager: failed to create sky shader");
    }
    else {
        LOG_INFO("ShaderManager: sprite shader loaded: " << vertSprite.c_str() << " frag " << fragSprite.c_str());
    }

    std::string vertSky = shaderDir + "sky.vert";
    std::string fragSky = shaderDir + "sky.frag";
    skyShader = std::make_unique<Shader>(vertSky, fragSky);
    if (!skyShader) {
        LOG_WARNING("ShaderManager: failed to create sky shader");
    }
    else {
        LOG_INFO("ShaderManager: sky shader loaded: " << vertSky.c_str() << " frag " << fragSky.c_str());
    }
}

void ShaderManager::Shutdown()
{
    if (defaultShader) {
        defaultShader.reset();
    }
    /*if (lightSpriteShader) {
        lightSpriteShader.reset();
    }*/
    if (skyShader) {
        skyShader.reset();
    }

}



