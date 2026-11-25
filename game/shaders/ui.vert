#version 330 core

uniform mat4 clipTransform;
out vec2 uv;

void main()
{
    const vec2 corners[6] = vec2[6](
        vec2(-1, -1), vec2(1, -1), vec2(1, 1),
        vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
    vec2 position = corners[gl_VertexID];
    uv = vec2((position.x + 1.0) * 0.5, (1.0 - position.y) * 0.5);
    gl_Position = clipTransform * vec4(position, 0.0, 1.0);
}
