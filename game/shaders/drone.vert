#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uTransform;
out vec3 normal;

void main()
{
    gl_Position = uTransform * vec4(aPosition, 1.0);
    // 정적 부품은 평행 이동만 하므로 OBJ의 법선 방향이 유지된다.
    normal = aNormal;
}
