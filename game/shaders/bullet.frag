#version 330 core

in vec3 localPosition;
uniform vec3 color;
out vec4 fragColor;

void main()
{
    vec3 normal = normalize(cross(dFdx(localPosition), dFdy(localPosition)));
    vec3 light = normalize(vec3(0.25, 1.0, 0.4));
    float facet = 0.5 + 0.5 * dot(normal, light);
    vec3 faceColor = color * (0.52 + 0.48 * facet);
    float highlight = smoothstep(0.52, 0.95, facet);
    fragColor = vec4(mix(faceColor, vec3(1.0), 0.65 * highlight), 1.0);
}
