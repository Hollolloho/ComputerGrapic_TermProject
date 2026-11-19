#version 330 core
in vec3 eyePosition;
in vec3 eyeNormal;
in vec2 uv;
out vec4 outColor;
uniform sampler2D basecolorMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D emissionMap;
uniform float shot;
uniform float emissionMultiplier;
const float PI=3.14159265;

vec3 SurfaceNormal() {
    vec3 N=normalize(eyeNormal);
    vec3 px=dFdx(eyePosition),py=dFdy(eyePosition);
    vec2 ux=dFdx(uv),uy=dFdy(uv);
    float det=ux.x*uy.y-ux.y*uy.x;
    if(abs(det)<1e-8) return N;
    vec3 rawT=(px*uy.y-py*ux.y)/det;
    vec3 rawB=(-px*uy.x+py*ux.x)/det;
    vec3 tangent=rawT-N*dot(N,rawT);
    if(dot(tangent,tangent)<1e-10) return N;
    vec3 T=normalize(tangent);
    vec3 B=normalize(cross(N,T));
    B*=dot(B,rawB)<0 ? -1.0 : 1.0;
    return normalize(mat3(T,B,N)*(texture(normalMap,uv).xyz*2.0-1.0));
}

vec3 ToSrgb(vec3 value) {
    return mix(12.92*value,1.055*pow(max(value,vec3(0)),vec3(1.0/2.4))-0.055,
        step(vec3(0.0031308),value));
}

void main() {
    vec3 base=texture(basecolorMap,uv).rgb; // GPU decodes sRGB basecolor.
    float metal=clamp(texture(metallicMap,uv).r,0,1);
    float rough=clamp(texture(roughnessMap,uv).r,0.08,1);
    vec3 N=SurfaceNormal(),V=normalize(-eyePosition);
    vec3 L=normalize(vec3(-0.4,0.8,0.6)),H=normalize(L+V);
    float nl=max(dot(N,L),0),nv=max(dot(N,V),0.001),nh=max(dot(N,H),0);
    float alpha=rough*rough,a2=alpha*alpha;
    float d=nh*nh*(a2-1)+1;
    float distribution=a2/max(PI*d*d,1e-6);
    float k=(rough+1)*(rough+1)/8;
    float geometry=(nl/(nl*(1-k)+k))*(nv/(nv*(1-k)+k));
    vec3 f0=mix(vec3(0.04),base,metal);
    vec3 fresnel=f0+(1-f0)*pow(1-max(dot(V,H),0),5);
    vec3 specular=distribution*geometry*fresnel/max(4*nl*nv,0.001);
    vec3 color=base*0.2+((1-fresnel)*(1-metal)*base/PI+specular)*nl*3.5;
    color+=texture(emissionMap,uv).rgb*emissionMultiplier*(1+shot*1.5);
    color=color/(1+color);
    outColor=vec4(ToSrgb(color),1);
}
