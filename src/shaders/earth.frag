#version 330 core
in vec3 vN;
in vec2 vUV;
in vec3 vWorldPos;
out vec4 FragColor;

uniform sampler2D uEarthTex;
uniform bool uHasTex;
uniform vec3 uSunDir;    
uniform vec3 uCamPos;

void main(){
    vec3 N = normalize(vN);
    vec3 L = normalize(-uSunDir);           
    float ndl = max(dot(N, L), 0.0);

    vec3 base = uHasTex ? texture(uEarthTex, vUV).rgb : vec3(0.12, 0.25, 0.65);
    vec3 night = base * 0.08;

    float dayFactor = smoothstep(0.0, 0.15, ndl);
    vec3 albedo = mix(night, base, dayFactor);

    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(V + L);
    //float spec = pow(max(dot(N, H), 0.0), 60.0) * 0.35;

    vec3 color = albedo * (0.25 + 0.95 * ndl);

    FragColor = vec4(color, 1.0);
}
