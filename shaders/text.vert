#version 420

uniform mat4 MVP;

out vec2 texCoord;

in vec3 vPos;
in vec2 vTexCoord;

void main() {
    texCoord = vTexCoord;
    gl_Position = MVP * vec4(vPos, 1.0);;
}
