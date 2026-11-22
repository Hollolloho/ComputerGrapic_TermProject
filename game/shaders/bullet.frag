#version 330 core

in vec3 viewPosition;
in vec3 viewNormal;
in vec3 crystalPosition;
uniform vec3 color;
out vec4 fragColor;

float segmentDistance(vec2 p, vec2 start, vec2 end)
{
    vec2 segment = end - start;
    float t = clamp(dot(p - start, segment) / dot(segment, segment), 0.0, 1.0);
    return length(p - (start + segment * t));
}

// 주 균열은 두 번 꺾이고, 꺾인 자리에서 짧고 가는 가지가 갈라진다.
float fracture(vec2 p)
{
    vec2 a = vec2(-0.32, 0.98), b = vec2(0.08, 0.34), c = vec2(-0.04, 0.16);
    vec2 d = vec2(0.33, -0.12), e = vec2(0.24, -0.37);
    float line = min(segmentDistance(p, a, b), segmentDistance(p, b, c));
    line = min(line, min(segmentDistance(p, c, d), segmentDistance(p, d, e)));
    line = min(line, segmentDistance(p, e, vec2(0.58, -0.88)));
    float branch = min(segmentDistance(p, c, vec2(-0.33, 0.20)),
                       segmentDistance(p, vec2(-0.33, 0.20), vec2(-0.53, 0.04)));
    branch = min(branch, segmentDistance(p, d, vec2(0.70, 0.03)));
    return min(line, 1.7 * branch);
}

float crackLine(float distanceToCrack, float width, float pixel)
{
    return clamp((width + 0.5 * pixel - distanceToCrack) / pixel, 0.0, min(1.0, 2.0 * width / pixel));
}

void main()
{
    vec3 surface = normalize(viewNormal);
    vec3 facet = normalize(cross(dFdx(viewPosition), dFdy(viewPosition)));
    vec3 normal = normalize(mix(surface, facet, 0.34));
    vec3 eye = normalize(-viewPosition);
    vec3 light = normalize(vec3(-0.45, 0.75, 0.65));
    float diffuse = max(dot(normal, light), 0.0);
    float shine = pow(max(dot(reflect(-light, normal), eye), 0.0), 22.0);
    float rim = pow(1.0 - clamp(dot(normal, eye), 0.0, 1.0), 3.0);
    // 몸통과 밑면에 새겨진 균열은 탄이 이동하거나 카메라가 돌아도 표면에 고정된다.
    vec3 p = crystalPosition;
    float rear = 1.0 - smoothstep(-1.40, -1.23, p.z);
    vec2 pattern = mix(vec2(p.z - 0.25 + 0.15 * p.x, p.y + 0.12 * p.x),
                       vec2(p.x + 0.16, p.y), rear);
    float pixel = max(max(fwidth(pattern.x), fwidth(pattern.y)), 0.001);
    float glint = crackLine(fracture(pattern), 0.014, pixel);
    float shadow = crackLine(fracture(pattern + vec2(0.025, 0.009)), 0.024, pixel);
    float innerPlane = dot(p, vec3(0.72, -0.60, 0.39));
    float depth = smoothstep(-0.18, 0.16, innerPlane);
    vec3 glass = mix(color * color * 0.38, color, 0.20 + 0.65 * diffuse + 0.15 * depth);
    glass *= 1.0 - 0.58 * shadow;
    float reflection = clamp(0.92 * shine + 0.34 * rim + glint * (0.58 + 0.34 * diffuse), 0.0, 1.0);
    fragColor = vec4(mix(glass, vec3(1.0), reflection), 1.0);
}
