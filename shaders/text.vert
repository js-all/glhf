#version 420

uniform mat4 MVP; 

layout(location = 0) out vec4 opos;
layout(location = 1) out vec4 color;

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 normals;
layout(location = 2) in vec4 vColor;

void main() {
    vec4 pos = MVP * vec4(vPos, 1.0);
    opos = pos;
    color = vColor;
    gl_Position = pos;
}
