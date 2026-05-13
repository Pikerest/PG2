#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 Revealage;

// Material properties
uniform vec3  ambient_material    = vec3(1.0);
uniform vec3  diffuse_material    = vec3(1.0);
uniform vec3  specular_material   = vec3(1.0);
uniform float specular_shininess  = 32.0;
uniform vec3  u_emissive          = vec3(0.0);
uniform bool  u_debug_collision   = false;
uniform vec4  u_debug_color       = vec4(0.05, 0.95, 1.0, 0.72);
uniform bool  u_oit_pass          = false;
uniform float u_material_alpha    = 1.0;
uniform bool  u_two_sided         = false;

uniform sampler2D uTexture;

// Directional light
uniform vec3 dir_light_direction;
uniform vec3 dir_light_ambient;
uniform vec3 dir_light_diffuse;
uniform vec3 dir_light_specular;

// Point lights
#define MAX_POINT_LIGHTS 24
uniform vec3  point_light_position[MAX_POINT_LIGHTS];
uniform vec3  point_light_ambient [MAX_POINT_LIGHTS];
uniform vec3  point_light_diffuse [MAX_POINT_LIGHTS];
uniform vec3  point_light_specular[MAX_POINT_LIGHTS];
uniform float point_light_radius  [MAX_POINT_LIGHTS];
uniform int   num_point_lights;

// Spotlight
uniform vec3  spot_light_position;
uniform vec3  spot_light_direction;
uniform vec3  spot_light_ambient;
uniform vec3  spot_light_diffuse;
uniform vec3  spot_light_specular;
uniform float spot_light_cos_cutoff       = 0.99;
uniform float spot_light_cos_outer_cutoff = 0.97;
uniform bool  u_spot_active               = false;

// Input from vertex shader (lean: no light-vector arrays)
in VS_OUT {
    vec3 fragPos;
    vec3 N;
    vec3 V;
    vec2 texCoord;
} fs_in;

vec3 calculatePointLight(int i, vec3 N, vec3 V) {
    // Compute light vector per-fragment (avoids 48 interpolated varyings in VS_OUT)
    vec3  pl    = point_light_position[i] - fs_in.fragPos;
    float r     = max(point_light_radius[i], 0.001);
    float dist2 = dot(pl, pl);
    // Early exit using squared distance — no sqrt needed for the reject case
    if (dist2 > r * r) return vec3(0.0);

    float dist        = sqrt(dist2);
    float attenuation = pow(clamp(1.0 - dist / r, 0.0, 1.0), 2.0);
    vec3  L = normalize(pl);
    vec3  R = reflect(-L, N);

    float NdotL   = u_two_sided ? abs(dot(N, L)) : max(dot(N, L), 0.0);
    vec3 ambient  = point_light_ambient[i]  * ambient_material;
    vec3 diffuse  = NdotL * point_light_diffuse[i] * diffuse_material;
    vec3 specular = pow(max(dot(R, V), 0.0), specular_shininess) *
                    point_light_specular[i] * specular_material;

    return (ambient + diffuse + specular) * attenuation;
}

vec3 calculateSpotLight(vec3 N, vec3 V) {
    vec3  sl      = spot_light_position - fs_in.fragPos;
    vec3  L       = normalize(sl);
    vec3  spotDir = normalize(spot_light_direction);

    float theta   = dot(L, -spotDir);
    float epsilon = spot_light_cos_cutoff - spot_light_cos_outer_cutoff;
    float intensity = clamp((theta - spot_light_cos_outer_cutoff) / epsilon, 0.0, 1.0);

    vec3 R = reflect(-L, N);

    vec3 ambient  = spot_light_ambient  * ambient_material  * intensity;
    vec3 diffuse  = max(dot(N, L), 0.0) * spot_light_diffuse  * diffuse_material  * intensity;
    vec3 specular = pow(max(dot(R, V), 0.0), specular_shininess) *
                    spot_light_specular * specular_material * intensity;

    return ambient + diffuse + specular;
}

void main()
{
    if (u_debug_collision) {
        FragColor = u_debug_color;
        return;
    }

    vec3 N = normalize(fs_in.N);
    vec3 V = normalize(fs_in.V);

    vec4 texColor = texture(uTexture, fs_in.texCoord);

    // Directional light — direction is already view-space & normalized on CPU
    vec3 dir_L   = dir_light_direction;
    vec3 dir_R   = reflect(-dir_L, N);
    vec3 dirLight = dir_light_ambient * ambient_material
                  + (u_two_sided ? abs(dot(N, dir_L)) : max(dot(N, dir_L), 0.0)) * dir_light_diffuse * diffuse_material
                  + pow(max(dot(dir_R, V), 0.0), specular_shininess) * dir_light_specular * specular_material;

    vec3 pointLight = vec3(0.0);
    for (int i = 0; i < num_point_lights; i++)
        pointLight += calculatePointLight(i, N, V);

    vec3 spotLight = u_spot_active ? calculateSpotLight(N, V) : vec3(0.0);

    vec3 lighting = dirLight + pointLight + spotLight;
    vec4 shaded   = vec4(lighting * texColor.rgb + u_emissive, texColor.a * u_material_alpha);

    if (u_oit_pass) {
        float alpha  = clamp(shaded.a, 0.0, 1.0);
        float weight = clamp(pow(min(1.0, alpha * 8.0) + 0.01, 3.0) * 1.0e3, 1.0e-2, 3.0e3);
        FragColor  = vec4(shaded.rgb * alpha, alpha) * weight;
        Revealage  = vec4(alpha);
        return;
    }

    FragColor = shaded;
    Revealage = vec4(1.0);
}
