#version 330 core
out vec4 FragColor;

uniform float uAlpha;

void main(){
    FragColor = vec4(1.0, 1.0, 1.0, uAlpha);
}
