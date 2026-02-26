#pragma once
#include "../include/Shader.h"
#include "../Camera/Camera.h"
#include "../include/entity.h"

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#include <glm/glm.hpp>
#include <stb\stb_image.h>
#include <json/json.hpp>

using json = nlohmann::json;
// ###########################################################
// This is a re-vamp of my old gltf loader which only loaded the mesh
// this one loades mesh and textures, and is designed to be used as a GameObj in the spxengine's entity system.
// It also has a DrawGltf method that binds textures and issues draw calls per submesh,
// so it can be rendered by the engine's main render loop. The loader is basic and only supports a subset of glTF features
// (e.g., no animations, no PBR extensions beyond baseColor/metallicRoughness),
// but it should be enough to get you started with loading simple glTF models into your engine. We can expand it later as needed.
// ###########################################################
struct SubMesh {
	// GPU objects for this primitive
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;
	GLuint textureID = 0;
	// textures (baseColor, normal, etc.)
	std::map<std::string, GLuint> textures;
	// index count for this submesh
	size_t indexCount = 0;
	// new material parameters:
	glm::vec3 baseColorFactor = glm::vec3(1.0f);
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	// spec-gloss extension (optional)
	glm::vec3 specularFactor = glm::vec3(0.5f); // specular color (if provided)
	float glossinessFactor = 0.5f; // 0..1 (if provided)

	// derived convenience value for classic Blinn-Phong style shader:
	float shininess = 20.0f; // default 32.0f; will be computed from roughness or glossiness
};

struct GLTFMesh {
	std::vector<SubMesh> submeshes;
};

class gltf : public GameObj {

public:
	
	GLTFMesh m_mesh;
	bool m_Loaded = false;

	gltf(int idx, const std::string& name, int m_modelGltfIdx);
	~gltf();

	bool LoadGLTF(const std::string& gltfPath, const std::string& binPath); // file path and bin path for the gltf model

	std::string ReadGltfTextFile(const std::string& path);

	std::vector<unsigned char> ReadGltfBinaryFile(const std::string& path);

	

	GLTFMesh LoadGLTFMesh(const std::string& gltfPath, const std::string& binPath);

	GLuint loadTextureFromImageIndex(const json& gltf, int imageIndex, const std::string& gltfPath);

	std::map<std::string, GLuint> loadTextureForMaterial(const json& gltf, int materialIndex, const std::string& gltfPath);

	//void DrawGltf(Shader& shader, Camera& camera);
	void DrawGltf();

	void DestroyGLTFMesh(GLTFMesh& m_mesh);

	bool IsLoaded() const { return m_Loaded; }
	
private:
	
	int idx = 0;
	std::string name;
	int m_modelGltfIdx = 0;  // index provided by engine (for bookkeeping)Idx = 0;
	
	std::string modelPathGltf;
	


};