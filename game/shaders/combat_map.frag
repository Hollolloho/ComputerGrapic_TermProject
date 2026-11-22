#version 330 core

in vec3 worldPos;
in vec3 mapNormal;

uniform vec3 baseColor;
uniform vec3 emissionColor;
uniform vec3 specularColor;
uniform float shininess;
uniform vec3 doorLightPos;
uniform vec3 emergencyLightPos[32];
uniform int emergencyLightCount;
uniform vec3 eyePos;

out vec4 fragColor;

vec3 PointLight(vec3 N, vec3 lightPos, vec3 lightColor, float radius)
{
    vec3 toLight = lightPos - worldPos;
    float distanceSquared = dot(toLight, toLight);
    if (distanceSquared >= radius * radius)
        return vec3(0.0);

    float d = sqrt(distanceSquared);
    vec3 L = toLight / max(d, 0.001);
    float diffuse = max(dot(N, L), 0.0);
    if (diffuse <= 0.0)
        return vec3(0.0);

    vec3 V = normalize(eyePos - worldPos);
    float specular = pow(max(dot(reflect(-L, N), V), 0.0), max(shininess, 1.0));
    float attenuation = 1.0 / (1.0 + 0.20 * d + 0.14 * d * d);
    attenuation *= 1.0 - smoothstep(radius - 2.0, radius, d);
    return (baseColor * diffuse + 0.12 * specularColor * specular) * lightColor * attenuation;
}

void main()
{
    vec3 N = normalize(mapNormal);
    vec3 lightDirection = normalize(vec3(-0.35, 0.9, 0.4));
    float light = 0.55 + 0.45 * max(dot(N, lightDirection), 0.0);
    vec3 doorLight = PointLight(N, doorLightPos, vec3(0.65, 0.80, 0.87), 5.5);
    vec3 emergencyLight = vec3(0.0);
    for (int i = 0; i < emergencyLightCount; ++i)
        emergencyLight += PointLight(N, emergencyLightPos[i], vec3(1.65, 0.045, 0.025), 4.8);
    vec3 color = clamp(baseColor * light + emissionColor + doorLight + emergencyLight, 0.0, 1.0);
    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
