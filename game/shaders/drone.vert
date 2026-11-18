#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 transform;
uniform mat4 model;
out vec3 droneNormal;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    droneNormal = mat3(transpose(inverse(model))) * normal;
}
