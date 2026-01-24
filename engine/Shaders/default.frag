#version 460 core
in vec2 vTexCoord;
in vec3 vNormal;

uniform sampler2D myTexture;

out vec4 FragColor;

void main()
{
    vec4 tex = texture(myTexture, vTexCoord);
    // simple lighting placeholder: modulate by a small ambient + normal-based light
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(vNormal), lightDir), 0.0);
    vec3 col = tex.rgb * (0.25 + 0.75 * diff);
    FragColor = vec4(col, tex.a);
}





//#version 460 core
//
//// Minimal fragment shader that outputs a textured color.
//
//out vec4 FragColor;
//
//in vec3 uColor;
//in vec2 TexCoord;
//
//uniform sampler2D myTexture; // sampler bound to texture unit 0 by default
//
//void main()
//{
//    // Sample texture with the interpolated texture coordinates
//    FragColor = texture(myTexture, TexCoord);
//}