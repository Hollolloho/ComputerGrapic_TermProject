#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
uniform mat4 transform;
uniform mat4 model;
out vec3 eyePosition;
out vec3 eyeNormal;
out vec2 uv;
void main() {
    eyePosition=vec3(model*vec4(position,1));
    eyeNormal=mat3(transpose(inverse(model)))*normal;
    uv=texcoord;
    gl_Position=transform*vec4(position,1);
}
