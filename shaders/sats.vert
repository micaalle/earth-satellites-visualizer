#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aBright;

uniform mat4 uView;
uniform mat4 uProj;

uniform int   uIsHighlight;
uniform float uBaseSize;
uniform float uHighlightSize;

out float vBright;

void main(){
    gl_Position = uProj * uView * vec4(aPos, 1.0);

    float w = max(gl_Position.w, 1.0);
    float size = uBaseSize + 120.0 / w;

    if(uIsHighlight == 1){
        gl_PointSize = uHighlightSize;
        vBright = 1.0;
    }else{
        gl_PointSize = clamp(size, 2.0, 14.0);
        vBright = aBright;
    }
}
