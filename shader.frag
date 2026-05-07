#version 460 core

// Outputs colors in RGBA
out vec4 FragColor;

// Material properties
uniform vec3 ambient_material = vec3(1.0, 1.0, 1.0);
uniform vec3 diffuse_material = vec3(1.0, 1.0, 1.0);
uniform vec3 specular_material = vec3(1.0, 1.0, 1.0);
uniform float specular_shininess = 32.0;
uniform vec3 u_emissive = vec3(0.0);
uniform bool u_debug_collision = false;
uniform vec4 u_debug_color = vec4(0.05, 0.95, 1.0, 0.72);

// Material transparency (Task 1: Transparency)
// 1.0 = fully opaque, 0.0 = fully transparent
uniform float u_material_alpha = 1.0;

// Active texture unit
uniform sampler2D uTexture;

// Directional light
uniform vec3 dir_light_ambient;
uniform vec3 dir_light_diffuse;
uniform vec3 dir_light_specular;

// Point lights
#define MAX_POINT_LIGHTS 16
uniform vec3 point_light_ambient[MAX_POINT_LIGHTS];
uniform vec3 point_light_diffuse[MAX_POINT_LIGHTS];
uniform vec3 point_light_specular[MAX_POINT_LIGHTS];
uniform float point_light_radius[MAX_POINT_LIGHTS];
uniform int num_point_lights;

// Spotlight
uniform vec3 spot_light_ambient;
uniform vec3 spot_light_diffuse;
uniform vec3 spot_light_specular;
uniform vec3 spot_light_direction;
uniform float spot_light_cutoff;
uniform float spot_light_outer_cutoff;

// Input from vertex shader
in VS_OUT {
    vec3 fragPos;
    vec3 N;
    vec3 dir_L;
    vec3 point_L[MAX_POINT_LIGHTS];
    vec3 spot_L;
    vec3 V;
    vec2 texCoord;
} fs_in;

vec3 calculatePointLight(int i, vec3 N, vec3 V) {
    float dist = length(fs_in.point_L[i]);
    float attenuation = pow(clamp(1.0 - dist / max(point_light_radius[i], 0.001), 0.0, 1.0), 2.0);
    vec3 L = normalize(fs_in.point_L[i]);
    vec3 R = reflect(-L, N);
    
    vec3 ambient = point_light_ambient[i] * ambient_material;
    vec3 diffuse = max(dot(N, L), 0.0) * point_light_diffuse[i] * diffuse_material;
    vec3 specular = pow(max(dot(R, V), 0.0), specular_shininess) * 
                    point_light_specular[i] * specular_material;
    
    return (ambient + diffuse + specular) * attenuation;
}

vec3 calculateSpotLight(vec3 N, vec3 V) {
    vec3 L = normalize(fs_in.spot_L);
    vec3 spotDir = normalize(spot_light_direction);
    
    float theta = dot(L, -spotDir);
    float epsilon = cos(radians(spot_light_cutoff)) - cos(radians(spot_light_outer_cutoff));
    float intensity = clamp((theta - cos(radians(spot_light_outer_cutoff))) / epsilon, 0.0, 1.0);
    
    vec3 R = reflect(-L, N);
    
    vec3 ambient = spot_light_ambient * ambient_material * intensity;
    vec3 diffuse = max(dot(N, L), 0.0) * spot_light_diffuse * diffuse_material * intensity;
    vec3 specular = pow(max(dot(R, V), 0.0), specular_shininess) * 
                    spot_light_specular * specular_material * intensity;
    
    return (ambient + diffuse + specular);
}

void main()
{
    if (u_debug_collision) {
        FragColor = u_debug_color;
        return;
    }

    // Normalize vectors
    vec3 N = normalize(fs_in.N);
    vec3 V = normalize(fs_in.V);

    // Sample texture
    vec4 texColor = texture(uTexture, fs_in.texCoord);

    // Directional light contribution
    vec3 dir_L = normalize(fs_in.dir_L);
    vec3 dir_R = reflect(-dir_L, N);
    vec3 dir_ambient = dir_light_ambient * ambient_material;
    vec3 dir_diffuse = max(dot(N, dir_L), 0.0) * dir_light_diffuse * diffuse_material;
    vec3 dir_specular = pow(max(dot(dir_R, V), 0.0), specular_shininess) * 
                        dir_light_specular * specular_material;
    vec3 dirLight = dir_ambient + dir_diffuse + dir_specular;

    // Point lights contribution
    vec3 pointLight = vec3(0.0);
    for (int i = 0; i < num_point_lights; i++) {
        pointLight += calculatePointLight(i, N, V);
    }

    // Spotlight contribution
    vec3 spotLight = calculateSpotLight(N, V);

    // Combine all lighting
    vec3 lighting = dirLight + pointLight + spotLight;
    
    // Apply to texture, including material alpha for transparency (Task 1)
    FragColor = vec4(lighting * texColor.rgb + u_emissive, texColor.a * u_material_alpha);
}
