#pragma once
#include <vector>
#include <string>

constexpr const char* TEXTURE_PATH = "assets/textures/texture/";
constexpr const char* SHADER_PATH = "Shaders/";

extern bool ShouldAddSkyBox;

extern const int MAIN_GRID;
extern const int OBJ_CUBE;
extern const int OBJ_PLANE;
extern const int OBJ_CIRCLE;
extern const int OBJ_LINE;
extern const int OBJ_SPHERE;
extern const int OBJ_CYLINDER;
extern const int OBJ_TORUS;
extern const int OBJ_GRID;
extern const int OBJ_CONE;
extern const int OBJ_PYRAMID;
extern const int OBJ_TRIANGEL;

extern const int OBJ_OBJ_MODEL; // generic OBJ model loaded from file. 15
extern const int GLTF_OBJ_MODEL; // generic GLTF model loaded from file. 16
extern std::string modelPathObj;
extern std::string modelPathGltf;


//extern bool ShouldAddPlane;


extern const int OBJ_FLOOR;

// Skybox types
extern const int SKY_OBJ; // for skybox
//extern const int SKYA_OBJ; // reserved for special skybox objects
//extern const int SKYB_OBJ; // reserved for skybox PBR version with multiple textures (albedo, normal, roughness etc)