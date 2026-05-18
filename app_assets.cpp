/*
 * Scene and asset construction.
 * Loads shaders, textures, models, lights, enemies, triggers, colliders,
 * and runtime helper registries used by gameplay and rendering.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "app.hpp"
#include "Mesh.hpp"
#include "Texture.hpp"

namespace {
constexpr size_t MAX_PARTICLES = 128;
}

// Builds every persistent resource needed by the level.
// This intentionally happens once at startup; runtime reset_game_world()
// only restores gameplay state without reloading meshes/textures.
void App::init_assets(void) {
    // Shader programs and fixed texture units used by regular rendering
    // and the weighted blended transparency composite pass.
    shader_prog = ShaderProgram::from_files("shader.vert", "shader.frag");
    oit_composite_prog = ShaderProgram::from_files("oit_composite.vert", "oit_composite.frag");

    shader_prog->use();
    shader_prog->setUniform("uTexture", 0);
    shader_prog->setUniform("u_oit_pass", 0);

    oit_composite_prog->use();
    oit_composite_prog->setUniform("uAccumTexture", 0);
    oit_composite_prog->setUniform("uRevealTexture", 1);
    oit_composite_prog->setUniform("uSampleCount", 4);

    if (fullscreen_vao == 0) {
        glCreateVertexArrays(1, &fullscreen_vao);
    }
    setup_oit_buffers(width, height);

    // Static light layout. Dynamic intensity/flicker is updated later in
    // update_gameplay(), but positions and baseline colors are declared here.
    dir_light.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.2f));
    dir_light.ambient = glm::vec3(0.21f, 0.21f, 0.21f);
    dir_light.diffuse = glm::vec3(0.14f, 0.14f, 0.15f);
    dir_light.specular = glm::vec3(0.18f, 0.20f, 0.22f);

    point_lights.clear();
    point_lights.push_back({ glm::vec3(  0.0f, 14.6f, -12.5f), glm::vec3(0.040f, 0.052f, 0.050f), glm::vec3(1.05f, 1.45f, 1.30f), glm::vec3(0.14f, 0.22f, 0.20f), 52.0f });
    point_lights.push_back({ glm::vec3(-21.5f, 4.8f, -12.5f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f });
    point_lights.push_back({ glm::vec3( 21.5f, 4.8f, -12.5f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f });
    point_lights.push_back({ glm::vec3(  0.0f, 4.8f,   9.0f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f });
    point_lights.push_back({ glm::vec3(  0.0f, 4.8f, -34.0f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f });
    point_lights.push_back({ glm::vec3(  0.0f, 3.7f, -12.5f), glm::vec3(0.016f, 0.032f, 0.030f), glm::vec3(0.56f, 1.40f, 1.24f), glm::vec3(0.04f, 0.10f, 0.09f), 22.0f });
    // === Left wing — one dedicated point light per lamp, consistent values ===
    // Ceiling lamp: ambient(0.022,0.020,0.016) diffuse(0.55,0.50,0.42) specular(0.08,0.07,0.06) r=8
    // Wall lamp:   ambient(0.018,0.016,0.013) diffuse(0.48,0.43,0.36) specular(0.07,0.06,0.05) r=7
    // Corridor:    ambient(0.015,0.013,0.010) diffuse(0.35,0.31,0.26) specular(0.05,0.04,0.04) r=9
    // R1 ceiling lamps (+50% extra on top of previous +50%)
    point_lights.push_back({ glm::vec3(-77.0f, 3.5f,  -9.0f), glm::vec3(0.108f,0.098f,0.080f), glm::vec3(2.70f,2.46f,2.06f), glm::vec3(0.395f,0.345f,0.297f),  8.0f });
    point_lights.push_back({ glm::vec3(-77.0f, 3.5f,   3.0f), glm::vec3(0.108f,0.098f,0.080f), glm::vec3(2.70f,2.46f,2.06f), glm::vec3(0.395f,0.345f,0.297f),  8.0f });
    // R1 wall lamps
    point_lights.push_back({ glm::vec3(-85.0f, 3.0f,  -3.0f), glm::vec3(0.093f,0.080f,0.066f), glm::vec3(2.37f,2.15f,1.76f), glm::vec3(0.345f,0.297f,0.246f),  7.0f }); // west
    point_lights.push_back({ glm::vec3(-77.0f, 3.0f, -12.5f), glm::vec3(0.093f,0.080f,0.066f), glm::vec3(2.37f,2.15f,1.76f), glm::vec3(0.345f,0.297f,0.246f),  7.0f }); // north
    point_lights.push_back({ glm::vec3(-77.0f, 3.0f,   6.5f), glm::vec3(0.093f,0.080f,0.066f), glm::vec3(2.37f,2.15f,1.76f), glm::vec3(0.345f,0.297f,0.246f),  7.0f }); // south
    // R1 corridor
    point_lights.push_back({ glm::vec3(-65.0f, 3.0f,  -5.0f), glm::vec3(0.072f,0.066f,0.050f), glm::vec3(1.74f,1.52f,1.26f), glm::vec3(0.246f,0.197f,0.197f),  9.0f });
    // R2 ceiling lamps
    point_lights.push_back({ glm::vec3(-77.0f, 3.5f, -35.0f), glm::vec3(0.108f,0.098f,0.080f), glm::vec3(2.70f,2.46f,2.06f), glm::vec3(0.395f,0.345f,0.297f),  8.0f });
    point_lights.push_back({ glm::vec3(-77.0f, 3.5f, -23.0f), glm::vec3(0.108f,0.098f,0.080f), glm::vec3(2.70f,2.46f,2.06f), glm::vec3(0.395f,0.345f,0.297f),  8.0f });
    // R2 wall lamps
    point_lights.push_back({ glm::vec3(-85.0f, 3.0f, -29.0f), glm::vec3(0.093f,0.080f,0.066f), glm::vec3(2.37f,2.15f,1.76f), glm::vec3(0.345f,0.297f,0.246f),  7.0f }); // west
    point_lights.push_back({ glm::vec3(-77.0f, 3.0f, -36.5f), glm::vec3(0.093f,0.080f,0.066f), glm::vec3(2.37f,2.15f,1.76f), glm::vec3(0.345f,0.297f,0.246f),  7.0f }); // north
    point_lights.push_back({ glm::vec3(-77.0f, 3.0f, -21.5f), glm::vec3(0.093f,0.080f,0.066f), glm::vec3(2.37f,2.15f,1.76f), glm::vec3(0.345f,0.297f,0.246f),  7.0f }); // south
    // R2 corridor
    point_lights.push_back({ glm::vec3(-65.0f, 3.0f, -29.0f), glm::vec3(0.072f,0.066f,0.050f), glm::vec3(1.74f,1.52f,1.26f), glm::vec3(0.246f,0.197f,0.197f),  9.0f });
    // Junction/wing lamps
    point_lights.push_back({ glm::vec3(-45.5f, 3.0f, -16.5f), glm::vec3(0.062f,0.053f,0.044f), glm::vec3(1.58f,1.43f,1.17f), glm::vec3(0.230f,0.198f,0.164f),  6.0f }); // entry left
    point_lights.push_back({ glm::vec3(-45.5f, 3.0f,  -9.0f), glm::vec3(0.062f,0.053f,0.044f), glm::vec3(1.58f,1.43f,1.17f), glm::vec3(0.230f,0.198f,0.164f),  6.0f }); // entry right
    point_lights.push_back({ glm::vec3(-55.0f, 3.0f, -17.0f), glm::vec3(0.062f,0.053f,0.044f), glm::vec3(1.58f,1.43f,1.17f), glm::vec3(0.230f,0.198f,0.164f),  9.0f }); // opp wall
    point_lights.push_back({ glm::vec3(-50.5f, 3.0f,  -3.5f), glm::vec3(0.062f,0.053f,0.044f), glm::vec3(1.58f,1.43f,1.17f), glm::vec3(0.230f,0.198f,0.164f),  7.0f }); // south wall
    point_lights.push_back({ glm::vec3(-50.5f, 3.0f, -32.0f), glm::vec3(0.062f,0.053f,0.044f), glm::vec3(1.58f,1.43f,1.17f), glm::vec3(0.230f,0.198f,0.164f),  7.0f }); // north wall
    // Junction general fill
    point_lights.push_back({ glm::vec3(-50.5f, 3.5f, -17.0f), glm::vec3(0.062f,0.053f,0.044f), glm::vec3(1.37f,1.26f,1.04f), glm::vec3(0.198f,0.164f,0.131f), 16.0f });
    // === East Wing — warm amber industrial lighting (×4 brightness) ===
    // East corridor
    point_lights.push_back({ glm::vec3( 33.5f, 3.0f, -12.5f), glm::vec3(0.88f,0.68f,0.36f), glm::vec3(8.40f,6.56f,4.00f), glm::vec3(1.04f,0.80f,0.52f), 18.0f });
    // Warehouse — 5 lights
    point_lights.push_back({ glm::vec3( 52.0f, 3.0f, -32.0f), glm::vec3(0.80f,0.64f,0.32f), glm::vec3(7.84f,6.08f,3.68f), glm::vec3(0.96f,0.74f,0.46f), 20.0f });
    point_lights.push_back({ glm::vec3( 73.0f, 3.0f, -27.0f), glm::vec3(0.80f,0.64f,0.32f), glm::vec3(7.84f,6.08f,3.68f), glm::vec3(0.96f,0.74f,0.46f), 20.0f });
    point_lights.push_back({ glm::vec3( 50.0f, 3.0f,  -6.0f), glm::vec3(0.80f,0.64f,0.32f), glm::vec3(7.84f,6.08f,3.68f), glm::vec3(0.96f,0.74f,0.46f), 20.0f });
    point_lights.push_back({ glm::vec3( 71.0f, 3.0f,  -2.0f), glm::vec3(0.80f,0.64f,0.32f), glm::vec3(7.84f,6.08f,3.68f), glm::vec3(0.96f,0.74f,0.46f), 20.0f });
    point_lights.push_back({ glm::vec3( 62.0f, 3.0f, -18.0f), glm::vec3(1.00f,0.80f,0.40f), glm::vec3(8.80f,6.88f,4.16f), glm::vec3(1.12f,0.86f,0.54f), 26.0f });
    // Reactor3 — indices 30-35, flicker in update_gameplay
    point_lights.push_back({ glm::vec3( 89.0f, 3.0f, -32.0f), glm::vec3(0.88f,0.68f,0.36f), glm::vec3(8.80f,6.88f,4.16f), glm::vec3(1.12f,0.86f,0.54f), 16.0f }); // 30
    point_lights.push_back({ glm::vec3( 89.0f, 3.0f, -16.0f), glm::vec3(0.88f,0.68f,0.36f), glm::vec3(8.80f,6.88f,4.16f), glm::vec3(1.12f,0.86f,0.54f), 16.0f }); // 31
    point_lights.push_back({ glm::vec3( 84.0f, 3.0f, -36.0f), glm::vec3(0.72f,0.56f,0.28f), glm::vec3(7.20f,5.60f,3.40f), glm::vec3(0.88f,0.68f,0.40f), 12.0f }); // 32
    point_lights.push_back({ glm::vec3( 94.0f, 3.0f, -36.0f), glm::vec3(0.72f,0.56f,0.28f), glm::vec3(7.20f,5.60f,3.40f), glm::vec3(0.88f,0.68f,0.40f), 12.0f }); // 33
    point_lights.push_back({ glm::vec3( 84.0f, 3.0f, -12.0f), glm::vec3(0.72f,0.56f,0.28f), glm::vec3(7.20f,5.60f,3.40f), glm::vec3(0.88f,0.68f,0.40f), 12.0f }); // 34
    point_lights.push_back({ glm::vec3( 94.0f, 3.0f, -12.0f), glm::vec3(0.72f,0.56f,0.28f), glm::vec3(7.20f,5.60f,3.40f), glm::vec3(0.88f,0.68f,0.40f), 12.0f }); // 35

    spot_lights.clear();

    // Shared textures. Some are loaded from files, others are simple solid
    // colors used for generated boxes, glass, UI helpers, and invisible colliders.
    auto tex_box             = std::make_shared<Texture>("textures/box.png");
    auto tex_metal_plate     = std::make_shared<Texture>("textures/metal_plate.jpg");
    auto tex_metal_plate_02  = std::make_shared<Texture>("textures/metal_plate_02.jpg");
    auto tex_rusty_metal     = std::make_shared<Texture>("textures/rusty_metal.jpg");
    auto tex_corrugated_iron = std::make_shared<Texture>("textures/corrugated_iron.jpg");
    auto tex_concrete_wall   = std::make_shared<Texture>("textures/concrete_wall.jpg");
    auto tex_metal_panel     = std::make_shared<Texture>("textures/metal_panel.jpg");
    auto tex_sci_floor       = std::make_shared<Texture>("textures/sci_floor.jpg");
    auto tex_floor = std::make_shared<Texture>(glm::vec3(0.30f, 0.32f, 0.30f));
    auto tex_wall = std::make_shared<Texture>(glm::vec3(0.22f, 0.25f, 0.28f));
    auto tex_dark = std::make_shared<Texture>(glm::vec3(0.08f, 0.09f, 0.10f));
    auto tex_terminal = std::make_shared<Texture>(glm::vec3(0.12f, 0.45f, 0.55f));
    auto tex_orc   = std::make_shared<Texture>("textures/orc_atlas.png");
    auto tex_catwalk = std::make_shared<Texture>("textures/catwalk_diamond_plate.png");
    auto tex_octagon_plate = std::make_shared<Texture>("textures/octagon_teardrop_plate.png");
    auto tex_rail = std::make_shared<Texture>(glm::vec3(0.68f, 0.82f, 0.74f));
    auto tex_lamp = std::make_shared<Texture>(glm::vec3(0.82f, 1.0f, 0.94f));
    auto tex_inner_orb = std::make_shared<Texture>(glm::vec4(1.0f, 0.46f, 0.08f, 1.0f));
    auto tex_orb_blue   = std::make_shared<Texture>(glm::vec4(0.22f, 0.48f, 1.0f, 1.0f));
    auto tex_red_glass = std::make_shared<Texture>(glm::vec4(1.0f, 0.12f, 0.08f, 1.0f));
    auto tex_blue_glass = std::make_shared<Texture>(glm::vec4(0.15f, 0.45f, 1.0f, 1.0f));
    auto tex_green_glass = std::make_shared<Texture>(glm::vec4(0.25f, 1.0f, 0.25f, 1.0f));
    light_debug_marker = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_lamp);
    light_debug_marker->material_alpha = 0.72f;
    light_debug_marker->is_transparent = true;

    // Clear gameplay/render registries before constructing the level.
    // This keeps init_assets() safe if it is ever called again in development.
    scene.clear();
    orb_layer_models.clear();
    reactors.clear();
    enemies.clear();
    fire_sources.clear();
    hub_door_panels.clear();
    particles.clear();
    particles.reserve(MAX_PARTICLES);
    reactors_active = 0;
    gate_unlocked = false;

    // Base floor plan and primary walls. These add both visible geometry and,
    // where collides=true, the first-pass physical boundaries.
    add_box("spawn_floor", glm::vec3(0.0f, -0.05f, 25.0f), glm::vec3(8.0f, 0.1f, 12.0f), tex_floor, true);
    add_box("entry_corridor_floor", glm::vec3(0.0f, -0.05f, 12.0f), glm::vec3(7.2f, 0.1f, 25.0f), tex_floor, true);
    add_box("hub_pit_floor", glm::vec3(0.0f, -14.5f, -12.5f), glm::vec3(34.0f, 0.4f, 34.0f), tex_dark);
    // Left wing floors — wider corridor + T-junction + two reactor rooms
    add_box("left_main_floor",      glm::vec3(-33.0f, -0.05f, -12.5f), glm::vec3(22.0f, 0.1f, 10.0f), tex_floor, true);
    add_box("left_junction_floor",  glm::vec3(-50.5f, -0.05f, -18.0f), glm::vec3(13.0f, 0.1f, 32.0f), tex_floor, true);
    add_box("r1_corridor_floor",    glm::vec3(-62.0f, -0.05f,  -5.0f), glm::vec3(10.0f, 0.1f,  6.0f), tex_floor, true);
    add_box("reactor1_floor",       glm::vec3(-77.0f, -0.05f,  -3.0f), glm::vec3(20.0f, 0.1f, 22.0f), tex_floor, true);
    add_box("r2_corridor_floor",    glm::vec3(-62.0f, -0.05f, -29.0f), glm::vec3(10.0f, 0.1f,  6.0f), tex_floor, true);
    add_box("reactor2_floor",       glm::vec3(-77.0f, -0.05f, -29.0f), glm::vec3(20.0f, 0.1f, 18.0f), tex_floor, true);
    // Right wing floors (tunnel end X≈44.4 → warehouse west wall X=44.5)
    add_box("east_corridor_floor",  glm::vec3( 33.5f, -0.05f, -12.5f), glm::vec3(23.0f, 0.1f, 10.0f), tex_floor, true);
    add_box("warehouse_floor",      glm::vec3( 63.0f, -0.05f, -17.0f), glm::vec3(36.0f, 0.1f, 46.0f), tex_floor, true);
    add_box("reactor3_floor",       glm::vec3( 89.0f, -0.05f, -24.0f), glm::vec3(14.0f, 0.1f, 33.0f), tex_floor, true);
    add_box("escape_corridor_floor", glm::vec3(0.0f, -0.05f, -37.0f), glm::vec3(8.5f, 0.1f, 18.0f), tex_floor, true);

    add_box("spawn_room_back_wall", glm::vec3(0.0f, 2.0f, 31.2f), glm::vec3(7.8f, 4.0f, 0.7f), tex_dark, true, 2.0f);
    add_box("spawn_room_left_wall", glm::vec3(-4.0f, 2.0f, 21.17f), glm::vec3(0.8f, 4.0f, 20.07f), tex_wall, true, 1.5f);
    add_box("spawn_room_right_wall", glm::vec3(4.0f, 2.0f, 21.17f), glm::vec3(0.8f, 4.0f, 20.07f), tex_wall, true, 1.5f);
    add_box("sealed_start_door", glm::vec3(0.0f, 2.0f, 28.0f), glm::vec3(6.5f, 4.0f, 0.7f), tex_dark, true, 2.0f);

    const glm::vec3 hub_center(0.0f, 0.0f, -12.5f);

    // Per-panel hex dome — each hexagon is a separate Model so frustum culling
    // can reject panels behind the player (the old single-mesh OBJ was never culled
    // because its bounding sphere ~34 units always contained the camera).
    {
        constexpr float dome_scale  = 1.45f;
        constexpr float frame_width = 0.32f;
        constexpr float portal_row_y = 1.7931034483f;

        auto tex_hub_shell = std::make_shared<Texture>(glm::vec3(0.78f, 0.86f, 0.82f));
        const glm::vec3 fill_emissive(0.018f, 0.020f, 0.018f);

        struct RowDef { float y, r; int count; float offset_deg, size; };
        const std::array<RowDef, 5> rows = {{
            { -2.05f,        15.7f, 28,  6.4285714286f, 2.35f },
            { portal_row_y,  16.3f, 28,  0.0f,          2.35f },
            {  5.65f,        15.1f, 24,  7.5f,          2.35f },
            {  9.15f,        11.6f, 20,  0.0f,          2.25f },
            { 11.85f,         7.2f, 12, 15.0f,          2.10f },
        }};

        auto is_portal = [&](float angle_deg, float y) -> bool {
            if (std::abs(y - portal_row_y) > 0.2f) return false;
            for (float ax : {0.0f, 90.0f, 180.0f, 270.0f}) {
                float d = std::abs(std::fmod(angle_deg - ax + 540.0f, 360.0f) - 180.0f);
                if (d < 10.0f) return true;
            }
            return false;
        };

        // Build one hexagonal panel (fill face + edge ring) as two scene entries.
        int panel_idx = 0;
        auto add_dome_panel = [&](const glm::vec3& ctr_ms, const glm::vec3& nm, float rad) {
            glm::vec3 n = glm::normalize(nm);
            // Local UV frame perpendicular to panel normal
            glm::vec3 u = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), n);
            if (glm::length(u) < 0.001f)
                u = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n);
            u = glm::normalize(u);
            glm::vec3 v = glm::normalize(glm::cross(n, u));

            float inner_r = rad - frame_width;
            glm::vec3 inward = -n; // normals face inside (toward player)

            // Panel pivot in world space; vertices are relative to this pivot
            glm::vec3 world_pivot = hub_center + ctr_ms * dome_scale;
            std::string sidx = std::to_string(panel_idx);

            // --- Fill hexagon (center + 6 inner vertices, 6 fan triangles) ---
            {
                std::vector<Vertex>  verts;  verts.reserve(7);
                std::vector<GLuint>  faces;  faces.reserve(18);

                verts.push_back({ n * (0.04f * dome_scale), inward, {0.5f, 0.5f} });
                for (int i = 0; i < 6; i++) {
                    float a  = glm::two_pi<float>() * i / 6.0f;
                    float ca = std::cos(a), sa = std::sin(a);
                    glm::vec3 pos = n * (0.035f * dome_scale)
                                  + u * (ca * inner_r * dome_scale)
                                  + v * (sa * inner_r * dome_scale);
                    verts.push_back({ pos, inward, {0.5f + ca * 0.45f, 0.5f + sa * 0.45f} });
                }
                for (int i = 0; i < 6; i++) {
                    faces.push_back(0);
                    faces.push_back(static_cast<GLuint>(1 + i));
                    faces.push_back(static_cast<GLuint>(1 + (i + 1) % 6));
                }

                auto m = std::make_shared<Model>();
                m->pivot_position = world_pivot;
                m->emissive_color  = fill_emissive;
                auto mesh = std::make_shared<Mesh>(verts, faces, GL_TRIANGLES);
                mesh->setTexture(tex_hub_shell);
                m->addMesh(mesh, shader_prog, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), tex_hub_shell);
                scene["hdp_f_" + sidx] = m;
            }

            // --- Edge ring (6 outer + 6 inner vertices, 12 triangles) ---
            {
                std::vector<Vertex>  verts;  verts.reserve(12);
                std::vector<GLuint>  faces;  faces.reserve(36);

                for (int i = 0; i < 6; i++) {
                    float a  = glm::two_pi<float>() * i / 6.0f;
                    float ca = std::cos(a), sa = std::sin(a);
                    glm::vec3 outer = u * (ca * rad    * dome_scale) + v * (sa * rad    * dome_scale);
                    glm::vec3 inner = n * (0.035f * dome_scale)
                                    + u * (ca * inner_r * dome_scale)
                                    + v * (sa * inner_r * dome_scale);
                    verts.push_back({ outer, inward, {0.0f, float(i) / 6.0f} });
                    verts.push_back({ inner, inward, {1.0f, float(i) / 6.0f} });
                }
                for (int i = 0; i < 6; i++) {
                    int next = (i + 1) % 6;
                    GLuint oi = i * 2, oi1 = next * 2, ii = i * 2 + 1, ii1 = next * 2 + 1;
                    faces.push_back(oi);  faces.push_back(oi1); faces.push_back(ii1);
                    faces.push_back(oi);  faces.push_back(ii1); faces.push_back(ii);
                }

                auto m = std::make_shared<Model>();
                m->pivot_position = world_pivot;
                auto mesh = std::make_shared<Mesh>(verts, faces, GL_TRIANGLES);
                mesh->setTexture(tex_dark);
                m->addMesh(mesh, shader_prog, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), tex_dark);
                scene["hdp_e_" + sidx] = m;
            }

            panel_idx++;
        };

        for (const auto& row : rows) {
            for (int i = 0; i < row.count; i++) {
                float angle_deg = row.offset_deg + (float(i) * 360.0f / float(row.count));
                if (is_portal(angle_deg, row.y)) continue;
                float ar = glm::radians(angle_deg);
                glm::vec3 ctr(std::cos(ar) * row.r, row.y, std::sin(ar) * row.r);
                glm::vec3 nm = glm::normalize(glm::vec3(ctr.x, ctr.y - 2.0f, ctr.z));
                add_dome_panel(ctr, nm, row.size);
            }
        }
        // Top cap panel
        add_dome_panel(glm::vec3(0.0f, 13.1f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 2.15f);
    }

    constexpr float hub_dome_model_scale = 1.45f;
    constexpr float hub_portal_panel_radius = 16.3f * hub_dome_model_scale;
    constexpr float hub_portal_panel_y = 1.7931034483f * hub_dome_model_scale;
    constexpr float hub_portal_hex_radius = 2.35f * hub_dome_model_scale;
    constexpr float hub_tunnel_visual_radius = hub_portal_hex_radius * 0.80f;
    constexpr float hub_tunnel_floor = 0.24f; // matches hub catwalk top surface
    constexpr float hub_tunnel_y = hub_tunnel_floor + 0.744782f * hub_tunnel_visual_radius;

    auto tex_tunnel_shell = std::make_shared<Texture>(glm::vec3(0.78f, 0.86f, 0.82f));
    auto add_hex_tunnel_shell = [&](const std::string& name, glm::vec3 position, float length, float yaw_degrees) {
        auto tunnel = std::make_shared<Model>("objects/hex_tunnel_shell.obj", shader_prog, tex_tunnel_shell);
        tunnel->pivot_position = position;
        tunnel->eulerAngles.y = yaw_degrees;
        tunnel->scale = glm::vec3(length, hub_tunnel_visual_radius, hub_tunnel_visual_radius);
        tunnel->is_transparent = false;
        tunnel->material_alpha = 1.0f;
        tunnel->two_sided_lighting = true;
        scene[name] = tunnel;
        return tunnel;
    };

    constexpr float west_tunnel_len = 20.2f;
    constexpr float east_tunnel_len = 20.8f;
    constexpr float south_tunnel_len = 16.8f;
    constexpr float north_tunnel_len = 9.8f;

    add_hex_tunnel_shell("hub_tunnel_shell_west",  glm::vec3(-hub_portal_panel_radius - west_tunnel_len * 0.5f, hub_tunnel_y, hub_center.z), west_tunnel_len, 0.0f);
    add_hex_tunnel_shell("hub_tunnel_shell_east",  glm::vec3( hub_portal_panel_radius + east_tunnel_len * 0.5f, hub_tunnel_y, hub_center.z), east_tunnel_len, 0.0f);
    add_hex_tunnel_shell("hub_tunnel_shell_south", glm::vec3(0.0f, hub_tunnel_y, hub_center.z + hub_portal_panel_radius + south_tunnel_len * 0.5f), south_tunnel_len, 90.0f);
    add_hex_tunnel_shell("hub_tunnel_shell_north", glm::vec3(0.0f, hub_tunnel_y, hub_center.z - hub_portal_panel_radius - north_tunnel_len * 0.5f), north_tunnel_len, 90.0f);

    auto add_lamp = [&](const std::string& name, const glm::vec3& position, const glm::vec3& size, float yaw_degrees = 0.0f) {
        auto lamp = add_box(name, position, size, tex_lamp, false);
        lamp->eulerAngles.y = yaw_degrees;
        lamp->emissive_color = glm::vec3(0.20f, 0.45f, 0.38f);
        return lamp;
    };
    auto add_lamp_warm = [&](const std::string& name, const glm::vec3& pos, const glm::vec3& size) {
        auto lamp = add_box(name, pos, size, tex_lamp, false);
        lamp->emissive_color = glm::vec3(0.88f, 0.68f, 0.22f); // warm industrial amber
        return lamp;
    };

    auto add_hex_lamp = [&](const std::string& name, const glm::vec3& position, float radius) {
        constexpr float apothem_factor = 0.8660254f;
        const float apothem = radius * apothem_factor;
        std::vector<Vertex> vertices;
        vertices.reserve(7);
        vertices.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f, 0.5f) });
        for (int i = 0; i < 6; ++i) {
            const float a = glm::half_pi<float>() + static_cast<float>(i) * glm::two_pi<float>() / 6.0f;
            const glm::vec2 p(std::cos(a) * radius, std::sin(a) * radius);
            vertices.push_back({
                glm::vec3(p.x, 0.0f, p.y),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec2(p.x / (apothem * 2.0f) + 0.5f, p.y / (radius * 2.0f) + 0.5f)
            });
        }

        std::vector<GLuint> indices;
        for (GLuint i = 1; i <= 6; ++i) {
            indices.push_back(0);
            indices.push_back(i == 6 ? 1 : i + 1);
            indices.push_back(i);
        }

        auto lamp = std::make_shared<Model>();
        auto mesh = std::make_shared<Mesh>(vertices, indices, GL_TRIANGLES);
        mesh->setTexture(tex_lamp);
        lamp->addMesh(mesh, shader_prog, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), tex_lamp);
        lamp->pivot_position = position;
        lamp->emissive_color = glm::vec3(0.36f, 0.78f, 0.66f);
        scene[name] = lamp;
        return lamp;
    };

    add_hex_lamp("hub_lamp_top_hex", glm::vec3(0.0f, 14.15f, hub_center.z), 2.35f);
    add_lamp("hub_lamp_west_portal", glm::vec3(-hub_portal_panel_radius + 0.4f, 4.9f, hub_center.z), glm::vec3(0.10f, 0.18f, 4.0f));
    add_lamp("hub_lamp_east_portal", glm::vec3( hub_portal_panel_radius - 0.4f, 4.9f, hub_center.z), glm::vec3(0.10f, 0.18f, 4.0f));
    add_lamp("hub_lamp_south_portal", glm::vec3(0.0f, 4.9f, hub_center.z + hub_portal_panel_radius - 0.4f), glm::vec3(4.0f, 0.18f, 0.10f));
    add_lamp("hub_lamp_north_portal", glm::vec3(0.0f, 4.9f, hub_center.z - hub_portal_panel_radius + 0.4f), glm::vec3(4.0f, 0.18f, 0.10f));

    auto tex_collision_clear = std::make_shared<Texture>(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

    // The hex tunnel meshes are visual shells only. Separate transparent boxes
    // below provide simple collision floors/walls for player and enemies.
    // Inner hex flat-side half-width = 0.860 * radius; use 0.82 to stay just inside the walls
    const float tunnel_walk_half_width  = 0.60f  * hub_tunnel_visual_radius; // ~1.77
    constexpr float tunnel_collision_thickness = 0.30f;
    // Inner hex height = 2 * 0.744782 * radius; use 1.44 to stay just inside ceiling
    const float tunnel_collision_height = 1.44f  * hub_tunnel_visual_radius;
    const float tunnel_collision_y      = hub_tunnel_y; // aligned with lowered tunnel shell
    auto add_tunnel_collision_x = [&](const std::string& name, glm::vec3 center, float length) {
        add_box(name + "_floor", glm::vec3(center.x, 0.19f, center.z),
                glm::vec3(length, 0.10f, tunnel_walk_half_width * 2.0f), tex_collision_clear, true, 1.0f, true, 0.0f);
        add_box(name + "_north_wall", glm::vec3(center.x, tunnel_collision_y, center.z - tunnel_walk_half_width),
                glm::vec3(length, tunnel_collision_height, tunnel_collision_thickness), tex_collision_clear, true, 1.0f, true, 0.0f);
        add_box(name + "_south_wall", glm::vec3(center.x, tunnel_collision_y, center.z + tunnel_walk_half_width),
                glm::vec3(length, tunnel_collision_height, tunnel_collision_thickness), tex_collision_clear, true, 1.0f, true, 0.0f);
    };
    auto add_tunnel_collision_z = [&](const std::string& name, glm::vec3 center, float length) {
        add_box(name + "_floor", glm::vec3(center.x, 0.19f, center.z),
                glm::vec3(tunnel_walk_half_width * 2.0f, 0.10f, length), tex_collision_clear, true, 1.0f, true, 0.0f);
        add_box(name + "_west_wall", glm::vec3(center.x - tunnel_walk_half_width, tunnel_collision_y, center.z),
                glm::vec3(tunnel_collision_thickness, tunnel_collision_height, length), tex_collision_clear, true, 1.0f, true, 0.0f);
        add_box(name + "_east_wall", glm::vec3(center.x + tunnel_walk_half_width, tunnel_collision_y, center.z),
                glm::vec3(tunnel_collision_thickness, tunnel_collision_height, length), tex_collision_clear, true, 1.0f, true, 0.0f);
    };

    add_tunnel_collision_x("hub_tunnel_collision_west",  glm::vec3(-hub_portal_panel_radius - west_tunnel_len * 0.5f, 0.0f, hub_center.z), west_tunnel_len);
    add_tunnel_collision_x("hub_tunnel_collision_east",  glm::vec3( hub_portal_panel_radius + east_tunnel_len * 0.5f, 0.0f, hub_center.z), east_tunnel_len);
    add_tunnel_collision_z("hub_tunnel_collision_south", glm::vec3(0.0f, 0.0f, hub_center.z + hub_portal_panel_radius + south_tunnel_len * 0.5f), south_tunnel_len);
    add_tunnel_collision_z("hub_tunnel_collision_north", glm::vec3(0.0f, 0.0f, hub_center.z - hub_portal_panel_radius - north_tunnel_len * 0.5f), north_tunnel_len);

    // Hub perimeter collision is approximated with straight box segments and
    // gaps at tunnel portals. The visible dome panels themselves do not collide.
    constexpr float hub_shell_radius = 23.8f;
    constexpr float hub_tunnel_half_width = 2.8f;
    constexpr float hub_shell_thickness = 0.8f;
    constexpr float hub_shell_height = 5.0f;
    const float hub_shell_y = hub_shell_height * 0.5f;
    const float hub_wall_segment_len = hub_shell_radius - hub_tunnel_half_width;
    const float hub_wall_segment_offset = (hub_shell_radius + hub_tunnel_half_width) * 0.5f;

    add_box("hub_collision_east_north", glm::vec3( hub_shell_radius, hub_shell_y, hub_center.z - hub_wall_segment_offset), glm::vec3(hub_shell_thickness, hub_shell_height, hub_wall_segment_len), tex_collision_clear, true, 1.0f, true, 0.0f);
    add_box("hub_collision_east_south", glm::vec3( hub_shell_radius, hub_shell_y, hub_center.z + hub_wall_segment_offset), glm::vec3(hub_shell_thickness, hub_shell_height, hub_wall_segment_len), tex_collision_clear, true, 1.0f, true, 0.0f);
    add_box("hub_collision_west_north", glm::vec3(-hub_shell_radius, hub_shell_y, hub_center.z - hub_wall_segment_offset), glm::vec3(hub_shell_thickness, hub_shell_height, hub_wall_segment_len), tex_collision_clear, true, 1.0f, true, 0.0f);
    add_box("hub_collision_west_south", glm::vec3(-hub_shell_radius, hub_shell_y, hub_center.z + hub_wall_segment_offset), glm::vec3(hub_shell_thickness, hub_shell_height, hub_wall_segment_len), tex_collision_clear, true, 1.0f, true, 0.0f);
    add_box("hub_collision_south_west", glm::vec3(-hub_wall_segment_offset, hub_shell_y, hub_center.z + hub_shell_radius), glm::vec3(hub_wall_segment_len, hub_shell_height, hub_shell_thickness), tex_collision_clear, true, 1.0f, true, 0.0f);
    add_box("hub_collision_south_east", glm::vec3( hub_wall_segment_offset, hub_shell_y, hub_center.z + hub_shell_radius), glm::vec3(hub_wall_segment_len, hub_shell_height, hub_shell_thickness), tex_collision_clear, true, 1.0f, true, 0.0f);
    add_box("hub_collision_north_west", glm::vec3(-hub_wall_segment_offset, hub_shell_y, hub_center.z - hub_shell_radius), glm::vec3(hub_wall_segment_len, hub_shell_height, hub_shell_thickness), tex_collision_clear, true, 1.0f, true, 0.0f);
    add_box("hub_collision_north_east", glm::vec3( hub_wall_segment_offset, hub_shell_y, hub_center.z - hub_shell_radius), glm::vec3(hub_wall_segment_len, hub_shell_height, hub_shell_thickness), tex_collision_clear, true, 1.0f, true, 0.0f);

    // Hub catwalks connect to the restored center octagon and orb pedestal.
    // The diagonal rail pieces are visual because collision is axis-aligned.
    constexpr float hub_oct_radius = 6.5f;
    constexpr float hub_oct_apothem = hub_oct_radius * 0.9238795f;
    constexpr float hub_oct_corner = hub_oct_radius * 0.3826834f;

    auto hub_platform = std::make_shared<Model>("objects/oct_platform.obj", shader_prog, tex_floor);
    hub_platform->pivot_position = glm::vec3(hub_center.x, 0.16f, hub_center.z);
    hub_platform->scale = glm::vec3(hub_oct_radius, 0.32f, hub_oct_radius);
    hub_platform->emissive_color = glm::vec3(0.006f, 0.014f, 0.012f);
    scene["hub_platform_center_oct"] = hub_platform;

    auto add_oct_catwalk_top = [&](const std::string& name, const glm::vec3& center, float radius, float y) {
        // Builds a flat octagonal top surface from a triangle fan. This keeps
        // the visible platform shape aligned with the hub pit/walkway checks.
        constexpr float apothem_factor = 0.9238795f;
        constexpr float corner_factor = 0.3826834f;
        const float apothem = radius * apothem_factor;
        const float corner = radius * corner_factor;
        const float repeat = 4.2f;

        std::vector<Vertex> vertices;
        vertices.reserve(9);
        vertices.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(repeat * 0.5f, repeat * 0.5f) });

        const std::array<glm::vec2, 8> outline = {
            glm::vec2( apothem, -corner),
            glm::vec2( apothem,  corner),
            glm::vec2( corner,   apothem),
            glm::vec2(-corner,   apothem),
            glm::vec2(-apothem,  corner),
            glm::vec2(-apothem, -corner),
            glm::vec2(-corner,  -apothem),
            glm::vec2( corner,  -apothem)
        };

        for (const glm::vec2& p : outline) {
            vertices.push_back({
                glm::vec3(p.x, 0.0f, p.y),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec2((p.x / (apothem * 2.0f) + 0.5f) * repeat,
                          (p.y / (apothem * 2.0f) + 0.5f) * repeat)
            });
        }

        std::vector<GLuint> indices;
        indices.reserve(24);
        for (GLuint i = 1; i <= 8; ++i) {
            indices.push_back(0);
            indices.push_back(i);
            indices.push_back(i == 8 ? 1 : i + 1);
        }

        auto top = std::make_shared<Model>();
        auto mesh = std::make_shared<Mesh>(vertices, indices, GL_TRIANGLES);
        mesh->setTexture(tex_octagon_plate);
        top->addMesh(mesh, shader_prog, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), tex_octagon_plate);
        top->pivot_position = glm::vec3(center.x, y, center.z);
        top->emissive_color = glm::vec3(0.012f, 0.020f, 0.018f);
        scene[name] = top;
        return top;
    };

    add_oct_catwalk_top("hub_platform_center_oct_top", hub_center, hub_oct_radius, 0.326f);

    auto add_oct_floor_collision = [&](const std::string& name, glm::vec3 center, float radius, float height) {
        // Approximate an octagonal floor using narrow strips. The player
        // collision code only understands boxes, so this gives a good shape
        // without triangle collision.
        const float apothem = radius * 0.9238795f;
        const float corner = radius * 0.3826834f;
        constexpr int strips = 8;
        const float strip_depth = (apothem * 2.0f) / static_cast<float>(strips);

        for (int i = 0; i < strips; ++i) {
            const float z_min = -apothem + strip_depth * static_cast<float>(i);
            const float z_max = z_min + strip_depth;
            const float farthest_z = std::max(std::abs(z_min), std::abs(z_max));
            const float half_width = farthest_z <= corner ? apothem : apothem + corner - farthest_z;
            if (half_width <= 0.05f) {
                continue;
            }

            add_box(name + "_strip_" + std::to_string(i),
                    glm::vec3(center.x, center.y, center.z + (z_min + z_max) * 0.5f),
                    glm::vec3(half_width * 2.0f, height, strip_depth),
                    tex_collision_clear, true, 1.0f, true, 0.0f);
        }
    };

    add_oct_floor_collision("hub_platform_center_collision",
                            glm::vec3(hub_center.x, 0.12f, hub_center.z),
                            hub_oct_radius,
                            0.24f);

    constexpr float hub_catwalk_portal_end = 23.45f;
    constexpr float hub_catwalk_inner_end = 5.85f;
    constexpr float hub_catwalk_width = 2.2f;
    const float hub_catwalk_length = hub_catwalk_portal_end - hub_catwalk_inner_end;
    const float hub_catwalk_center_offset = (hub_catwalk_portal_end + hub_catwalk_inner_end) * 0.5f;

    auto add_catwalk_top = [&](const std::string& name, const glm::vec3& position, const glm::vec3& size, bool rotate_pattern) {
        // Custom top mesh gives catwalks real UVs, unlike a scaled cube where
        // the diamond-plate pattern would stretch badly.
        const float half_x = size.x * 0.5f;
        const float half_z = size.z * 0.5f;
        const float repeat_x = rotate_pattern ? std::max(size.x / 1.35f, 1.0f) : 1.0f;
        const float repeat_z = rotate_pattern ? 1.0f : std::max(size.z / 1.35f, 1.0f);
        const auto uv = [&](float u, float v) {
            return rotate_pattern ? glm::vec2(v, u) : glm::vec2(u, v);
        };

        std::vector<Vertex> vertices = {
            { glm::vec3(-half_x, 0.0f, -half_z), glm::vec3(0.0f, 1.0f, 0.0f), uv(0.0f, 0.0f) },
            { glm::vec3( half_x, 0.0f, -half_z), glm::vec3(0.0f, 1.0f, 0.0f), uv(repeat_x, 0.0f) },
            { glm::vec3( half_x, 0.0f,  half_z), glm::vec3(0.0f, 1.0f, 0.0f), uv(repeat_x, repeat_z) },
            { glm::vec3(-half_x, 0.0f,  half_z), glm::vec3(0.0f, 1.0f, 0.0f), uv(0.0f, repeat_z) }
        };
        std::vector<GLuint> indices = { 0, 1, 2, 0, 2, 3 };

        auto top = std::make_shared<Model>();
        auto mesh = std::make_shared<Mesh>(vertices, indices, GL_TRIANGLES);
        mesh->setTexture(tex_catwalk);
        top->addMesh(mesh, shader_prog, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), tex_catwalk);
        top->pivot_position = position + glm::vec3(0.0f, size.y * 0.5f + 0.006f, 0.0f);
        top->emissive_color = glm::vec3(0.012f, 0.020f, 0.018f);
        scene[name] = top;
        return top;
    };

    auto add_catwalk = [&](const std::string& name, const glm::vec3& position, const glm::vec3& size, bool rotate_pattern = false) {
        // The base box is both visual thickness and collision; the custom top
        // mesh is added just above it for better texture mapping.
        auto base = add_box(name + "_base", position, size, tex_floor, true);
        base->emissive_color = glm::vec3(0.006f, 0.014f, 0.012f);
        add_catwalk_top(name, position, size, rotate_pattern);
        return base;
    };

    add_catwalk("hub_catwalk_west",  glm::vec3(-hub_catwalk_center_offset, 0.12f, hub_center.z), glm::vec3(hub_catwalk_length, 0.24f, hub_catwalk_width), true);
    add_catwalk("hub_catwalk_east",  glm::vec3( hub_catwalk_center_offset, 0.12f, hub_center.z), glm::vec3(hub_catwalk_length, 0.24f, hub_catwalk_width), true);
    add_catwalk("hub_catwalk_south", glm::vec3(0.0f, 0.12f, hub_center.z + hub_catwalk_center_offset), glm::vec3(hub_catwalk_width, 0.24f, hub_catwalk_length));
    add_catwalk("hub_catwalk_north", glm::vec3(0.0f, 0.12f, hub_center.z - hub_catwalk_center_offset), glm::vec3(hub_catwalk_width, 0.24f, hub_catwalk_length));

    constexpr float rail_top_y = 1.0f;
    constexpr float rail_mid_y = 0.78f;
    constexpr float rail_leg_bottom_y = 0.30f;
    constexpr float rail_thickness = 0.08f;
    constexpr float rail_post_spacing = 3.25f;

    auto add_rail_bar_3d = [&](const std::string& name, glm::vec3 a, glm::vec3 b, float thickness) {
        // Creates a rectangular rail bar between two 3D points and rotates it
        // around Y so it follows the segment direction.
        const glm::vec3 delta = b - a;
        const float length = glm::length(delta);
        if (length <= 0.001f) {
            return std::shared_ptr<Model>{};
        }

        auto rail = add_box(name, (a + b) * 0.5f, glm::vec3(length, thickness, thickness), tex_rail, false, 1.0f);
        rail->eulerAngles.y = -glm::degrees(std::atan2(delta.z, delta.x));
        rail->eulerAngles.z = 0.0f;
        rail->emissive_color = glm::vec3(0.030f, 0.014f, 0.010f);
        return rail;
    };

    auto add_railing_segment = [&](const std::string& name, glm::vec2 a, glm::vec2 b, bool collides = true) {
        // A railing segment is visual geometry plus an optional invisible
        // collider aligned to the same 2D segment.
        const glm::vec2 delta = b - a;
        const float length = glm::length(delta);
        if (length <= 0.001f) {
            return;
        }

        const glm::vec3 a_top(a.x, rail_top_y, hub_center.z + a.y);
        const glm::vec3 b_top(b.x, rail_top_y, hub_center.z + b.y);
        const glm::vec3 a_mid(a.x, rail_mid_y, hub_center.z + a.y);
        const glm::vec3 b_mid(b.x, rail_mid_y, hub_center.z + b.y);

        add_rail_bar_3d(name + "_top", a_top, b_top, rail_thickness * 1.25f);
        add_rail_bar_3d(name + "_mid", a_mid, b_mid, rail_thickness);

        const int posts = std::max(2, static_cast<int>(std::ceil(length / rail_post_spacing)) + 1);
        for (int i = 0; i < posts; ++i) {
            const float t = posts == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(posts - 1);
            const glm::vec2 p = a + delta * t;
            auto post = add_box(name + "_post_" + std::to_string(i),
                                glm::vec3(p.x, (rail_top_y + rail_leg_bottom_y) * 0.5f, hub_center.z + p.y),
                                glm::vec3(rail_thickness, rail_top_y - rail_leg_bottom_y, rail_thickness),
                                tex_rail, false, 1.0f);
            post->emissive_color = glm::vec3(0.030f, 0.014f, 0.010f);
        }

        if (!collides) {
            return;
        }

        const glm::vec2 mid = (a + b) * 0.5f;
        const float collider_y = rail_top_y * 0.5f;
        const float collider_height = rail_top_y;
        const bool axis_aligned = std::abs(delta.x) < 0.001f || std::abs(delta.y) < 0.001f;
        if (!axis_aligned) {
            auto collision = add_box(name + "_collision", glm::vec3(mid.x, collider_y, hub_center.z + mid.y),
                                     glm::vec3(length, collider_height, 0.26f), tex_collision_clear, true, 1.0f, true, 0.0f);
            collision->eulerAngles.y = -glm::degrees(std::atan2(delta.y, delta.x));
        } else if (std::abs(delta.x) >= std::abs(delta.y)) {
            add_box(name + "_collision", glm::vec3(mid.x, collider_y, hub_center.z + mid.y),
                    glm::vec3(length, collider_height, 0.26f), tex_collision_clear, true, 1.0f, true, 0.0f);
        } else {
            add_box(name + "_collision", glm::vec3(mid.x, collider_y, hub_center.z + mid.y),
                    glm::vec3(0.26f, collider_height, length), tex_collision_clear, true, 1.0f, true, 0.0f);
        }
    };

    add_railing_segment("hub_oct_rail_west_north", glm::vec2(-hub_oct_apothem, -hub_oct_corner), glm::vec2(-hub_oct_apothem, -1.1f));
    add_railing_segment("hub_oct_rail_west_south", glm::vec2(-hub_oct_apothem,  1.1f), glm::vec2(-hub_oct_apothem,  hub_oct_corner));
    add_railing_segment("hub_oct_rail_east_north", glm::vec2( hub_oct_apothem, -hub_oct_corner), glm::vec2( hub_oct_apothem, -1.1f));
    add_railing_segment("hub_oct_rail_east_south", glm::vec2( hub_oct_apothem,  1.1f), glm::vec2( hub_oct_apothem,  hub_oct_corner));
    add_railing_segment("hub_oct_rail_south_left",  glm::vec2(-hub_oct_corner,  hub_oct_apothem), glm::vec2(-1.1f,  hub_oct_apothem));
    add_railing_segment("hub_oct_rail_south_right", glm::vec2( 1.1f,  hub_oct_apothem), glm::vec2( hub_oct_corner,  hub_oct_apothem));
    add_railing_segment("hub_oct_rail_north_left",  glm::vec2(-hub_oct_corner, -hub_oct_apothem), glm::vec2(-1.1f, -hub_oct_apothem));
    add_railing_segment("hub_oct_rail_north_right", glm::vec2( 1.1f, -hub_oct_apothem), glm::vec2( hub_oct_corner, -hub_oct_apothem));
    add_railing_segment("hub_oct_rail_sw", glm::vec2(-hub_oct_apothem,  hub_oct_corner), glm::vec2(-hub_oct_corner,  hub_oct_apothem));
    add_railing_segment("hub_oct_rail_nw", glm::vec2(-hub_oct_apothem, -hub_oct_corner), glm::vec2(-hub_oct_corner, -hub_oct_apothem));
    add_railing_segment("hub_oct_rail_ne", glm::vec2( hub_oct_apothem, -hub_oct_corner), glm::vec2( hub_oct_corner, -hub_oct_apothem));
    add_railing_segment("hub_oct_rail_se", glm::vec2( hub_oct_apothem,  hub_oct_corner), glm::vec2( hub_oct_corner,  hub_oct_apothem));
    add_railing_segment("hub_rail_west_north", glm::vec2(-hub_catwalk_portal_end, -1.1f), glm::vec2(-hub_oct_apothem, -1.1f));
    add_railing_segment("hub_rail_west_south", glm::vec2(-hub_catwalk_portal_end,  1.1f), glm::vec2(-hub_oct_apothem,  1.1f));
    add_railing_segment("hub_rail_east_north", glm::vec2( hub_oct_apothem, -1.1f), glm::vec2(hub_catwalk_portal_end, -1.1f));
    add_railing_segment("hub_rail_east_south", glm::vec2( hub_oct_apothem,  1.1f), glm::vec2(hub_catwalk_portal_end,  1.1f));
    add_railing_segment("hub_rail_south_left",  glm::vec2(-1.1f,  hub_oct_apothem), glm::vec2(-1.1f, hub_catwalk_portal_end));
    add_railing_segment("hub_rail_south_right", glm::vec2( 1.1f,  hub_oct_apothem), glm::vec2( 1.1f, hub_catwalk_portal_end));
    add_railing_segment("hub_rail_north_left",  glm::vec2(-1.1f, -hub_catwalk_portal_end), glm::vec2(-1.1f, -hub_oct_apothem));
    add_railing_segment("hub_rail_north_right", glm::vec2( 1.1f, -hub_catwalk_portal_end), glm::vec2( 1.1f, -hub_oct_apothem));

    // === Left wing: main corridor → T-junction → Reactor 1 (south) + Reactor 2 (north) ===
    // T-junction room (x=-44 to -57, z=-34 to -2)
    add_box("left_junc_north_wall",  glm::vec3(-50.5f, 2.0f, -34.5f), glm::vec3(13.0f, 4.0f, 0.8f), tex_concrete_wall, true, 1.5f);
    add_box("left_junc_south_wall",  glm::vec3(-50.5f, 2.0f,  -1.5f), glm::vec3(13.0f, 4.0f, 0.8f), tex_concrete_wall, true, 1.5f);
    // East wall — opening z=-14.861 to -10.139
    add_box("left_junc_east_n", glm::vec3(-44.235f, 2.0f, -24.430f), glm::vec3(0.8f, 4.0f, 19.139f), tex_concrete_wall, true, 1.5f);
    add_box("left_junc_east_s", glm::vec3(-44.235f, 2.0f,  -6.070f), glm::vec3(0.8f, 4.0f,  8.139f), tex_concrete_wall, true, 1.5f);
    // Short hex cap bridging tunnel end to junction wall — covers corner gaps naturally
    constexpr float junc_cap_len = 1.6f;
    add_hex_tunnel_shell("hub_tunnel_cap_west",
        glm::vec3(-hub_portal_panel_radius - west_tunnel_len - junc_cap_len * 0.5f, hub_tunnel_y, hub_center.z),
        junc_cap_len, 0.0f);
    // West wall of junction — two gaps: R2 at z=-32 to -26, R1 at z=-8 to -2
    add_box("left_junc_west_n",      glm::vec3(-57.0f, 2.0f, -33.0f), glm::vec3(0.8f, 4.0f,  2.0f), tex_concrete_wall, true, 1.5f);
    add_box("left_junc_west_mid",    glm::vec3(-57.0f, 2.0f, -17.0f), glm::vec3(0.8f, 4.0f, 18.0f), tex_concrete_wall, true, 1.5f);

    // Reactor 1 corridor — extended to x=-67.5 to meet entry wall flush
    add_box("r1_corr_north_wall",    glm::vec3(-62.25f, 2.0f,  -8.5f), glm::vec3(10.5f, 4.0f, 0.8f), tex_concrete_wall, true, 1.5f);
    add_box("r1_corr_south_wall",    glm::vec3(-62.25f, 2.0f,  -1.5f), glm::vec3(10.5f, 4.0f, 0.8f), tex_concrete_wall, true, 1.5f);

    // Reactor 1 room — entry opening aligned with corridor (z=-8.5 to -1.5, 7 units)
    add_box("reactor1_room_west",    glm::vec3(-87.5f, 2.0f,  -3.0f), glm::vec3(0.8f, 4.0f, 22.0f), tex_concrete_wall, true, 1.5f);
    add_box("reactor1_room_north",   glm::vec3(-77.0f, 2.0f, -14.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_concrete_wall, true, 1.5f);
    add_box("reactor1_room_south",   glm::vec3(-77.0f, 2.0f,   8.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_concrete_wall, true, 1.5f);
    add_box("reactor1_entry_n",      glm::vec3(-67.5f, 2.0f, -11.75f), glm::vec3(0.8f, 4.0f,  5.5f), tex_concrete_wall, true, 1.5f);
    add_box("reactor1_entry_s",      glm::vec3(-67.5f, 2.0f,   3.25f), glm::vec3(0.8f, 4.0f,  9.5f), tex_concrete_wall, true, 1.5f);
    // Reactor 1 room — ceiling strip lights
    add_lamp("r1_ceil_lamp_n",     glm::vec3(-77.0f, 3.88f,  -9.0f), glm::vec3(5.5f, 0.12f, 0.55f));
    add_lamp("r1_ceil_lamp_s",     glm::vec3(-77.0f, 3.88f,   3.0f), glm::vec3(5.5f, 0.12f, 0.55f));
    // Reactor 1 room — wall strip lights
    add_lamp("r1_wall_lamp_west",  glm::vec3(-87.1f, 2.85f,  -3.0f), glm::vec3(0.12f, 0.22f, 12.0f));
    add_lamp("r1_wall_lamp_north", glm::vec3(-77.0f, 2.85f, -14.1f), glm::vec3(12.0f, 0.22f, 0.12f));
    add_lamp("r1_wall_lamp_south", glm::vec3(-77.0f, 2.85f,   8.1f), glm::vec3(12.0f, 0.22f, 0.12f));
    // Left and right of entrance — inner face of east wall, near hex opening edges (z=-14.861 / -10.139)
    add_lamp("wing_entry_left",    glm::vec3(-44.64f, 2.85f, -16.5f),  glm::vec3(0.12f, 0.22f, 3.5f));
    add_lamp("wing_entry_right",   glm::vec3(-44.64f, 2.85f,  -9.0f),  glm::vec3(0.12f, 0.22f, 2.5f));
    // Wall opposite the tunnel opening — west wall of the junction, full width (z=-26 to -8)
    add_lamp("wing_opp_wall_lamp", glm::vec3(-56.6f,  2.85f, -17.0f),  glm::vec3(0.12f, 0.22f, 17.5f));
    // Right wall when entering from tunnel — south wall of junction, middle
    add_lamp("wing_south_wall_lamp", glm::vec3(-50.5f, 2.85f,  -1.9f),  glm::vec3(8.0f, 0.22f, 0.12f));
    // North wall of junction — the blank far wall (inside room, lamp faces south)
    add_lamp("wing_north_wall_lamp", glm::vec3(-50.5f, 2.85f, -33.85f), glm::vec3(8.0f, 0.22f, 0.12f));
    // === East Wing lamps — warm amber industrial overhead fixtures ===
    add_lamp_warm("east_corr_lamp_c", glm::vec3( 33.5f, 3.88f, -12.5f), glm::vec3(3.0f, 0.12f, 3.0f));
    add_lamp_warm("wh_lamp_nw",       glm::vec3( 52.0f, 3.88f, -32.0f), glm::vec3(4.0f, 0.12f, 1.0f));
    add_lamp_warm("wh_lamp_ne",       glm::vec3( 73.0f, 3.88f, -27.0f), glm::vec3(1.0f, 0.12f, 4.0f));
    add_lamp_warm("wh_lamp_sw",       glm::vec3( 50.0f, 3.88f,  -6.0f), glm::vec3(4.0f, 0.12f, 1.0f));
    add_lamp_warm("wh_lamp_se",       glm::vec3( 71.0f, 3.88f,  -2.0f), glm::vec3(1.0f, 0.12f, 4.0f));
    add_lamp_warm("wh_lamp_center",   glm::vec3( 62.0f, 3.88f, -18.0f), glm::vec3(5.0f, 0.12f, 5.0f));
    add_lamp_warm("r3_lamp_n",        glm::vec3( 89.0f, 3.88f, -32.0f), glm::vec3(5.0f, 0.12f, 0.8f));
    add_lamp_warm("r3_lamp_s",        glm::vec3( 89.0f, 3.88f, -16.0f), glm::vec3(5.0f, 0.12f, 0.8f));

    // === T-junction props ===
    auto tex_warning = std::make_shared<Texture>(glm::vec3(0.78f, 0.64f, 0.04f));

    // Floor warning stripes marking the east-west corridor crossing
    add_box("junc_stripe_n", glm::vec3(-50.5f, 0.02f, -15.0f), glm::vec3(12.5f, 0.04f, 0.35f), tex_warning, false);
    add_box("junc_stripe_s", glm::vec3(-50.5f, 0.02f, -10.5f), glm::vec3(12.5f, 0.04f, 0.35f), tex_warning, false);

    // Pipes along west wall — split to avoid doorway openings (R2 gap z=-32 to -26, R1 gap z=-8 to -2)
    add_box("junc_pipe_lo_n",   glm::vec3(-56.7f, 0.55f, -33.0f), glm::vec3(0.18f, 0.18f,  2.0f), tex_rusty_metal, false, 2.0f);
    add_box("junc_pipe_lo_mid", glm::vec3(-56.7f, 0.55f, -17.0f), glm::vec3(0.18f, 0.18f, 18.0f), tex_rusty_metal, false, 2.0f);
    add_box("junc_pipe_hi_n",   glm::vec3(-56.7f, 2.30f, -33.0f), glm::vec3(0.18f, 0.18f,  2.0f), tex_rusty_metal, false, 2.0f);
    add_box("junc_pipe_hi_mid", glm::vec3(-56.7f, 2.30f, -17.0f), glm::vec3(0.18f, 0.18f, 18.0f), tex_rusty_metal, false, 2.0f);
    // Pipe brackets only on solid wall segments
    add_box("junc_pipe_brk1", glm::vec3(-56.5f, 1.40f, -25.0f), glm::vec3(0.35f, 2.10f, 0.22f), tex_dark, false, 1.5f);
    add_box("junc_pipe_brk2", glm::vec3(-56.5f, 1.40f, -17.0f), glm::vec3(0.35f, 2.10f, 0.22f), tex_dark, false, 1.5f);
    add_box("junc_pipe_brk3", glm::vec3(-56.5f, 1.40f,  -9.5f), glm::vec3(0.35f, 2.10f, 0.22f), tex_dark, false, 1.5f);

    // Central console station — OBJ models against west wall (z=-24 to -17)
    auto add_obj = [&](const std::string& name, const std::string& path,
                       glm::vec3 pos, glm::vec3 euler, glm::vec3 sc,
                       std::shared_ptr<Texture> tex) {
        // Decorative OBJ helper. Callers can set collides=true afterward when
        // a prop should also block movement.
        auto m = std::make_shared<Model>("objects/" + path, shader_prog, tex);
        m->pivot_position = pos;
        m->eulerAngles    = euler;
        m->scale          = sc;
        scene[name] = m;
        return m;
    };

    add_obj("junc_computer",      "computer-system.obj",  glm::vec3(-56.0f, 0.0f, -22.0f), glm::vec3(0,  90, 0), glm::vec3(2.10f), tex_metal_plate_02)->collides = true;
    add_obj("junc_screen",        "computer-screen.obj",  glm::vec3(-56.0f, 0.0f, -19.5f), glm::vec3(0,  90, 0), glm::vec3(2.10f), tex_terminal)->collides = true;
    add_obj("junc_table",         "table-large.obj",      glm::vec3(-55.0f, 0.0f, -17.0f), glm::vec3(0,   0, 0), glm::vec3(1.95f), tex_metal_plate)->collides = true;
    add_obj("junc_chair_1",       "chair.obj",            glm::vec3(-53.0f, 0.0f, -19.5f), glm::vec3(0, 180, 0), glm::vec3(1.80f), tex_dark)->collides = true;
    add_obj("junc_chair_2",       "chair.obj",            glm::vec3(-53.0f, 0.0f, -22.0f), glm::vec3(0, 180, 0), glm::vec3(1.80f), tex_dark)->collides = true;

    // Containers NE corner — against east wall
    add_obj("junc_container_ne1", "container-tall.obj",   glm::vec3(-45.0f, 0.0f, -27.0f), glm::vec3(0,   0, 0), glm::vec3(1.95f), tex_corrugated_iron)->collides = true;
    add_obj("junc_container_ne2", "container.obj",        glm::vec3(-45.0f, 0.0f, -24.0f), glm::vec3(0,  90, 0), glm::vec3(1.80f), tex_metal_plate_02)->collides = true;
    add_obj("junc_box_ne",        "box-large.obj",        glm::vec3(-46.0f, 0.0f, -22.0f), glm::vec3(0,  45, 0), glm::vec3(1.65f), tex_box)->collides = true;

    // Containers SE corner — against east wall
    add_obj("junc_container_se1", "container.obj",        glm::vec3(-45.0f, 0.0f,  -6.5f), glm::vec3(0,   0, 0), glm::vec3(1.80f), tex_corrugated_iron)->collides = true;
    add_obj("junc_box_se1",       "box-large.obj",        glm::vec3(-45.5f, 0.0f,  -4.5f), glm::vec3(0,  20, 0), glm::vec3(1.50f), tex_box)->collides = true;
    add_obj("junc_box_se2",       "box-small.obj",        glm::vec3(-46.5f, 0.0f,  -3.5f), glm::vec3(0, -15, 0), glm::vec3(1.65f), tex_box)->collides = true;

    // Reactor 2 corridor (north arm, z=-32 to -26, x=-57 to -67)
    add_box("r2_corr_north_wall",    glm::vec3(-62.0f, 2.0f, -32.5f), glm::vec3(10.0f, 4.0f, 0.8f), tex_metal_panel, true, 1.5f);
    add_box("r2_corr_south_wall",    glm::vec3(-62.0f, 2.0f, -25.5f), glm::vec3(10.0f, 4.0f, 0.8f), tex_metal_panel, true, 1.5f);

    // Reactor 2 room (x=-67 to -87, z=-38 to -20) — entry from east at z=-32 to -26
    add_box("reactor2_room_west",    glm::vec3(-87.5f, 2.0f, -29.0f), glm::vec3(0.8f, 4.0f, 18.0f), tex_metal_panel, true, 1.5f);
    add_box("reactor2_room_north",   glm::vec3(-77.0f, 2.0f, -38.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_metal_panel, true, 1.5f);
    add_box("reactor2_room_south",   glm::vec3(-77.0f, 2.0f, -19.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_metal_panel, true, 1.5f);
    add_box("reactor2_entry_n",      glm::vec3(-67.5f, 2.0f, -35.0f), glm::vec3(0.8f, 4.0f,  6.0f), tex_metal_panel, true, 1.5f);
    add_box("reactor2_entry_s",      glm::vec3(-67.5f, 2.0f, -23.0f), glm::vec3(0.8f, 4.0f,  6.0f), tex_metal_panel, true, 1.5f);
    // Reactor 2 room — ceiling strip lights (mirrored from R1, offset z by -26)
    add_lamp("r2_ceil_lamp_n",     glm::vec3(-77.0f, 3.88f, -35.0f), glm::vec3(5.5f, 0.12f, 0.55f));
    add_lamp("r2_ceil_lamp_s",     glm::vec3(-77.0f, 3.88f, -23.0f), glm::vec3(5.5f, 0.12f, 0.55f));
    // Reactor 2 room — wall strip lights
    add_lamp("r2_wall_lamp_west",  glm::vec3(-87.1f, 2.85f, -29.0f), glm::vec3(0.12f, 0.22f, 12.0f));
    add_lamp("r2_wall_lamp_north", glm::vec3(-77.0f, 2.85f, -38.1f), glm::vec3(12.0f, 0.22f, 0.12f));
    add_lamp("r2_wall_lamp_south", glm::vec3(-77.0f, 2.85f, -19.9f), glm::vec3(12.0f, 0.22f, 0.12f));

    // === Left wing ceilings ===
    add_box("left_junc_ceiling",   glm::vec3(-50.5f, 4.05f, -18.0f), glm::vec3(13.0f, 0.1f, 32.0f), tex_metal_plate,  false, 1.5f);
    add_box("r1_corr_ceiling",     glm::vec3(-62.0f, 4.05f,  -5.0f), glm::vec3(10.0f, 0.1f,  6.0f), tex_metal_plate,  false, 1.5f);
    add_box("reactor1_ceiling",    glm::vec3(-77.0f, 4.05f,  -3.0f), glm::vec3(20.0f, 0.1f, 22.0f), tex_metal_plate,  false, 1.5f);
    add_box("r2_corr_ceiling",     glm::vec3(-62.0f, 4.05f, -29.0f), glm::vec3(10.0f, 0.1f,  6.0f), tex_metal_panel,  false, 1.5f);
    add_box("reactor2_ceiling",    glm::vec3(-77.0f, 4.05f, -29.0f), glm::vec3(20.0f, 0.1f, 18.0f), tex_metal_panel,  false, 1.5f);
    // East Wing ceilings — different textures for industrial look
    add_box("east_corr_ceiling",  glm::vec3( 33.5f, 4.05f, -12.5f), glm::vec3(23.0f, 0.1f, 10.0f), tex_metal_plate,    false, 1.5f);
    add_box("warehouse_ceiling",  glm::vec3( 63.0f, 4.05f, -17.0f), glm::vec3(36.0f, 0.1f, 46.0f), tex_corrugated_iron, false, 1.5f);
    add_box("reactor3_ceiling",   glm::vec3( 89.0f, 4.05f, -24.0f), glm::vec3(14.0f, 0.1f, 32.0f), tex_sci_floor,       false, 1.5f);

    // === Reactor 1 room furniture — industrial lab with machines and server racks ===
    // (x=-67 to -87, z=-14 to +8, reactor at (-77,0.8,-3), button at (-71,0.55,-3))
    // Server racks against west wall
    add_obj("r1_server_a",   "computer-system.obj", glm::vec3(-86.5f, 0.0f, -10.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_metal_panel)->collides = true;
    add_obj("r1_server_b",   "computer-system.obj", glm::vec3(-86.5f, 0.0f,  -7.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_metal_panel)->collides = true;
    add_obj("r1_screen",     "screen-flat.obj",     glm::vec3(-86.5f, 0.0f,  -4.0f), glm::vec3(0,  90, 0), glm::vec3(2.0f), tex_terminal)->collides = true;
    // Work table + chair in SE corner (clear of reactor/button)
    add_obj("r1_table",      "table.obj",           glm::vec3(-73.0f, 0.0f,   5.5f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_metal_plate)->collides = true;
    add_obj("r1_chair_a",    "chair.obj",           glm::vec3(-70.5f, 0.0f,   5.5f), glm::vec3(0, 180, 0), glm::vec3(1.8f), tex_dark)->collides = true;
    // Industrial machine in NW corner
    add_obj("r1_machine",    "machine.obj",         glm::vec3(-85.5f, 0.0f, -12.0f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_corrugated_iron)->collides = true;
    // Containers along north wall
    add_obj("r1_cont_a",     "container.obj",       glm::vec3(-82.0f, 0.0f, -12.5f), glm::vec3(0,  90, 0), glm::vec3(1.8f), tex_corrugated_iron)->collides = true;
    add_obj("r1_cont_b",     "container.obj",       glm::vec3(-79.0f, 0.0f, -12.5f), glm::vec3(0,  90, 0), glm::vec3(1.8f), tex_metal_plate_02)->collides = true;
    // Hopper in south corner (away from button)
    add_obj("r1_hopper",     "hopper-square.obj",   glm::vec3(-85.0f, 0.0f,   6.5f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_rusty_metal)->collides = true;

    // === Reactor 2 room furniture — monitoring/control room style ===
    // (x=-67 to -87, z=-38 to -20, reactor at (-77,0.8,-29), button at (-71,0.55,-29))
    // Wide display wall along west wall (monitoring station)
    add_obj("r2_screen_a",   "screen-flat.obj",     glm::vec3(-86.5f, 0.0f, -32.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_terminal)->collides = true;
    add_obj("r2_screen_b",   "screen-flat.obj",     glm::vec3(-86.5f, 0.0f, -28.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_terminal)->collides = true;
    add_obj("r2_computer",   "computer.obj",        glm::vec3(-86.5f, 0.0f, -25.0f), glm::vec3(0,  90, 0), glm::vec3(2.0f), tex_metal_panel)->collides = true;
    // Central monitoring table with two chairs
    add_obj("r2_table",      "table-large.obj",     glm::vec3(-79.0f, 0.0f, -29.0f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_sci_floor)->collides = true;
    add_obj("r2_chair_a",    "chair.obj",           glm::vec3(-76.0f, 0.0f, -27.5f), glm::vec3(0, 135, 0), glm::vec3(1.8f), tex_dark)->collides = true;
    add_obj("r2_chair_b",    "chair.obj",           glm::vec3(-76.0f, 0.0f, -30.5f), glm::vec3(0, 225, 0), glm::vec3(1.8f), tex_dark)->collides = true;
    // Storage boxes in NE corner (near north wall)
    add_obj("r2_box_a",      "box-large.obj",       glm::vec3(-70.0f, 0.0f, -36.5f), glm::vec3(0,  20, 0), glm::vec3(1.8f), tex_box)->collides = true;
    add_obj("r2_box_b",      "box-small.obj",       glm::vec3(-68.5f, 0.0f, -35.0f), glm::vec3(0, -30, 0), glm::vec3(1.6f), tex_box)->collides = true;
    // Industrial container against south wall
    add_obj("r2_cont",       "container-tall.obj",  glm::vec3(-84.0f, 0.0f, -21.5f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_metal_plate_02)->collides = true;

    // === Right wing: east corridor → large warehouse → Reactor 3 behind hidden door ===
    // Hex cap bridging tunnel end to warehouse west wall
    constexpr float east_junc_cap_len = 1.6f;
    add_hex_tunnel_shell("hub_tunnel_cap_east",
        glm::vec3(hub_portal_panel_radius + east_tunnel_len + east_junc_cap_len * 0.5f, hub_tunnel_y, hub_center.z),
        east_junc_cap_len, 0.0f);

    // Warehouse room (x=45 to 81, z=-40 to 6)
    add_box("warehouse_north_wall",  glm::vec3( 63.0f, 2.0f, -40.5f), glm::vec3(36.0f, 4.0f, 0.8f), tex_metal_panel, true, 1.5f);
    add_box("warehouse_south_wall",  glm::vec3( 63.0f, 2.0f,   6.5f), glm::vec3(36.0f, 4.0f, 0.8f), tex_metal_panel, true, 1.5f);
    // West wall opening aligned with east tunnel (z=-14.14 to -10.86)
    add_box("warehouse_west_n",      glm::vec3( 44.5f, 2.0f, -27.32f), glm::vec3(0.8f, 4.0f, 26.36f), tex_metal_panel, true, 1.5f);
    add_box("warehouse_west_s",      glm::vec3( 44.5f, 2.0f,  -2.18f), glm::vec3(0.8f, 4.0f, 17.36f), tex_metal_panel, true, 1.5f);
    add_box("warehouse_east_n",      glm::vec3( 81.5f, 2.0f, -34.0f), glm::vec3(0.8f, 4.0f, 12.0f), tex_metal_panel, true, 1.5f);
    add_box("warehouse_east_s",      glm::vec3( 81.5f, 2.0f,  -7.0f), glm::vec3(0.8f, 4.0f, 26.0f), tex_metal_panel, true, 1.5f);
    hidden_door_wall = add_box("warehouse_door_panel", glm::vec3( 81.5f, 2.0f, -24.0f), glm::vec3(0.8f, 4.0f, 8.0f), tex_metal_plate, true, 1.5f);
    hidden_door_wall->emissive_color = glm::vec3(0.04f, 0.04f, 0.05f);
    // Warning stripes at top and bottom of door
    auto tex_warn = std::make_shared<Texture>(glm::vec3(0.72f, 0.55f, 0.02f));
    add_box("door_stripe_top",    glm::vec3( 81.5f, 3.85f, -24.0f), glm::vec3(0.85f, 0.22f, 8.0f), tex_warn, false);
    add_box("door_stripe_bottom", glm::vec3( 81.5f, 0.11f, -24.0f), glm::vec3(0.85f, 0.22f, 8.0f), tex_warn, false);
    // Hidden door button — výrazné oranžové tlačítko s podestičkou
    auto tex_btn_orange = std::make_shared<Texture>(glm::vec3(0.90f, 0.35f, 0.03f)); // bright orange
    auto tex_btn_plate  = std::make_shared<Texture>(glm::vec3(0.18f, 0.18f, 0.22f)); // dark plate
    add_box("warehouse_btn_plate", glm::vec3( 78.0f, 0.02f,-38.0f), glm::vec3(1.8f, 0.04f, 1.8f), tex_btn_plate, false);
    hidden_door_btn = add_box("warehouse_secret_btn", glm::vec3( 78.0f, 0.60f,-38.0f), glm::vec3(0.9f, 0.45f, 0.9f), tex_btn_orange, true, 0.6f);
    hidden_door_btn->emissive_color = glm::vec3(0.6f, 0.2f, 0.0f);

    // Crates inside warehouse
    add_box("warehouse_crate_a",     glm::vec3( 55.0f, 0.8f, -30.0f), glm::vec3(3.4f, 1.6f, 3.4f), tex_box, true, 1.6f);
    add_box("warehouse_crate_b",     glm::vec3( 68.0f, 0.8f,  -5.0f), glm::vec3(4.2f, 1.6f, 3.0f), tex_box, true, 1.6f);
    add_box("warehouse_crate_c",     glm::vec3( 72.0f, 1.5f, -32.0f), glm::vec3(2.4f, 3.0f, 2.4f), tex_box, true, 1.5f);
    add_box("warehouse_terminal_l",  glm::vec3( 50.0f, 1.0f, -10.0f), glm::vec3(2.4f, 2.0f, 2.5f), tex_dark, true, 1.4f);
    add_box("warehouse_terminal_r",  glm::vec3( 75.0f, 1.0f, -35.0f), glm::vec3(2.4f, 2.0f, 2.5f), tex_dark, true, 1.4f);
    // === East Wing OBJ props ===
    // Sklad — průmyslové rekvizity, vše podél zdí nebo ve volném prostoru
    add_obj("wh_crane",       "crane.obj",          glm::vec3( 62.0f, 0.0f, -22.0f), glm::vec3(0,  45,0), glm::vec3(2.4f), tex_rusty_metal)->collides = true;
    add_obj("wh_cont_n1",     "container-tall.obj", glm::vec3( 51.0f, 0.0f, -39.0f), glm::vec3(0,   0,0), glm::vec3(2.0f), tex_corrugated_iron)->collides = true;
    add_obj("wh_cont_n2",     "container-tall.obj", glm::vec3( 60.0f, 0.0f, -39.0f), glm::vec3(0,  90,0), glm::vec3(2.0f), tex_metal_plate_02)->collides = true;
    add_obj("wh_cont_n3",     "container.obj",      glm::vec3( 70.0f, 0.0f, -39.0f), glm::vec3(0,  90,0), glm::vec3(1.9f), tex_corrugated_iron)->collides = true;
    add_obj("wh_cont_s1",     "container.obj",      glm::vec3( 55.0f, 0.0f,   5.0f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_metal_plate)->collides = true;
    add_obj("wh_machine_w",   "machine.obj",        glm::vec3( 47.0f, 0.0f, -26.0f), glm::vec3(0,  90,0), glm::vec3(2.0f), tex_rusty_metal)->collides = true;
    add_obj("wh_hopper",      "hopper-square.obj",  glm::vec3( 57.0f, 0.0f,  -4.0f), glm::vec3(0,   0,0), glm::vec3(2.0f), tex_rusty_metal)->collides = true;
    add_obj("wh_box_a",       "box-large.obj",      glm::vec3( 76.0f, 0.0f, -14.0f), glm::vec3(0,  25,0), glm::vec3(1.8f), tex_box)->collides = true;
    add_obj("wh_box_b",       "box-small.obj",      glm::vec3( 77.5f, 0.0f, -15.5f), glm::vec3(0, -10,0), glm::vec3(1.6f), tex_box)->collides = true;
    // Potrubí pouze podél severní zdi skladu (Z≈-39)
    add_obj("wh_pipe_n1",     "pipe-large.obj",     glm::vec3( 47.0f, 0.0f, -39.2f), glm::vec3(0,  90,0), glm::vec3(2.0f), tex_rusty_metal)->collides = true;
    add_obj("wh_pipe_n2",     "pipe-large.obj",     glm::vec3( 55.0f, 0.0f, -39.2f), glm::vec3(0,  90,0), glm::vec3(2.0f), tex_rusty_metal)->collides = true;
    add_obj("wh_pipe_ring_w", "pipe-ring.obj",      glm::vec3( 45.2f, 1.5f, -20.0f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_metal_plate)->collides = true;
    // Chodba — potrubí jen podél jižní stěny (Z≈-17)
    add_obj("corr_pipe_s",    "pipe.obj",           glm::vec3( 30.0f, 0.0f, -17.0f), glm::vec3(0,  90,0), glm::vec3(1.6f), tex_rusty_metal)->collides = true;
    // Reaktor 3 — servery a vybavení podél východní stěny (X≈96)
    add_obj("r3_server_a",    "computer-system.obj",glm::vec3( 95.5f, 0.0f, -30.0f), glm::vec3(0, -90,0), glm::vec3(2.2f), tex_metal_panel)->collides = true;
    add_obj("r3_server_b",    "computer-system.obj",glm::vec3( 95.5f, 0.0f, -20.0f), glm::vec3(0, -90,0), glm::vec3(2.2f), tex_metal_panel)->collides = true;
    add_obj("r3_screen",      "screen-flat.obj",    glm::vec3( 95.5f, 0.0f, -25.0f), glm::vec3(0, -90,0), glm::vec3(2.0f), tex_terminal)->collides = true;
    add_obj("r3_machine",     "machine.obj",        glm::vec3( 92.0f, 0.0f, -38.0f), glm::vec3(0,   0,0), glm::vec3(2.0f), tex_corrugated_iron)->collides = true;
    add_obj("r3_pipe_e1",     "pipe.obj",           glm::vec3( 96.0f, 0.0f, -35.0f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_rusty_metal)->collides = true;
    add_obj("r3_pipe_e2",     "pipe.obj",           glm::vec3( 96.0f, 0.0f, -15.0f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_rusty_metal)->collides = true;

    // === Warehouse SE kancelářský cluster (Z: 0-4, X: 63-79 — mimo enemy) ===
    add_obj("wh_desk_a",      "table-large.obj",    glm::vec3( 67.0f, 0.0f,  2.0f), glm::vec3(0,   0,0), glm::vec3(2.0f), tex_metal_plate)->collides = true;
    add_obj("wh_desk_chair_a","chair.obj",           glm::vec3( 64.0f, 0.0f,  1.5f), glm::vec3(0, 180,0), glm::vec3(1.8f), tex_dark)->collides = true;
    add_obj("wh_screen_se",   "computer-screen.obj",glm::vec3( 71.0f, 0.0f,  1.0f), glm::vec3(0,  90,0), glm::vec3(2.0f), tex_terminal)->collides = true;
    add_obj("wh_computer_se", "computer.obj",       glm::vec3( 75.5f, 0.0f,  0.5f), glm::vec3(0,  90,0), glm::vec3(1.8f), tex_metal_panel)->collides = true;
    add_obj("wh_chair_se2",   "chair.obj",           glm::vec3( 75.5f, 0.0f,  3.0f), glm::vec3(0, 180,0), glm::vec3(1.8f), tex_dark)->collides = true;

    // === Warehouse NW monitorovací stanice (X: 47-54, Z: -33 to -36 — mimo ew1 na -22) ===
    add_obj("wh_table_nw",    "table.obj",           glm::vec3( 50.0f, 0.0f,-34.0f), glm::vec3(0,   0,0), glm::vec3(2.0f), tex_metal_plate_02)->collides = true;
    add_obj("wh_comp_nw",     "computer-system.obj", glm::vec3( 47.5f, 0.0f,-34.5f), glm::vec3(0,  90,0), glm::vec3(2.0f), tex_metal_panel)->collides = true;
    add_obj("wh_chair_nw",    "chair.obj",           glm::vec3( 52.0f, 0.0f,-33.0f), glm::vec3(0, 180,0), glm::vec3(1.8f), tex_dark)->collides = true;

    // === East corridor konzole (X: 35-42, Z: -10 to -16) ===
    add_obj("corr_computer",  "computer.obj",        glm::vec3( 36.0f, 0.0f,-16.5f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_metal_panel)->collides = true;
    add_obj("corr_screen",    "computer-screen.obj", glm::vec3( 42.0f, 0.0f,-16.5f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_terminal)->collides = true;
    add_obj("corr_chair",     "chair.obj",           glm::vec3( 38.0f, 0.0f,-13.5f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_dark)->collides = true;

    // === Reaktor 3 pracovní stůl (X: 83-85, Z: -29 to -32 — mimo ew7 na (86,-24)) ===
    add_obj("r3_table",       "table-large.obj",     glm::vec3( 83.5f, 0.0f,-31.0f), glm::vec3(0,   0,0), glm::vec3(2.0f), tex_sci_floor)->collides = true;
    add_obj("r3_chair",       "chair.obj",           glm::vec3( 83.5f, 0.0f,-28.0f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_dark)->collides = true;
    add_obj("r3_comp",        "computer.obj",        glm::vec3( 83.5f, 0.0f,-34.0f), glm::vec3(0,   0,0), glm::vec3(1.8f), tex_metal_panel)->collides = true;

    // Reactor 3 room (x=81 to 96, z=-40 to -8) — west wall is shared with warehouse east wall above
    add_box("reactor3_room_north",   glm::vec3( 88.5f, 2.0f, -40.5f), glm::vec3(14.0f, 4.0f, 0.8f), tex_metal_panel, true, 1.5f);
    add_box("reactor3_room_south",   glm::vec3( 88.5f, 2.0f,  -7.5f), glm::vec3(14.0f, 4.0f, 0.8f), tex_metal_panel, true, 1.5f);
    add_box("reactor3_room_east",    glm::vec3( 96.5f, 2.0f, -24.0f), glm::vec3(0.8f, 4.0f, 32.0f), tex_metal_panel, true, 1.5f);

    add_box("gate_frame_left", glm::vec3(-5.6f, 2.0f, -39.0f), glm::vec3(1.0f, 4.0f, 2.4f), tex_dark, true, 1.5f);
    add_box("gate_frame_right", glm::vec3(5.6f, 2.0f, -39.0f), glm::vec3(1.0f, 4.0f, 2.4f), tex_dark, true, 1.5f);

    auto tex_pedestal = std::make_shared<Texture>(glm::vec3(0.11f, 0.13f, 0.16f));
    auto hub_pedestal = std::make_shared<Model>("objects/oct_pedestal.obj", shader_prog, tex_pedestal);
    hub_pedestal->pivot_position = hub_center;
    scene["hub_orb_pedestal"] = hub_pedestal;
    {
        const std::array<glm::vec2, 8> pedestal_outline = {
            glm::vec2( 2.586864f, -1.071513f),
            glm::vec2( 2.586864f,  1.071513f),
            glm::vec2( 1.071513f,  2.586864f),
            glm::vec2(-1.071513f,  2.586864f),
            glm::vec2(-2.586864f,  1.071513f),
            glm::vec2(-2.586864f, -1.071513f),
            glm::vec2(-1.071513f, -2.586864f),
            glm::vec2( 1.071513f, -2.586864f)
        };

        for (size_t i = 0; i < pedestal_outline.size(); ++i) {
            const glm::vec2 a = pedestal_outline[i];
            const glm::vec2 b = pedestal_outline[(i + 1) % pedestal_outline.size()];
            const glm::vec2 mid = (a + b) * 0.5f;
            const glm::vec2 edge = b - a;
            auto collision = add_box("hub_orb_pedestal_collision_" + std::to_string(i),
                                     glm::vec3(hub_center.x + mid.x, 0.35f, hub_center.z + mid.y),
                                     glm::vec3(glm::length(edge), 0.7f, 0.38f),
                                     tex_collision_clear, true, 1.0f, true, 0.0f);
            collision->eulerAngles.y = -glm::degrees(std::atan2(edge.y, edge.x));
        }

        add_oct_floor_collision("hub_orb_pedestal_top_collision",
                                glm::vec3(hub_center.x, 0.34f, hub_center.z),
                                2.8f,
                                0.68f);
    }

    add_box("glass_warning_left",  glm::vec3(-20.0f, 2.4f, -12.5f), glm::vec3(0.18f, 4.0f, 7.0f), tex_red_glass,  false, 1.0f, true, 0.35f);
    add_box("glass_warning_right", glm::vec3( 20.0f, 2.4f, -12.5f), glm::vec3(0.18f, 4.0f, 7.0f), tex_blue_glass, false, 1.0f, true, 0.35f);

    reactors.push_back({
        add_box("reactor_01",        glm::vec3(-77.0f, 0.8f,  -3.0f), glm::vec3(1.4f, 2.0f, 1.4f), tex_red_glass,   true, 1.0f, true, 0.55f),
        add_box("reactor_01_button", glm::vec3(-71.0f, 0.55f, -3.0f), glm::vec3(0.8f, 0.35f, 0.8f), tex_terminal,   true, 0.6f),
        glm::vec3(1.0f, 0.16f, 0.10f)
    });
    reactors.push_back({
        add_box("reactor_02",        glm::vec3(-77.0f, 0.8f, -29.0f), glm::vec3(1.4f, 2.0f, 1.4f), tex_blue_glass,  true, 1.0f, true, 0.55f),
        add_box("reactor_02_button", glm::vec3(-71.0f, 0.55f,-29.0f), glm::vec3(0.8f, 0.35f, 0.8f), tex_terminal,   true, 0.6f),
        glm::vec3(0.15f, 0.45f, 1.0f)
    });
    reactors.push_back({
        add_box("reactor_03_hidden", glm::vec3( 89.0f, 0.8f, -24.0f), glm::vec3(1.4f, 2.0f, 1.4f), tex_green_glass, true, 1.0f, true, 0.55f),
        add_box("reactor_03_button", glm::vec3( 84.0f, 0.55f,-24.0f), glm::vec3(0.8f, 0.35f, 0.8f), tex_terminal,   true, 0.6f),
        glm::vec3(0.25f, 1.0f, 0.30f)
    });

    gate_model = add_box("containment_gate", glm::vec3(0.0f, 1.8f, -39.2f), glm::vec3(7.5f, 3.6f, 0.8f), tex_dark, true, 2.5f);

    // Reactor enemies — orc OBJ model
    auto make_reactor_enemy = [&](const std::string& name, glm::vec3 pos, float bob_off) -> Enemy {
        // Enemies share the same model/texture. Spawn data is captured below so
        // reset_game_world() can revive them without loading assets again.
        auto m = std::make_shared<Model>("objects/orc_solid.obj", shader_prog, tex_orc);
        m->pivot_position = pos;
        m->scale = glm::vec3(74.0f);
        m->two_sided_lighting = true;
        m->bounding_radius = m->get_cull_radius();
        scene[name] = m;
        Enemy e{ m, 3, bob_off };
        e.y_base = 0.22f;
        return e;
    };
    // Reactor 1 — spread across open floor, away from servers/containers
    Enemy e5  = make_reactor_enemy("enemy_05", glm::vec3(-78.5f, 0.0f,  -5.0f), 0.8f);
    Enemy e6  = make_reactor_enemy("enemy_06", glm::vec3(-74.0f, 0.0f,   1.5f), 2.5f);
    Enemy e9  = make_reactor_enemy("enemy_09", glm::vec3(-83.0f, 0.0f,  -2.5f), 1.4f);
    Enemy e10 = make_reactor_enemy("enemy_10", glm::vec3(-75.5f, 0.0f,  -9.0f), 3.1f);
    // Reactor 2 — spread across open floor, away from table/boxes/screens
    Enemy e7  = make_reactor_enemy("enemy_07", glm::vec3(-80.5f, 0.0f, -30.5f), 4.2f);
    Enemy e8  = make_reactor_enemy("enemy_08", glm::vec3(-74.0f, 0.0f, -24.0f), 6.0f);
    Enemy e11 = make_reactor_enemy("enemy_11", glm::vec3(-77.5f, 0.0f, -34.0f), 0.5f);
    Enemy e12 = make_reactor_enemy("enemy_12", glm::vec3(-73.0f, 0.0f, -31.5f), 2.2f);
    // T-junction — 2 enemies visible from tunnel entrance
    Enemy e13 = make_reactor_enemy("enemy_13", glm::vec3(-50.0f, 0.0f, -25.0f), 3.8f);
    Enemy e14 = make_reactor_enemy("enemy_14", glm::vec3(-50.0f, 0.0f,  -7.0f), 5.5f);
    // East Wing — sklad (4 orci) + reaktor3 (3 orci), pozice mimo objekty
    // Sklad: crane(62,-22), crate_a(55,-30), crate_c(72,-32), terminal_l(50,-10), hopper(57,-4)
    Enemy e3  = make_reactor_enemy("enemy_03",  glm::vec3( 65.0f, 0.0f, -15.0f), 3.4f); // center warehouse
    Enemy ew1 = make_reactor_enemy("enemy_ew1", glm::vec3( 53.0f, 0.0f, -22.0f), 1.1f); // NW open area
    Enemy ew2 = make_reactor_enemy("enemy_ew2", glm::vec3( 69.0f, 0.0f,  -9.0f), 2.8f); // SE open area
    Enemy ew3 = make_reactor_enemy("enemy_ew3", glm::vec3( 49.0f, 0.0f,  -8.0f), 4.5f); // SW open area
    Enemy ew4 = make_reactor_enemy("enemy_ew4", glm::vec3( 74.0f, 0.0f, -27.0f), 6.2f); // NE open area
    // Reaktor3: machine(92,-38), server_a(95.5,-30), server_b(95.5,-20), reactor(89,-24)
    Enemy ew5 = make_reactor_enemy("enemy_ew5", glm::vec3( 86.0f, 0.0f, -36.0f), 0.7f); // NW reactor3
    Enemy ew6 = make_reactor_enemy("enemy_ew6", glm::vec3( 90.0f, 0.0f, -14.0f), 3.3f); // S reactor3
    Enemy ew7 = make_reactor_enemy("enemy_ew7", glm::vec3( 86.0f, 0.0f, -24.0f), 5.0f); // center reactor3
    enemies = { e3, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14,
                ew1, ew2, ew3, ew4, ew5, ew6, ew7 };

    // Store spawn snapshots so reset_game_world() can restore enemies without
    // reconstructing models or reloading textures.
    for (auto& enemy : enemies) {
        enemy.max_health = enemy.health;
        if (!enemy.model) {
            continue;
        }
        enemy.spawn_position = enemy.model->pivot_position;
        enemy.spawn_euler = enemy.model->eulerAngles;
        enemy.spawn_scale = enemy.model->scale;
        enemy.spawn_collides = enemy.model->collides;
        enemy.spawn_transparent = enemy.model->is_transparent;
        enemy.spawn_alpha = enemy.model->material_alpha;
    }

    // Transparent orb cluster in the hub. Its shells are registered later for
    // the OIT pass instead of ordinary transparent sorting.
    model = add_box("levitating_orb", glm::vec3(0.0f, 3.7f, -12.5f), glm::vec3(2.6f), tex_terminal, false, 1.0f, true, 0.72f);
    model->emissive_color = glm::vec3(0.02f, 0.09f, 0.08f);
    model->two_sided_lighting = true;
    inner_orb_model = add_box("levitating_orb_inner", glm::vec3(0.0f, 3.7f, -12.5f), glm::vec3(1.18f), tex_inner_orb, false, 1.0f, true, 0.34f);
    inner_orb_model->emissive_color = glm::vec3(0.34f, 0.12f, 0.02f);
    inner_orb_model->two_sided_lighting = true;
    orb_layer_models.push_back(inner_orb_model);
    struct OrbLayerSpec {
        const char* name;
        glm::vec3 scale;
        std::shared_ptr<Texture> texture;
        glm::vec3 emissive;
        float alpha;
    };
    const std::array<OrbLayerSpec, 1> orb_layers = {
        OrbLayerSpec{ "levitating_orb_layer_blue", glm::vec3(0.59f), tex_orb_blue, glm::vec3(0.04f, 0.10f, 0.28f), 0.28f },
    };
    for (const auto& layer : orb_layers) {
        auto layer_model = add_box(layer.name, glm::vec3(0.0f, 3.7f, -12.5f), layer.scale, layer.texture, false, 1.0f, true, layer.alpha);
        layer_model->emissive_color = layer.emissive;
        layer_model->two_sided_lighting = true;
        orb_layer_models.push_back(layer_model);
    }
    // fire_sources = {
    //     glm::vec3( -7.0f, 0.1f,  -9.0f),
    //     glm::vec3(  7.0f, 0.1f, -16.0f),
    //     glm::vec3(-50.0f, 0.1f,  -5.0f),
    //     glm::vec3(-50.0f, 0.1f, -29.0f),
    //     glm::vec3( 58.0f, 0.1f, -32.0f),
    //     glm::vec3( 70.0f, 0.1f,  -5.0f),
    //     glm::vec3(  0.0f, 0.1f, -31.0f),
    //     glm::vec3( 89.0f, 0.1f, -18.0f)
    // };

    auto tex_particle = std::make_shared<Texture>(glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));

    // One small model is reused for every live particle by changing its
    // transform before each draw call in app_render.cpp.
    particle_template = std::make_shared<Model>("objects/tetrahedron.obj", shader_prog, tex_particle);
    particle_template->scale = glm::vec3(0.1f);
    particle_template->is_transparent = true;
    particle_template->material_alpha = 0.8f;

    // Build fast lookup lists from the scene map:
    // scene_colliders is used by movement/enemy collision, orb_model_set by render sorting.
    scene_colliders.clear();
    for (auto& [name, obj] : scene)
        if (obj && obj->collides)
            scene_colliders.push_back(obj);

    orb_model_set.clear();
    if (model)           orb_model_set.insert(model.get());
    if (inner_orb_model) orb_model_set.insert(inner_orb_model.get());
    for (auto& m : orb_layer_models) if (m) orb_model_set.insert(m.get());

    // Pre-size render lists to avoid first-frame realloc
    render_opaque.reserve(scene.size());
    render_transparent.reserve(32);
    render_oit_orbs.reserve(orb_model_set.size() + 4);

    // Trigger zones — message fires once when player enters the radius
    trigger_zones = {
        { glm::vec3(  0.0f, 2.0f,  0.0f), 4.0f, "Containment Zone",  5.5f },
        { glm::vec3(-30.0f, 2.0f, -12.5f), 3.0f, "West Wing",         5.5f },
        { glm::vec3( 30.0f, 2.0f, -12.5f), 3.0f, "East Wing",         5.5f },
        { glm::vec3(  0.0f, 2.0f, -25.0f), 3.0f, "North Wing",        5.5f },
    };

    set_hud_message("GOAL: activate all reactors.");
}

// Convenience constructor for cube-based scene objects.
// Most floors, walls, rails, invisible colliders, and simple props go through
// this helper so they are consistently added to the scene map.
std::shared_ptr<Model> App::add_box(const std::string& name,
                                    const glm::vec3& position,
                                    const glm::vec3& scale,
                                    const std::shared_ptr<Texture>& texture,
                                    bool collides,
                                    float radius,
                                    bool transparent,
                                    float alpha)
{
    auto box = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, texture);
    box->pivot_position = position;
    box->scale = scale;
    box->collides = collides;
    box->bounding_radius = radius;
    box->is_transparent = transparent;
    box->material_alpha = alpha;
    scene[name] = box;
    return box;
}
