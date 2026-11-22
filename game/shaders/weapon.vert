#version 330 core
in vec3 position;
in vec3 normal;
in vec2 texcoord;
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
