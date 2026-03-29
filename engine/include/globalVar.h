#pragma once
#include <vector>
#include <string>

extern const float TWO_PI; // useful for rotation wrapping; define in globalVar.cpp as 6.28318530717958647692f for precision
extern const float DEFAULT_ROT_SPEED_DEG; // degrees per second for rotation of objects with rotateY enabled

constexpr const char* TEXTURE_PATH = "assets/textures/texture/";
constexpr const char* SHADER_PATH = "Shaders/";
constexpr const char* LIGHT_PATH = "src/Effects/";
constexpr const char* SCENE_PATH = "Scene_Files/";

extern bool ShouldAddSkyBox; // if set to true add skybox images to buttons
extern bool ShouldAddTextures; // if set to true add textures to buttons in asset browser tab

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


// Lighting
extern const int OBJ_LIGHT;
extern const int OBJ_LIGHT_AMBIENT;
extern const int OBJ_LIGHT_SPOT;

//extern bool ShouldAddPlane;


extern const int OBJ_FLOOR;
extern const int OBJ_TILE;

// Skybox types
extern const int SKY_OBJ; // for skybox
//extern const int SKY_DOME_OBJ; // reserved for special skybox objects
//extern const int SKY_PBR_OBJ; // reserved for skybox PBR version with multiple textures (albedo, normal, roughness etc)