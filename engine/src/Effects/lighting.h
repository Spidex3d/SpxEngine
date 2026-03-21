#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <optional>
#include "../include/shader.h"
#include "../include/log.h"
#include "../include/globalVar.h" // add defs if you want a LIGHT_OBJ id
#include "../include/entity.h" // for GameObj reference (path adjust if needed)

enum class LightType {
    Ambient,
    Spot
};

struct Light {
    int id = -1;
    LightType type = LightType::Ambient;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    // spatial for non-ambient lights
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    float cutoffCos = 0.95f; // spot cutoff cos(theta)
    bool enabled = true;

    // optional link to a scene entity used as the visual marker (engine-owned index)
    // -1 == none
    int linkedEntityIndex = -1;
};

class LightManager {
public:
    LightManager();
    ~LightManager();

    // Add convenience helpers - they return assigned light id
    int AddAmbient(const glm::vec3& color = glm::vec3(1.0f), float intensity = 0.2f);
    int AddSpot(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f, float cutoffCos = 0.95f);

    bool RemoveLight(int id);

    // find pointer or nullptr
    Light* GetLight(int id);
    std::vector<Light>& GetLights() { return m_lights; }

    // Apply aggregated lighting uniforms to given shader (call before rendering objects)
    // Minimal: sets u_ambient and up to N spot lights as u_spotLights[i].*
    void ApplyToShader(Shader* shader);

    // Utility: find light by linked entity index (so UI can map a scene object back to a light)
    Light* FindByEntityIndex(int entityIndex);

    int GetNextId() const { return m_nextId; }

    
    glm::vec3 GetGlobalAmbient() const { return m_globalAmbient; }
    float GetGlobalAmbientIntensity() const { return m_globalAmbientIntensity; }
    void SetGlobalAmbient(const glm::vec3& color, float intensity) {
        m_globalAmbient = color;
        m_globalAmbientIntensity = intensity;
    }

private:
    std::vector<Light> m_lights;
    int m_nextId = 1;
    glm::vec3 m_globalAmbient = glm::vec3(0.0f);
    float m_globalAmbientIntensity = 0.0f;
 
    static constexpr int MAX_SPOT_LIGHTS = 8;
};
