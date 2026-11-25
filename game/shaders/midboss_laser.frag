#version 330 core

in vec3 beamPoint;
uniform vec3 beamEye;
uniform vec3 beamSize;
uniform vec3 beamColor;
uniform float strength;
out vec4 fragColor;

void main()
{
    if (!gl_FrontFacing)
        discard;
    vec3 ray = beamPoint - beamEye;
    float t = max(-dot(beamPoint.xy, ray.xy) / max(dot(ray.xy, ray.xy), 1e-8), 0.0);
    if (ray.z > 0.0)
        t = min(t, (1.0 - beamPoint.z) / ray.z);
    else if (ray.z < 0.0)
        t = min(t, -beamPoint.z / ray.z);
    vec2 across = abs(beamPoint.xy + t * ray.xy);
    float distanceFromAxis = max(across.x, across.y);
    float core = 1.0 - smoothstep(0.28, 0.58, distanceFromAxis);
    float glow = 1.0 - smoothstep(0.42, 1.0, distanceFromAxis);
    float nearFade = smoothstep(0.14, 0.55, length(ray * beamSize));
    vec3 color = mix(beamColor, vec3(1.0, 0.96, 0.79), core);
    fragColor = vec4(color, max(0.64 * glow, core) * strength * nearFade);
}
