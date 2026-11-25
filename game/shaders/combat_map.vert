#version 330 core

in vec3 position;
in vec3 normal;

uniform mat4 transform;
out vec3 worldPos;
out vec3 mapNormal;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    worldPos = position;
    mapNormal = normal;
}
