#version 330 core

in vec3 position;
in vec3 normal;
uniform mat4 uViewProjection;
uniform mat4 uModel;
out vec3 worldPosition;
out vec3 worldNormal;

void main()
{
    vec4 world = uModel * vec4(position, 1.0);
    worldPosition = world.xyz;
    worldNormal = mat3(transpose(inverse(uModel))) * normal;
    gl_Position = uViewProjection * world;
}
