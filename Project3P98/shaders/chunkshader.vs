
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;

out vec3 fragpos;
out vec3 normal;

uniform mat4 projectionViewMatrix;

void main() {
	fragpos = pos;
	normal = norm;
	gl_Position = projectionViewMatrix * vec4(pos, 1.0);
}