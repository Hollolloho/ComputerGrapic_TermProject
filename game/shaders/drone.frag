#version 330 core

in vec3 droneNormal;
in vec3 worldPosition;
in vec3 beamPosition;
uniform vec3 baseColor;
uniform vec3 emissionColor;
uniform float opacity;
uniform bool drawBeam;
uniform vec3 eyePosition;
uniform vec3 beamEye;
out vec4 fragColor;

float BeamRadius(out float along)
{
    // 카메라 광선 위에서 조준선에 가장 가까운 점을 찾는다. 끝면 밖으로는 가지 않는다.
    vec3 ray = beamPosition - beamEye;
    float t = max(-dot(beamPosition.xy, ray.xy) / max(dot(ray.xy, ray.xy), 1e-8), 0.0);
    if (ray.z > 0.0)
        t = min(t, (1.0 - beamPosition.z) / ray.z);
    else if (ray.z < 0.0)
        t = min(t, -beamPosition.z / ray.z);
    along = clamp(beamPosition.z + t * ray.z, 0.0, 1.0);
    return length(beamPosition.xy + t * ray.xy);
}

void main()
{
    if (drawBeam)
    {
        float along;
        float radius = BeamRadius(along);
        // 먼 거리에서 한 픽셀보다 가는 중심선도 이어지도록 픽셀 폭을 반영한다.
        float antialias = clamp(0.5 * fwidth(radius), 0.10, 0.35);
        if (!gl_FrontFacing)
            discard;
        float core = 1.0 - smoothstep(0.26 - antialias, 0.26 + antialias, radius);
        float glow = 1.0 - smoothstep(0.08, 1.0, radius);
        float tip = smoothstep(0.97, 1.0, along) * (1.0 - smoothstep(0.18, 0.67, radius));
        float nearFade = smoothstep(0.16, 0.42, distance(worldPosition, eyePosition));
        vec3 color = mix(vec3(1.0, 0.13, 0.085), vec3(1.0, 0.96, 0.93), max(core, 0.85 * tip));
        float alpha = max(mix(0.34 * glow, 0.98, core), 0.88 * tip);
        fragColor = vec4(color, alpha * nearFade);
        return;
    }

    vec3 lightDirection = normalize(vec3(-0.35, 0.9, 0.4));
    float light = 0.55 + 0.45 * max(dot(normalize(droneNormal), lightDirection), 0.0);
    vec3 color = clamp(baseColor * light + emissionColor, 0.0, 1.0);
    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), opacity);
}
