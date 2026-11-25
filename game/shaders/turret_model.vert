#version 330 core

in vec3 position;
in vec3 normal;
in vec2 texcoord;
uniform mat4 uTransform;
uniform mat4 uModel;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;

void main()
{
    worldPosition = vec3(uModel * vec4(position, 1));
    worldNormal = mat3(uModel) * normal;
    uv = texcoord;
    gl_Position = uTransform * vec4(position, 1);
}
