#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;
out vec4 aFrag;
uniform vec3 uEye;
uniform float uWarning;
uniform bool uTextured;
uniform vec3 uMaterialColor;
uniform sampler2D uBaseColorMap;
uniform sampler2D uNormalMap;
uniform sampler2D uMetallicMap;
uniform sampler2D uRoughnessMap;

const float Pi = 3.14159265;

// UV가 퇴화하면 메시 법선을 사용한다. 미러 UV의 접선 방향도 유지한다.
vec3 SurfaceNormal()
{
    vec3 N = normalize(worldNormal);
    vec3 dx = dFdx(worldPosition), dy = dFdy(worldPosition);
    vec2 tx = dFdx(uv), ty = dFdy(uv);
    float determinant = tx.x * ty.y - tx.y * ty.x;
    if (abs(determinant) <= 1e-4 * length(tx) * length(ty)) return N;
    vec3 T = (dx * ty.y - dy * tx.y) * sign(determinant);
    vec3 B = (dy * tx.x - dx * ty.x) * sign(determinant);
    T -= N * dot(N, T);
    if (dot(T, T) <= 0) return N;
    T = normalize(T);
    float handedness = sign(dot(cross(N, T), B));
    if (handedness == 0) return N;
    return normalize(mat3(T, cross(N, T) * handedness, N) *
                     (texture(uNormalMap, uv).xyz * 2.0 - 1.0));
}

float LinearToSrgb(float value)
{
    return value <= .0031308 ? 12.92 * value : 1.055 * pow(value, 1.0 / 2.4) - .055;
}

void main()
{
    vec3 N = uTextured ? SurfaceNormal() : normalize(worldNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(uEye - worldPosition);
    vec3 L = normalize(vec3(.45, .9, .7));
    vec3 baseColor = uTextured ? texture(uBaseColorMap, uv).rgb : uMaterialColor;
    float metallic = uTextured ? clamp(texture(uMetallicMap, uv).r, 0, 1) : 0.45;
    float roughness = uTextured ? clamp(texture(uRoughnessMap, uv).r, .04, 1) : 0.55;
    float nl = max(dot(N, L), 0);
    float nv = max(dot(N, V), .001);
    vec3 f0 = mix(vec3(.04), baseColor, metallic);
    vec3 color = .35 * mix(baseColor, f0, metallic);
    if (nl > 0) {
        vec3 H = normalize(L + V);
        float nh = clamp(dot(N, H), 0, 1);
        float vh = clamp(dot(V, H), 0, 1);
        float a2 = pow(roughness, 4);
        float denominator = 1.0 - nh * nh + a2 * nh * nh;
        float distribution = a2 / (Pi * denominator * denominator);
        float visibility = 1.0 / ((nl + sqrt(a2 + (1.0 - a2) * nl * nl)) *
                                  (nv + sqrt(a2 + (1.0 - a2) * nv * nv)));
        vec3 fresnel = f0 + (vec3(1) - f0) * pow(1.0 - vh, 5);
        vec3 diffuse = (vec3(1) - fresnel) * (1.0 - metallic) * baseColor / Pi;
        color += (diffuse + distribution * visibility * fresnel) * nl * 3.5;
    }
    // 적색 센서만 조준 경고 중 밝게 만든다.
    float redSensor = step(baseColor.g * 2.0 + .03, baseColor.r);
    color += baseColor * redSensor * uWarning * 2.0;
    color = color / (vec3(1) + color);
    aFrag = vec4(LinearToSrgb(color.r), LinearToSrgb(color.g), LinearToSrgb(color.b), 1);
}
