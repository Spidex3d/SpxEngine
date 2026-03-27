#version 460

//out vec4 FragColor;
//
//in vec2 TexCoord;
//
//uniform sampler2D spriteTexture;
//uniform vec3 u_tint;    // added: multiply sprite color by tint
//uniform float u_alpha;  // optional overall alpha
//
//void main()
//{
//    vec4 tex = texture(spriteTexture, TexCoord);
//    FragColor = vec4(tex.rgb * u_tint, tex.a * u_alpha);
//}

out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D spriteTexture;

void main() {
    vec4 c = texture(spriteTexture, TexCoord);
    if (c.a < 0.01) discard; // optional
    
    FragColor = c;
}

