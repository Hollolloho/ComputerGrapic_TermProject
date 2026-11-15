#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
out vec4 aFrag;

uniform vec3 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEyePosition;
uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightStrength;

const float Pi = 3.14159265;

vec3 NormalizeDirection(vec3 direction)
{
    return direction * inversesqrt(max(dot(direction, direction), 1e-8));
}

// 거칠기에 따라 미세면 법선이 반각 벡터 쪽으로 얼마나 모이는지 계산한다.
float DistributionGGX(float nDotH, float alphaSquared)
{
    float nDotHSquared = nDotH * nDotH;
    float denominator = (1.0 - nDotHSquared) + alphaSquared * nDotHSquared;
    return alphaSquared / (Pi * denominator * denominator);
}

// Smith GGX 가림 항 G를 정반사 분모 4(N·L)(N·V)와 약분한 값이다.
float VisibilitySmithGGX(float nDotL, float nDotV, float alphaSquared)
{
    float light = nDotL + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotL * nDotL);
    float view = nDotV + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotV * nDotV);
    return 1.0 / (light * view);
}

// 보는 방향이 표면에 가까워질수록 커지는 정반사 비율을 구한다.
vec3 FresnelSchlick(float vDotH, vec3 f0)
{
    return f0 + (vec3(1.0) - f0) * pow(1.0 - vDotH, 5.0);
}

// 톤 매핑한 선형 색을 화면 출력용 sRGB로 바꾼다.
float LinearToSrgb(float channel)
{
    return channel <= 0.0031308 ? 12.92 * channel : 1.055 * pow(channel, 1.0 / 2.4) - 0.055;
}

void main()
{
    vec3 N = NormalizeDirection(worldNormal);
    vec3 V = NormalizeDirection(uEyePosition - worldPosition);
    vec3 L = NormalizeDirection(uLightDirection);
    float nDotL = max(dot(N, L), 0.0);
    float nDotV = max(dot(N, V), 0.0);

    float metallic = clamp(uMetallic, 0.0, 1.0);
    float roughness = clamp(uRoughness, 0.04, 1.0);
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    vec3 f0 = mix(vec3(0.04), uBaseColor, metallic);

    // 직접광이 닿지 않는 부분의 색이 보이도록 약한 주변광을 더한다.
    vec3 color = 0.16 * mix(uBaseColor, f0, metallic);
    if (nDotL > 0.0 && nDotV > 0.0) {
        vec3 H = NormalizeDirection(L + V);
        float nDotH = clamp(dot(N, H), 0.0, 1.0);
        float vDotH = clamp(dot(V, H), 0.0, 1.0);
        vec3 fresnel = FresnelSchlick(vDotH, f0);
        float distribution = DistributionGGX(nDotH, alphaSquared);
        float visibility = VisibilitySmithGGX(nDotL, nDotV, alphaSquared);
        vec3 specular = distribution * visibility * fresnel;
        vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * uBaseColor / Pi;
        color += (diffuse + specular) * uLightColor * uLightStrength * nDotL;
    }

    vec3 mapped = color / (vec3(1.0) + color);
    aFrag = vec4(LinearToSrgb(mapped.r), LinearToSrgb(mapped.g), LinearToSrgb(mapped.b), 1.0);
}
