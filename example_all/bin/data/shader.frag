#version 150
in vec2 geoTexture;

out vec4 out_color;

uniform sampler2DRect tex;

void main() {
   vec4 color = texture(tex,geoTexture);
   out_color = color;
}