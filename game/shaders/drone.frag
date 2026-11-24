#version 330 core

in vec3 droneNormal;
in vec3 worldPosition;
uniform vec3 baseColor;
uniform vec3 emissionColor;
uniform float opacity;
uniform bool drawBeam;
uniform vec3 eyePosition;
uniform float beamFadeDistance;
out vec4 fragColor;

void main()
{
    if (drawBeam)
    {
        if (!gl_FrontFacing)
            discard;
        float nearFade = smoothstep(beamFadeDistance * (2.0 / 3.0), beamFadeDistance * 1.35,
                                   distance(worldPosition, eyePosition));
        fragColor = vec4(baseColor, opacity * nearFade);
        return;
    }

    vec3 lightDirection = normalize(vec3(-0.35, 0.9, 0.4));
    float light = 0.55 + 0.45 * max(dot(normalize(droneNormal), lightDirection), 0.0);
    vec3 color = clamp(baseColor * light + emissionColor, 0.0, 1.0);
    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), opacity);
}
