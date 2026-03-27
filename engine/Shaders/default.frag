#version 460

in vec3 vFragPos;   // ensure your vertex shader forwards world-space position
in vec3 vNormal;
in vec2 vTexCoord;

uniform sampler2D myTexture;
uniform vec3 u_ambient;

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float cutoffCos;
    int enabled;
};

uniform SpotLight u_spotLights[8]; // match MAX_SPOT_LIGHTS

uniform int u_selected;        // selection flag
uniform vec3 u_highlightColor; // highlight color (rgb)

out vec4 FragColor;

void main() {
    vec3 albedo = texture(myTexture, vTexCoord).rgb;

    // simple ambient
    vec3 color = albedo * u_ambient;

    // surface normal
    vec3 N = normalize(vNormal);
    vec3 viewDir = normalize(-vFragPos); // assuming camera at origin in world space for simplicity

    // add sloted spot lights
    for (int i = 0; i < 8; ++i) {
        if (u_spotLights[i].enabled == 0) continue;
        vec3 L = normalize(u_spotLights[i].position - vFragPos);
        float NdotL = max(dot(N, L), 0.0);
        // spot cutoff test:
        float spotFactor = dot(normalize(-u_spotLights[i].direction), L);
        if (spotFactor >= u_spotLights[i].cutoffCos) {
            // simple diffuse
            color += albedo * u_spotLights[i].color * NdotL;
        }
    }

    FragColor = vec4(color, 1.0);
}


//  if (u_selected == 1) {
//        float highlightMix = 0.35;
//        vec3 blended = mix(lit, u_highlightColor, highlightMix);
//        FragColor = vec4(blended, 1.0);
//    } else {
//        FragColor = vec4(lit, 1.0);
//    }

//#version 460 core
//
//in vec2 vTexCoord;
//in vec3 vNormal;
//in vec3 vFragPos;
//
//out vec4 FragColor;
//
//uniform sampler2D myTexture;   // bound to texture unit 0 by engine
//uniform int u_useTexture;      // 1 = sample texture, 0 = use u_albedo
//uniform vec3 u_albedo;         // fallback color when no texture (rgb)
//uniform int u_selected;        // selection flag
//uniform vec3 u_highlightColor; // highlight color (rgb)
//
//
//
//// Simple directional light (in world space)
//uniform vec3 u_lightDir = vec3(0.5, 1.0, 0.3); // can be overridden from CPU
//uniform vec3 u_lightColor = vec3(1.0);
//
//// simple material params
//const float kAmbient = 0.2;
//const float kDiffuse = 0.8;
//
//void main()
//{
//    // choose base color (texture or fallback)
//    vec3 baseColor = u_albedo;
//    if (u_useTexture == 1) { //0
//        vec4 tex = texture(myTexture, vTexCoord);
//        baseColor = tex.rgb;
//    }
//
//    // lighting: simple Lambertian directional light
//    vec3 N = normalize(vNormal);
//    vec3 L = normalize(u_lightDir);
//    float NdotL = max(dot(N, L), 0.0);
//
//    vec3 lit = baseColor * (kAmbient + kDiffuse * NdotL) * u_lightColor;
//
//    // apply selection highlight (blend)
//    if (u_selected == 1) {
//        float highlightMix = 0.35;
//        vec3 blended = mix(lit, u_highlightColor, highlightMix);
//        FragColor = vec4(blended, 1.0);
//    } else {
//        FragColor = vec4(lit, 1.0);
//    }
//}
//


//#version 460 core
//
//in vec2 vTexCoord;
//in vec3 vNormal;
//// (frag pos isn't used here, remove if unused)
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
//    // Use texture coordinates produced by the vertex shader
//    vec4 base = texture(myTexture, vTexCoord);
//
//    if (u_selected == 1) {
//        // Blend highlight color into base color. Adjust factor to taste.
//        float highlightMix = 0.35;
//        vec3 blended = mix(base.rgb, u_highlightColor, highlightMix);
//        FragColor = vec4(blended, base.a);
//    } else {
//        FragColor = base;
//    }
//}
//


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