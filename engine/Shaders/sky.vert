#version 460 core
layout(location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 view;
uniform mat4 projection;
void main() {
    TexCoords = aPos;
    mat4 rotOnlyView = view; // we already removed translation in DrawSkyBox
    gl_Position = projection * rotOnlyView * vec4(aPos, 1.0);
    // ensure cube is at infinite distance by setting w = 1 and not translating
}



//#version 460 
//// skybox.vert
//layout(location = 0) in vec3 aPos;
//
//out vec3 TexCoords;
//
//uniform mat4 view;
//uniform mat4 projection;
//
//void main()
//{
//    TexCoords = aPos;
//    vec4 pos = projection * view * vec4(aPos, 1.0);
//    gl_Position = pos.xyww; // skybox trick
//}
