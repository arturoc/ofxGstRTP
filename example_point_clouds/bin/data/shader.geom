#version 150

uniform float max_distance;
uniform mat4 modelViewProjectionMatrix;

in vec2 texcoordVarying[3];
out vec2 geoTexture;

layout(triangles) in;
layout (triangle_strip, max_vertices = 3) out;

void main(){		
	if( abs(gl_in[0].gl_Position.z - gl_in[1].gl_Position.z)>max_distance ||
		abs(gl_in[0].gl_Position.z - gl_in[2].gl_Position.z)>max_distance ||
		abs(gl_in[1].gl_Position.z - gl_in[2].gl_Position.z)>max_distance){
		return;
	}else{
		geoTexture = texcoordVarying[0];
		gl_Position = modelViewProjectionMatrix * gl_in[0].gl_Position;
		EmitVertex();
		geoTexture = texcoordVarying[1];
		gl_Position = modelViewProjectionMatrix * gl_in[1].gl_Position;
		EmitVertex();
		geoTexture = texcoordVarying[2];
		gl_Position = modelViewProjectionMatrix * gl_in[2].gl_Position;
		EmitVertex();
		EndPrimitive();
	}
}