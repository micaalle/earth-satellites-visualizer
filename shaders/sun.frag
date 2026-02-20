#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNrm;
out vec4 FragColor;

uniform vec3  uCamPos;
uniform float uTime;

uniform vec3  uBaseColor;   // (1.0, 0.65, 0.20) etc
uniform float uIntensity;   // 2.0 - 4.0 range usually

float hash31(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f*f*(3.0 - 2.0*f);

    float n000 = hash31(i + vec3(0,0,0));
    float n100 = hash31(i + vec3(1,0,0));
    float n010 = hash31(i + vec3(0,1,0));
    float n110 = hash31(i + vec3(1,1,0));
    float n001 = hash31(i + vec3(0,0,1));
    float n101 = hash31(i + vec3(1,0,1));
    float n011 = hash31(i + vec3(0,1,1));
    float n111 = hash31(i + vec3(1,1,1));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);

    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);

    return mix(nxy0, nxy1, f.z);
}

void main()
{
    vec3 N = normalize(vWorldNrm);
    vec3 V = normalize(uCamPos - vWorldPos);

    float mu = clamp(dot(N, V), 0.0, 1.0);
    float limb = pow(mu, 0.35);

    float t = uTime * 0.25;
    float n1 = noise(N * 14.0 + vec3(t, 0.0, -t));
    float n2 = noise(N * 28.0 + vec3(-t, t, 0.0));
    float gran = mix(n1, n2, 0.5);

    float bands = 0.15 * sin((N.y + t) * 18.0) + 0.10 * sin((N.x - t) * 24.0);

    float surf = 0.75 + 0.35 * gran + bands;
    surf = clamp(surf, 0.0, 2.0);

    vec3 col = uBaseColor * (limb * surf) * uIntensity;

    FragColor = vec4(col, 1.0);
}
