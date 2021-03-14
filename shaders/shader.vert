#version 420

uniform mat4 MVP;

in vec3 vPos;

void main() {
    gl_Position = MVP * vec4(vPos, -1.0);
}
