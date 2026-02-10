#version 460 core

in vec2 vTexCoord;
in vec3 vNormal;
// (frag pos isn't used here, remove if unused)

out vec4 FragColor;

uniform sampler2D myTexture;

// highlight uniforms
uniform int u_selected;            // 1 = selected, 0 = not
uniform vec3 u_highlightColor;     // highlight color (rgb)

void main()
{
    // Use texture coordinates produced by the vertex shader
    vec4 base = texture(myTexture, vTexCoord);

    if (u_selected == 1) {
        // Blend highlight color into base color. Adjust factor to taste.
        float highlightMix = 0.35;
        vec3 blended = mix(base.rgb, u_highlightColor, highlightMix);
        FragColor = vec4(blended, base.a);
    } else {
        FragColor = base;
    }
}



//#version 460 core
//
//in vec2 vTexCoords;
//in vec3 vNormal;
//in vec3 FragPos;
//
//out vec4 FragColor;
//
//uniform sampler2D myTexture;
//
//// highlight uniforms
//uniform int u_selected;            // 1 = selected, 0 = not
//uniform vec3 u_highlightColor;     // highlight color (rgb)
//
//void main()
//{
//    // Example base color: use texture if present or fallback to white
//    vec4 base = texture(myTexture, vTexCoords);
//    // If your shader computes lighting, integrate highlight on the final color instead.
//
//    if (u_selected == 1) {
//        // Blend highlight color into base color. Adjust factor to taste.
//        float highlightMix = 0.25;
//        vec3 blended = mix(base.rgb, u_highlightColor, highlightMix);
//        FragColor = vec4(blended, base.a);
//    } else {
//        FragColor = base;
//    }
//}






//#version 460 core
//in vec2 vTexCoord;
//in vec3 vNormal;
//
//uniform sampler2D myTexture;
//
//out vec4 FragColor;
//
//void main()
//{
//    vec4 tex = texture(myTexture, vTexCoord);
//    // simple lighting placeholder: modulate by a small ambient + normal-based light
//    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
//    float diff = max(dot(normalize(vNormal), lightDir), 0.0);
//    vec3 col = tex.rgb * (0.25 + 0.75 * diff);
//    FragColor = vec4(col, tex.a);
//}
//




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