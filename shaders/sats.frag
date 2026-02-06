#version 330 core
out vec4 FragColor;

in float vBright;

uniform int uIsHighlight;

void main(){
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(p,p);
    if(r2 > 1.0) discard;

    float core = smoothstep(1.0, 0.0, r2);
    float glow = smoothstep(1.0, 0.0, r2 * 0.7);

    float b = clamp(vBright, 0.0, 1.0);

    float vis = mix(0.35, 1.0, b);

    vec3 col;
    float a;

    if(uIsHighlight == 1){
        col = vec3(1.0, 0.85, 0.25) * (0.55 + 0.95 * glow);
        a   = 0.30 + 0.70 * core;
    }else{
        col = vec3(1.0) * (0.25 + 0.85 * glow) * vis;

        a   = (0.10 + 0.90 * core) * mix(0.50, 1.0, b);
    }

    FragColor = vec4(col, a);
}
