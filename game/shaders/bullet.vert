#version 330 core

layout(location = 0) in vec4 vertex;
uniform mat4 transform;
uniform mat4 modelView;
out vec3 viewPosition;
out float facetU;

void main()
{
    vec4 position = vec4(vertex.xyz, 1.0);
    viewPosition = vec3(modelView * position);
    facetU = vertex.w;
    gl_Position = transform * position;
}
