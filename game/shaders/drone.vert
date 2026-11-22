#version 330 core

in vec3 position;
in vec3 normal;

uniform mat4 transform;
uniform mat4 model;
out vec3 droneNormal;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    droneNormal = transpose(inverse(mat3(model))) * normal;
}
