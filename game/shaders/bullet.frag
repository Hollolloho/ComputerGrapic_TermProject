#version 330 core

in vec3 viewPosition;
in float facetU;
uniform vec3 color;
out vec4 fragColor;

void main()
{
    vec3 normal = normalize(cross(dFdx(viewPosition), dFdy(viewPosition)));
    vec3 light = normalize(vec3(-0.45, 0.75, 0.65));
    vec3 eye = normalize(-viewPosition);
    float diffuse = max(dot(normal, light), 0.0);
    float shine = pow(max(dot(reflect(-light, normal), eye), 0.0), 18.0);
    float rim = pow(1.0 - clamp(dot(normal, eye), 0.0, 1.0), 3.0);

    // 멀어져 모서리가 한 픽셀보다 작아지면 반사선도 함께 약해진다.
    float pixel = max(fwidth(facetU), 0.001);
    float edge = (1.0 - smoothstep(0.0, 0.10 + pixel, facetU)) * (0.10 / (0.10 + pixel));
    vec3 faceColor = mix(color * color * 0.45, color, 0.24 + 0.76 * diffuse);
    float highlight = clamp(0.90 * shine + 0.28 * rim + 0.50 * edge * diffuse, 0.0, 1.0);
    fragColor = vec4(mix(faceColor, vec3(1.0), highlight), 1.0);
}
