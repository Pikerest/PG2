/*
 * Camera and world collision handling.
 * Resolves player movement against scene colliders and provides helper
 * tests for the hub pit and safe walkways.
 */

#include <algorithm>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include "app.hpp"

// Main collision pass for the player camera.
// Clamps the player to map bounds, resolves floor support, then pushes the
// camera out of every active scene collider.
void App::apply_collisions(float delta_t) {
	(void)delta_t;

	if (!collisions_enabled) {
		return;
	}

    camera.Position.x = std::clamp(camera.Position.x, MAP_MIN_X, MAP_MAX_X);
    camera.Position.z = std::clamp(camera.Position.z, MAP_MIN_Z, MAP_MAX_Z);
    camera.Position.y = current_camera_eye_y();

    const float CAMERA_RADIUS = 0.4f;
    bool supported = player_vertical_offset <= 0.02f && (!is_over_hub_pit() || is_over_hub_walkway());

    for (auto& obj : scene_colliders) {
        if (!obj || !obj->collides) {
            continue;
        }
        if (try_resolve_camera_top_collision(obj, CAMERA_RADIUS)) {
            supported = true;
            continue;
        }
        resolve_camera_box_collision(obj, CAMERA_RADIUS);
    }

    if (player_on_ground && player_vertical_offset > 0.02f && !supported) {
        player_on_ground = false;
        player_vertical_velocity = -0.1f;
    }
}

// Pushes the camera out of the side of an oriented box collider.
// Only X/Z movement is corrected here; top/floor support is handled separately.
void App::resolve_camera_box_collision(const std::shared_ptr<Model>& obj, float camera_radius)
{
    const glm::vec3 half_extents = obj->scale * 0.5f + glm::vec3(camera_radius, 0.0f, camera_radius);
    const glm::vec3 world_delta = camera.Position - obj->pivot_position;
    const float yaw = glm::radians(obj->eulerAngles.y);
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const glm::vec3 delta(
        c * world_delta.x - s * world_delta.z,
        world_delta.y,
        s * world_delta.x + c * world_delta.z
    );

    if (std::abs(delta.x) > half_extents.x ||
        std::abs(delta.y) > obj->scale.y * 0.5f + 1.0f ||
        std::abs(delta.z) > half_extents.z) {
        return;
    }

    const float overlap_x = half_extents.x - std::abs(delta.x);
    const float overlap_z = half_extents.z - std::abs(delta.z);

    if (overlap_x < overlap_z) {
        const float push = delta.x < 0.0f ? -overlap_x : overlap_x;
        camera.Position.x += c * push;
        camera.Position.z += -s * push;
    } else {
        const float push = delta.z < 0.0f ? -overlap_z : overlap_z;
        camera.Position.x += s * push;
        camera.Position.z += c * push;
    }

    camera.Position.x = std::clamp(camera.Position.x, MAP_MIN_X, MAP_MAX_X);
    camera.Position.z = std::clamp(camera.Position.z, MAP_MIN_Z, MAP_MAX_Z);
    camera.Position.y = current_camera_eye_y();
}

// Allows the player to stand on top of low colliders.
// Returns true when the camera feet are close enough to the collider top.
bool App::try_resolve_camera_top_collision(const std::shared_ptr<Model>& obj, float camera_radius)
{
    const glm::vec3 half_extents = obj->scale * 0.5f + glm::vec3(camera_radius, 0.0f, camera_radius);
    const glm::vec3 world_delta = camera.Position - obj->pivot_position;
    const float yaw = glm::radians(obj->eulerAngles.y);
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const glm::vec3 delta(
        c * world_delta.x - s * world_delta.z,
        world_delta.y,
        s * world_delta.x + c * world_delta.z
    );

    if (std::abs(delta.x) > half_extents.x || std::abs(delta.z) > half_extents.z) {
        return false;
    }

    const float feet_y = camera.Position.y - PLAYER_EYE_HEIGHT;
    const float top_y = obj->pivot_position.y + obj->scale.y * 0.5f;
    const bool close_to_top = feet_y >= top_y - 0.55f && feet_y <= top_y + 0.35f;

    if (!close_to_top || player_vertical_velocity > 0.0f) {
        return false;
    }

    player_vertical_offset = std::max(0.0f, top_y);
    player_vertical_velocity = 0.0f;
    player_on_ground = true;
    camera.Position.y = current_camera_eye_y();
    return true;
}

// Checks whether the camera is horizontally inside the hub pit footprint.
bool App::is_over_hub_pit() const
{
    const glm::vec2 p(camera.Position.x, camera.Position.z + 12.5f);
    return std::abs(p.x) <= 24.0f && std::abs(p.y) <= 24.0f;
}

// Checks whether the player is on one of the safe hub walkways/platforms
// instead of over the open pit void.
bool App::is_over_hub_walkway() const
{
    const glm::vec2 p(camera.Position.x, camera.Position.z + 12.5f);
    const bool center_platform = std::abs(p.x) <= 6.3f  && std::abs(p.y) <= 6.3f;
    const bool orb_pedestal    = std::abs(p.x) <= 3.15f && std::abs(p.y) <= 3.15f;
    const bool west_catwalk    = p.x >= -23.7f && p.x <= -5.65f && std::abs(p.y) <= 1.55f;
    const bool east_catwalk    = p.x >=   5.65f && p.x <=  23.7f && std::abs(p.y) <= 1.55f;
    const bool south_catwalk   = p.y >=   5.65f && p.y <=  23.7f && std::abs(p.x) <= 1.55f;
    const bool north_catwalk   = p.y >= -23.7f && p.y <=  -5.65f && std::abs(p.x) <= 1.55f;
    return center_platform || orb_pedestal || west_catwalk || east_catwalk || south_catwalk || north_catwalk;
}
