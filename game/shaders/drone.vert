#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uTransform;
uniform mat4 uModel;
out vec3 normal;

void main()
{
    gl_Position = uTransform * vec4(aPosition, 1.0);
    // 조명에 사용하는 월드 법선도 부품과 함께 회전한다.
    normal = mat3(uModel) * aNormal;
}
