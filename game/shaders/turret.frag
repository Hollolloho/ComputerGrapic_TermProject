#version 330 core
in vec3 localPosition;
uniform vec3 baseColor;
out vec4 fragColor;
void main() {
    vec3 normal=normalize(cross(dFdx(localPosition),dFdy(localPosition)));
    float light=0.65+0.35*abs(dot(normal,normalize(vec3(0.4,0.8,0.5))));
    fragColor=vec4(pow(baseColor*light,vec3(1.0/2.2)),1);
}
