#version 330 core

in vec3 mapNormal;
uniform vec3 baseColor;
uniform vec3 emissionColor;
out vec4 fragColor;

void main()
{
    vec3 lightDirection = normalize(vec3(-0.35, 0.9, 0.4));
    float light = 0.55 + 0.45 * max(dot(normalize(mapNormal), lightDirection), 0.0);
    vec3 color = clamp(baseColor * light + emissionColor, 0.0, 1.0);
    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
