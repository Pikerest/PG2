#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNorm;
layout(location = 2) in vec2 aTex;

// Matrices 
uniform mat4 uM_m = mat4(1.0);
uniform mat4 uV_m = mat4(1.0);
uniform mat4 uP_m = mat4(1.0);

// Light properties - directional
uniform vec3 dir_light_direction;
uniform vec3 dir_light_ambient;
uniform vec3 dir_light_diffuse;
uniform vec3 dir_light_specular;

// Light properties - point lights (array)
#define MAX_POINT_LIGHTS 3
uniform vec3 point_light_position[MAX_POINT_LIGHTS];
uniform vec3 point_light_ambient[MAX_POINT_LIGHTS];
uniform vec3 point_light_diffuse[MAX_POINT_LIGHTS];
uniform vec3 point_light_specular[MAX_POINT_LIGHTS];
uniform int num_point_lights;

// Light properties - spotlight
uniform vec3 spot_light_position;
uniform vec3 spot_light_direction;
uniform vec3 spot_light_ambient;
uniform vec3 spot_light_diffuse;
uniform vec3 spot_light_specular;
uniform float spot_light_cutoff;
uniform float spot_light_outer_cutoff;

// Outputs to the fragment shader
out VS_OUT {
    vec3 fragPos;
    vec3 N;
    vec3 dir_L;
    vec3 point_L[MAX_POINT_LIGHTS];
    vec3 spot_L;
    vec3 V;
    vec2 texCoord;
} vs_out;

void main()
{
    // Create Model-View matrix
    mat4 mv_m = uV_m * uM_m;

    // Calculate view-space coordinate
    vec4 P = mv_m * vec4(aPos, 1.0f);

    // Calculate normal in view space
    vs_out.N = mat3(mv_m) * aNorm;
    
    // Directional light vector
    vs_out.dir_L = dir_light_direction;
    
    // Point lights vectors
    for (int i = 0; i < num_point_lights; i++) {
        vs_out.point_L[i] = point_light_position[i] - P.xyz;
    }
    
    // Spotlight vector
    vs_out.spot_L = spot_light_position - P.xyz;
    
    // Calculate view vector (negative of the view-space position)
    vs_out.V = -P.xyz;
    
    // Fragment position for spotlight cone calculation
    vs_out.fragPos = P.xyz;

    // Assign texture coordinates
    vs_out.texCoord = aTex;

    // Calculate the clip-space position of each vertex
    gl_Position = uP_m * P;
}
