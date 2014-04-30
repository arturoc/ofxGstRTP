#version 150

in vec4 position;
in vec2 texcoord;

uniform float ref_pix_size;
uniform float ref_distance;
uniform float SCALE_UP;

out vec2 texcoordVarying;

uniform mat4 modelViewProjectionMatrix;

void main() {
	texcoordVarying = texcoord;
	
	float factor = 2 * ref_pix_size * position.z / ref_distance;
	vec3 pos;
	pos.x = (position.x*SCALE_UP - 640.0/2.0) * factor;
	pos.y = (position.y*SCALE_UP - 480.0/2.0) * factor * -1.0;
	pos.z = position.z * -1.0;
	gl_Position = vec4(pos,1.0);
}