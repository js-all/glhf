#version 420

in vec2 texCoord;

out vec4 FragColor;

uniform vec4 color; 
uniform sampler2D uTexture;

void main() {
    float texv = texture(uTexture, texCoord).x;
    FragColor = vec4(color.xyz, color.w * texv);
}