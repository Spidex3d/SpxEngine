#include "lighting.h"

LightManager::LightManager() = default;
LightManager::~LightManager() = default;

int LightManager::AddAmbient(const glm::vec3& color, float intensity) {
    Light L;
    L.id = m_nextId++;
    L.type = LightType::Ambient;
    L.color = color;
    L.intensity = intensity;
    L.enabled = true;
    L.linkedEntityIndex = -1;
    m_lights.push_back(L);
    LOG_INFO("LightManager: added ambient light id=" << L.id);
    return L.id;
}

int LightManager::AddSpot(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float intensity, float cutoffCos) {
    Light L;
    L.id = m_nextId++;
    L.type = LightType::Spot;
    L.position = pos;
    L.direction = glm::normalize(dir);
    L.color = color;
    L.intensity = intensity;
    L.cutoffCos = cutoffCos;
    L.enabled = true;
    L.linkedEntityIndex = -1;
    m_lights.push_back(L);
    LOG_INFO("LightManager: added spot light id=" << L.id << " pos=" << pos.x << "," << pos.y << "," << pos.z);
    return L.id;
}

bool LightManager::RemoveLight(int id) {
    for (auto it = m_lights.begin(); it != m_lights.end(); ++it) {
        if (it->id == id) {
            m_lights.erase(it);
            LOG_INFO("LightManager: removed light id=" << id);
            return true;
        }
    }
    return false;
}

Light* LightManager::GetLight(int id) {
    for (auto& l : m_lights) if (l.id == id) return &l;
    return nullptr;
}

Light* LightManager::FindByEntityIndex(int entityIndex) {
    for (auto& l : m_lights) if (l.linkedEntityIndex == entityIndex) return &l;
    return nullptr;
}

void LightManager::ApplyToShader(Shader* shader) {
    if (!shader) return;

    glm::vec3 ambientSum(0.0f);
    for (const auto& l : m_lights) {
        if (!l.enabled) continue;
        if (l.type == LightType::Ambient) {
            ambientSum += l.color * l.intensity;
        }
    }

    // Add global ambient (if any)
    ambientSum += m_globalAmbient * m_globalAmbientIntensity;

    shader->Use();
    shader->setVec3("u_ambient", ambientSum);

    //// Aggregate ambient contribution from ambient lights
    //glm::vec3 ambientSum(0.0f);
    //for (const auto& l : m_lights) {
    //    if (!l.enabled) continue;
    //    if (l.type == LightType::Ambient) {
    //        ambientSum += l.color * l.intensity;
    //    }
    //}
    //shader->Use();
    //shader->setVec3("u_ambient", ambientSum);

    // Collect up to MAX_SPOT_LIGHTS spots
    int slot = 0;
    for (const auto& l : m_lights) {
        if (!l.enabled) continue;
        if (l.type == LightType::Spot) {
            if (slot >= MAX_SPOT_LIGHTS) break;
            std::string base = std::string("u_spotLights[") + std::to_string(slot) + std::string("].");
            shader->setVec3((base + "position").c_str(), l.position);
            shader->setVec3((base + "direction").c_str(), l.direction);
            shader->setVec3((base + "color").c_str(), l.color * l.intensity);
            shader->SetUniformFloat((base + "cutoffCos").c_str(), l.cutoffCos);
            shader->SetUniformInt((base + "enabled").c_str(), 1);
            ++slot;
        }
    }
    // disable remaining slots
    for (int i = slot; i < MAX_SPOT_LIGHTS; ++i) {
        std::string base = std::string("u_spotLights[") + std::to_string(i) + std::string("].");
        shader->SetUniformInt((base + "enabled").c_str(), 0);
    }
}
