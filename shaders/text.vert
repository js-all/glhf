#version 420

uniform mat4 MVP;

out vec2 texCoord;
out vec4 color;

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 normals;
layout(location = 3) in vec4 vColor;

void main() {
    texCoord = vTexCoord;
    color = vColor;
    gl_Position = MVP * vec4(vPos, 1.0);;
}
