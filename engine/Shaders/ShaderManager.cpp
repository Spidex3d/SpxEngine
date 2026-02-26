#include "ShaderManager.h"
#include "../include/asset_path.h" // if you want to resolve asset paths here (optional)
#include "log.h"

std::unique_ptr<Shader> ShaderManager::defaultShader = nullptr;
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

    std::string vertSky = shaderDir + "sky.vert";
    std::string fragSky = shaderDir + "sky.frag";
    skyShader = std::make_unique<Shader>(vertSky, fragSky);
    if (!skyShader) {
        LOG_WARNING("ShaderManager: failed to create sky shader");
    }
    else {
        LOG_INFO("ShaderManager: sky shader loaded: %s, %s", vertSky.c_str(), fragSky.c_str());
    }
}

void ShaderManager::Shutdown()
{
    if (defaultShader) {
        defaultShader.reset();
    }
    if (skyShader) {
        skyShader.reset();
    }

}



