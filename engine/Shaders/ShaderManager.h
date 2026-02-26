#pragma once
#include "../include/Shader.h"
#include <memory>
#include <string>

class ShaderManager {
public:
    // Initialize and load shaders (pass the directory where shader files live)
    static void SetupShaders(const std::string& shaderDir);

    // Free shader resources
    static void Shutdown();

    // Non-owning accessors
    static Shader* Default() { return defaultShader.get(); }
    static Shader* Sky() { return skyShader.get(); }

private:
    static std::unique_ptr<Shader> defaultShader;   // Default shader for rendering most objects
    static std::unique_ptr<Shader> skyShader;       // Sky shader
};

