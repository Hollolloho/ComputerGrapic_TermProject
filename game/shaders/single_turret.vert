#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uTransform;
uniform mat4 uModel;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;

void main()
{
    worldPosition = vec3(uModel * vec4(aPosition, 1));
    worldNormal = mat3(uModel) * aNormal;
    uv = aUV;
    gl_Position = uTransform * vec4(aPosition, 1);
}
