#pragma once
#include <string>

struct Entity { // Any game object not player-related 
	int entId;
	std::string entName;
	
	float entPozx, entPozy, entPoxz;
	float entScaleX, entScaleY, entScaleZ;
	float entRotX, entRotY, entRotZ;
	int entPoints; // value or score associated with the entity

	bool isActive;
	bool isHealthPack;
	bool isDangerous;
	bool isCollidable;
	bool isVisible; 
};

class Entity // Give this more thought !!
{
public:
	Entity();
	~Entity();

private:

};

