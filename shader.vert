#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNorm;
layout(location = 2) in vec2 aTex;

// Matrices
uniform mat4 uM_m = mat4(1.0);
uniform mat4 uV_m = mat4(1.0);
uniform mat4 uP_m = mat4(1.0);
uniform mat3 uNormal_m = mat3(1.0);
uniform vec2 u_tex_scale = vec2(1.0);
uniform int u_swap_tex_axes = 0;

// Outputs to the fragment shader
// Point/spot/dir light vectors removed — computed per-fragment from fragPos + uniforms
// This eliminates MAX_POINT_LIGHTS*3 + 6 interpolated floats (was 54 extra varyings).
out VS_OUT {
    vec3 fragPos;
    vec3 N;
    vec3 V;
    vec2 texCoord;
} vs_out;

void main()
{
    mat4 mv_m = uV_m * uM_m;
    vec4 P = mv_m * vec4(aPos, 1.0f);

    vs_out.N        = mat3(uV_m) * uNormal_m * aNorm;
    vs_out.V        = -P.xyz;
    vs_out.fragPos  = P.xyz;

    vec2 tex = u_swap_tex_axes == 1 ? aTex.yx : aTex;
    vs_out.texCoord = tex * u_tex_scale;

    gl_Position = uP_m * P;
}
