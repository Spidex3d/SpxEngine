#version 460

out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D spriteTexture;

void main() {
    vec4 c = texture(spriteTexture, TexCoord);
    if (c.a < 0.01) discard; // optional
    
    FragColor = c;
}

