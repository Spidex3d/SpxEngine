#include "../include/globalVar.h"



bool ShouldAddSkyBox = false;

const int MAIN_GRID = 0; // this is for the editor grid
const int OBJ_CUBE = 1;
const int OBJ_PLANE = 2; // use ffor walls and stuff
const int OBJ_CIRCLE = 3;
const int OBJ_LINE = 4;
const int OBJ_SPHERE = 5;
const int OBJ_CYLINDER = 6;
const int OBJ_TORUS = 7;
const int OBJ_GRID = 8;
const int OBJ_CONE = 9;
const int OBJ_PYRAMID = 10;
const int OBJ_TRIANGEL = 11;

const int OBJ_OBJ_MODEL = 15; // generic OBJ model loaded from file.
const int GLTF_OBJ_MODEL = 16; // generic GLTF model loaded from file.

const int SKY_OBJ = 30; // for skybox
//const int SKYA_OBJ = 31; // reserved for special skybox objects
//const int SKYB_OBJ = 32; // reserved for skybox PBR version with multiple textures (albedo, normal, roughness etc)

std::string modelPathObj = "";


const int OBJ_FLOOR = 20; // a large plane to serve as the floor.