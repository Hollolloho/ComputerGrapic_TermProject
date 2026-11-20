#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
out vec4 aFrag;
uniform vec3 uEye;
uniform vec3 uBaseColor;
uniform vec3 uEmission;
uniform float uMetallic;
uniform float uRoughness;
uniform float uOpacity;
uniform int uLayer;
uniform float uPower;

const float Pi = 3.14159265;

float LinearToSrgb(float value)
{
    return value <= .0031308 ? 12.92 * value : 1.055 * pow(value, 1.0 / 2.4) - .055;
}

vec3 DisplayColor(vec3 color)
{
    color = color / (vec3(1) + color);
    return vec3(LinearToSrgb(color.r), LinearToSrgb(color.g), LinearToSrgb(color.b));
}

void main()
{
    vec3 N = normalize(worldNormal);
    vec3 V = normalize(uEye - worldPosition);
    if (!gl_FrontFacing) N = -N;
    if (uLayer == 2) {
        // MTL의 투명도를 그대로 사용해 내부 격자와 서버를 보이게 한다.
        float edge = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 3.0);
        vec3 color = uBaseColor * (.6 + .4 * uPower) + vec3(.12 * edge);
        aFrag = vec4(DisplayColor(color), uOpacity);
        return;
    }
    if (uLayer == 1 || uLayer == 3) {
        vec3 color = uLayer == 1 ? 1.5 * uBaseColor + 1.4 * uEmission : 2.0 * uBaseColor;
        aFrag = vec4(DisplayColor(color * uPower), 1);
        return;
    }

    vec3 L = normalize(vec3(.38, .84, .48));
    float nl = max(dot(N, L), 0);
    float nv = max(dot(N, V), .001);
    vec3 color = uBaseColor * .4;
    if (nl > 0) {
        vec3 H = normalize(L + V);
        float nh = clamp(dot(N, H), 0, 1);
        float vh = clamp(dot(V, H), 0, 1);
        float a2 = pow(clamp(uRoughness, .04, 1), 4);
        float denominator = 1.0 - nh * nh + a2 * nh * nh;
        float distribution = a2 / (Pi * denominator * denominator);
        float visibility = 1.0 / ((nl + sqrt(a2 + (1.0 - a2) * nl * nl)) *
                                  (nv + sqrt(a2 + (1.0 - a2) * nv * nv)));
        vec3 f0 = mix(vec3(.04), uBaseColor, uMetallic);
        vec3 fresnel = f0 + (vec3(1) - f0) * pow(1.0 - vh, 5);
        vec3 diffuse = (vec3(1) - fresnel) * (1.0 - uMetallic) * uBaseColor / Pi;
        color += (diffuse + distribution * visibility * fresnel) * nl * 3.5;
    }
    aFrag = vec4(DisplayColor(color + 1.4 * uPower * uEmission), 1);
}
