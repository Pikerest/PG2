#version 460 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2DMS uAccumTexture;
uniform sampler2DMS uRevealTexture;
uniform int uSampleCount = 4;

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 accum = vec4(0.0);
    float reveal = 0.0;
    for (int i = 0; i < uSampleCount; ++i) {
        accum += texelFetch(uAccumTexture, pixel, i);
        reveal += texelFetch(uRevealTexture, pixel, i).r;
    }
    accum /= float(uSampleCount);
    reveal /= float(uSampleCount);

    if (reveal >= 0.999) {
        discard;
    }

    vec3 color = accum.rgb / max(accum.a, 1.0e-5);
    float alpha = clamp(1.0 - reveal, 0.0, 1.0);
    FragColor = vec4(color, alpha);
}
