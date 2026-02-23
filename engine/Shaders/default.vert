#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTang; // CHANGED: tangent is a vec3 to match loader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vTexCoord;
out vec3 vNormal;    // world-space normal
out vec3 vFragPos;   // world-space position

void main()
{
    vTexCoord = aTexCoord;

    // normal matrix (transform normals to world-space)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vNormal = normalize(normalMatrix * aNormal);

    vec4 worldPos = model * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;

    gl_Position = projection * view * worldPos;
}

//#version 460 core
//
//layout(location = 0) in vec3 aPos;
//layout(location = 1) in vec3 aNormal;
//layout(location = 2) in vec2 aTexCoord;
//layout(location = 3) in vec3 aTang;
//
//uniform mat4 model;
//uniform mat4 view;
//uniform mat4 projection;
//
//out vec2 vTexCoord;
//out vec3 vNormal;    // world-space normal
//out vec3 vFragPos;   // world-space position
//
//void main()
//{
//    vTexCoord = aTexCoord;
//
//    // normal matrix (transform normals to world-space)
//    mat3 normalMatrix = transpose(inverse(mat3(model)));
//    vNormal = normalize(normalMatrix * aNormal);
//
//    vec4 worldPos = model * vec4(aPos, 1.0);
//    vFragPos = worldPos.xyz;
//
//    gl_Position = projection * view * worldPos;
//}
//
