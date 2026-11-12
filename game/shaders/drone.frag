#version 330 core

in vec3 normal;
uniform vec3 uColor;
out vec4 aFrag;

void main()
{
    vec3 lightDirection = normalize(vec3(-0.4, 0.9, 0.7));
    float diffuse = max(dot(normalize(normal), lightDirection), 0.0);
    aFrag = vec4(uColor * (0.55 + 0.45 * diffuse), 1.0);
}
