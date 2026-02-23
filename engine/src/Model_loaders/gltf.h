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

struct SubMesh {
	GLuint textureID = 0;
	size_t indexCount = 0;
	std::map<std::string, GLuint> textures;  // e.g., "baseColor", "normal", etc.
};
struct GLTFMesh {
	std::vector<SubMesh> submeshes;
};

class gltf : public GameObj {

public:
	GLuint gltfVAO = 0;
	GLuint gltfVBO = 0;
	GLuint gltfEBO = 0;
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
	//GLuint gltfVAO = 0; // VAO for this submesh
	//GLuint gltfVBO = 0; // VBO for vertex data (positions, normals, texcoords)
	//GLuint gltfEBO = 0; // EBO for indices (if using indexed drawing)
	//GLuint textureID = 0;
	//size_t indexCount = 0;
	//std::map<std::string, GLuint> textures;  // e.g., "baseColor", "normal", etc.

	int idx = 0;
	std::string name;
	int m_modelGltfIdx = 0;  // index provided by engine (for bookkeeping)Idx = 0;
	
	std::string modelPathGltf;
	


};