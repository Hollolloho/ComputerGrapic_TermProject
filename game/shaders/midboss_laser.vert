#version 330 core

in vec3 position;
uniform mat4 transform;
out vec3 beamPoint;

void main()
{
    beamPoint = position;
    gl_Position = transform * vec4(position, 1.0);
}
