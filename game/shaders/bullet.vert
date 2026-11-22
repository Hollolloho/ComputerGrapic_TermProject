#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
uniform mat4 transform;
uniform mat4 modelView;
out vec3 viewPosition;
out vec3 viewNormal;
out vec3 crystalPosition;

void main()
{
    crystalPosition = position;
    viewPosition = vec3(modelView * vec4(position, 1.0));
    viewNormal = mat3(modelView) * normal;
    gl_Position = transform * vec4(position, 1.0);
}
