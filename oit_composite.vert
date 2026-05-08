#version 460 core

out vec2 vTexCoord;

void main()
{
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 p = positions[gl_VertexID];
    vTexCoord = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
