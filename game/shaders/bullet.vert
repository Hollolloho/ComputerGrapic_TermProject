#version 330 core

in vec3 position;
in vec3 normal;
in vec2 texcoord;
uniform mat4 transform;
uniform mat4 modelView;
out vec3 viewPosition;
out vec3 viewNormal;
out vec3 crystalPosition;
out vec2 effectUv;

void main()
{
    crystalPosition = position;
    effectUv = texcoord;
    viewPosition = vec3(modelView * vec4(position, 1.0));
    viewNormal = mat3(modelView) * normal;
    gl_Position = transform * vec4(position, 1.0);
}
