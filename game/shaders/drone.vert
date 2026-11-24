#version 330 core

in vec3 position;
in vec3 normal;

uniform mat4 transform;
uniform mat4 model;
out vec3 droneNormal;
out vec3 worldPosition;
out vec3 beamPosition;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    droneNormal = transpose(inverse(mat3(model))) * normal;
    worldPosition = vec3(model * vec4(position, 1.0));
    beamPosition = position;
}
