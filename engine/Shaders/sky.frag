#version 460 core
in vec3 TexCoords;
out vec4 FragColor;
uniform samplerCube skybox;
void main() {
    vec4 col = texture(skybox, TexCoords);
    FragColor = col;
}


//    #version 460 
//    uniform samplerCube skybox;
//
//in vec3 TexCoords;
//out vec4 FragColor;
//
//void main() {
//    FragColor = texture(skybox, TexCoords);
//}