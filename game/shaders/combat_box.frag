#version 330 core

in vec3 localPosition;

uniform vec3 color;
uniform float opacity;
out vec4 outColor;

void main()
{
    vec3 normal = normalize(cross(dFdx(localPosition), dFdy(localPosition)));
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.5));
    float light = 0.65 + 0.35 * abs(dot(normal, lightDirection));
    outColor = vec4(color * light, opacity);
}
