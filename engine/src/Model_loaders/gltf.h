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

//using json = nlohmann::json;
//
//struct SubMesh {
//	GLuint gltfVAO = 0; // VAO for this submesh
//	GLuint gltfVBO = 0; // VBO for vertex data (positions, normals, texcoords)
//	GLuint gltfEBO = 0; // EBO for indices (if using indexed drawing)
//    GLuint textureID = 0;
//    size_t indexCount = 0;
//    std::map<std::string, GLuint> textures;  // e.g., "baseColor", "normal", etc.
//};
//
//struct GLTFMesh {
//    std::vector<SubMesh> submeshes;
//};
//
//class gltf : public GameObj {
//
//public:
//
//	gltf(int idx, const std::string& name, int modelGlTFIdx);
//	~gltf();
//
//	bool LoadGLTF(const std::string& gltfPath, const std::string& binPath); // file path and bin path for the gltf model
//
//	void DrawGltf(Shader& shader, Camera& camera);
//
//	std::string ReadGltfTextFile(const std::string& path);
//
//	std::vector<unsigned char> ReadGltfBinaryFile(const std::string& path);
//
//	template <typename T>
//	inline T* GetAccessorData(const json& gltf, const std::vector<unsigned char>& buffer, const json& accessor) {
//		const auto& bufferView = gltf["bufferViews"][accessor["bufferView"].get<int>()];
//		size_t offset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
//		return reinterpret_cast<T*>(const_cast<unsigned char*>(buffer.data() + offset));
//	}
//
//	GLTFMesh LoadGLTFMesh(const std::string& gltfPath, const std::string& binPath);
//
//	GLuint loadTextureFromImageIndex(const json& gltf, int imageIndex, const std::string& gltfPath);

};