#version 460

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;

out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec2 p = ((pos.xy / pos.w) + 1) / 2;
    vec4 texv = texture(uTexture, p);
    FragColor = texv * texv.w + color * (1-texv.w);
}