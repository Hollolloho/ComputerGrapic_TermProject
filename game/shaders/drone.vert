#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uTransform;
uniform mat4 uModel;
out vec3 worldPosition;
out vec3 worldNormal;

void main()
{
    gl_Position = uTransform * vec4(aPosition, 1.0);
    worldPosition = vec3(uModel * vec4(aPosition, 1.0));
    // 부품 변환은 회전과 이동이므로 법선에는 회전만 적용한다.
    worldNormal = mat3(uModel) * aNormal;
}
