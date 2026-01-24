#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vTexCoord;
out vec3 vNormal;

void main()
{
    vTexCoord = aTexCoord;
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}







//#version 460 core
//
//// Simple vertex shader that accepts a vec3 position (location = 0)
//// and outputs clip-space position.
//layout(location = 0) in vec3 aPosition;
//layout(location = 1) in vec3 aColor; // Normals needs to be changed
//layout(location = 2) in vec2 aTexCoord;
//
//// names now match fragment shader inputs
//out vec3 uColor;
//out vec2 TexCoord;
//
//void main()
//{
//    gl_Position = vec4(aPosition, 1.0);
//    uColor = aColor;
//    TexCoord = aTexCoord;
//}