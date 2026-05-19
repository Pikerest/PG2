/*
 * Runtime gameplay state updates.
 * Handles player movement/gravity, reactors, enemies, doors, weapon hits,
 * respawn/reset logic, and game-state transitions.
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <array>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include "app.hpp"
#include "audio.hpp"

// Per-frame gameplay update while the game is running.
// Animates reactors/lights, moves enemies, handles attacks, opens doors,
// updates interaction hints, particles, and trigger-zone text.
void App::update_gameplay(float delta_t, double now)
{
	// Active reactors spin and pulse so the player gets immediate feedback.
	for (size_t i = 0; i < reactors.size(); ++i) {
		auto& reactor = reactors[i];
		if (reactor.active) {
			reactor.model->eulerAngles.y += delta_t * 120.0f;
			reactor.model->material_alpha = 0.78f + 0.18f * std::sin(static_cast<float>(now) * 10.0f);
			if (reactor.button) {
				reactor.button->eulerAngles.y += delta_t * 180.0f;
				reactor.button->material_alpha = 1.0f;
			}
		}
	}

    // Hub lighting is intentionally much dimmer until every reactor is active.
    const bool hub_powered = reactors_active >= static_cast<int>(reactors.size());
    const float light_power     = hub_powered ? 1.55f : 0.34f;
    const float top_light_power = hub_powered ? 2.05f : 0.48f;

    // Index 5 is the orb light and is overridden below. Keeping it in this
    // fixed profile array makes the first six point light slots easy to reason
    // about when comparing with app_assets.cpp.
    const std::array<PointLight, 6> hub_light_profiles = {
        PointLight{ glm::vec3(  0.0f, 14.6f, -12.5f), glm::vec3(0.040f, 0.052f, 0.050f), glm::vec3(1.05f, 1.45f, 1.30f), glm::vec3(0.14f, 0.22f, 0.20f), 56.0f },
        PointLight{ glm::vec3(-21.5f,  4.8f, -12.5f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f },
        PointLight{ glm::vec3( 21.5f,  4.8f, -12.5f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f },
        PointLight{ glm::vec3(  0.0f,  4.8f,   9.0f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f },
        PointLight{ glm::vec3(  0.0f,  4.8f, -34.0f), glm::vec3(0.018f, 0.030f, 0.030f), glm::vec3(0.42f, 0.78f, 0.70f), glm::vec3(0.06f, 0.12f, 0.11f), 28.0f },
    };

    for (size_t i = 0; i < point_lights.size() && i < hub_light_profiles.size(); ++i) {
        const PointLight& profile = hub_light_profiles[i];
        const float power = i == 0 ? top_light_power : light_power;
        point_lights[i].position = profile.position;
        point_lights[i].ambient  = profile.ambient  * power;
        point_lights[i].diffuse  = profile.diffuse  * power;
        point_lights[i].specular = profile.specular * power;
        point_lights[i].radius   = profile.radius;
    }

    // Reactor room light flicker (indices 6–17 = R1+R2 lamps, 30-35 = R3)
    if (point_lights.size() > 17) {
        const float t = static_cast<float>(now);

        // Sum several sine waves to make the lamps feel unstable without
        // requiring random state or per-light timers.
        const float r1 = std::max(0.05f, 0.50f + 0.45f * std::sin(t * 7.3f)
                                        + 0.25f * std::sin(t * 19.7f + 0.8f)
                                        + 0.10f * std::sin(t * 43.1f));
        const float r2 = std::max(0.05f, 0.50f + 0.45f * std::sin(t * 6.8f + 1.57f)
                                        + 0.25f * std::sin(t * 21.3f + 0.4f)
                                        + 0.10f * std::sin(t * 51.7f));
        const glm::vec3 ceil_base(1.20f, 1.09f, 0.91f);
        const glm::vec3 wall_base(1.05f, 0.95f, 0.78f);
        const glm::vec3 corr_base(0.77f, 0.67f, 0.56f);
        for (int i = 6;  i <= 7;  ++i) point_lights[i].diffuse = ceil_base * r1;
        for (int i = 8;  i <= 10; ++i) point_lights[i].diffuse = wall_base * r1;
        point_lights[11].diffuse = corr_base * r1;
        for (int i = 12; i <= 13; ++i) point_lights[i].diffuse = ceil_base * r2;
        for (int i = 14; i <= 16; ++i) point_lights[i].diffuse = wall_base * r2;
        point_lights[17].diffuse = corr_base * r2;
        if (point_lights.size() > 35) {
            const float r3 = std::max(0.05f, 0.50f + 0.45f * std::sin(t * 8.1f + 0.9f)
                                                    + 0.25f * std::sin(t * 22.5f + 0.3f)
                                                    + 0.10f * std::sin(t * 47.3f));
            const glm::vec3 r3_ceil(1.10f, 0.86f, 0.52f);
            const glm::vec3 r3_fill(0.90f, 0.70f, 0.43f);
            for (int i = 30; i <= 31; ++i) point_lights[i].diffuse = r3_ceil * r3;
            for (int i = 32; i <= 35; ++i) point_lights[i].diffuse = r3_fill * r3;
        }
    }

    // Orb light — bright while reactors are off, fades once hub is fully powered
    constexpr size_t ORB_LIGHT = 5;
    if (ORB_LIGHT < point_lights.size()) {
        const float orb_power = hub_powered ? 0.0f : 1.0f;
        point_lights[ORB_LIGHT].position = glm::vec3(0.0f, 3.7f, -12.5f);
        point_lights[ORB_LIGHT].ambient  = glm::vec3(0.05f, 0.14f, 0.13f) * orb_power;
        point_lights[ORB_LIGHT].diffuse  = glm::vec3(1.20f, 3.00f, 2.70f) * orb_power;
        point_lights[ORB_LIGHT].specular = glm::vec3(0.30f, 0.70f, 0.60f) * orb_power;
        point_lights[ORB_LIGHT].radius   = 14.0f;
    }

    const glm::vec3 lamp_dim(0.055f, 0.13f, 0.11f);
    const glm::vec3 lamp_bright(0.42f, 0.95f, 0.80f);
    const glm::vec3 top_lamp_dim(0.10f, 0.24f, 0.20f);
    const glm::vec3 top_lamp_bright(0.70f, 1.35f, 1.12f);
    for (const char* name : { "hub_lamp_west_portal", "hub_lamp_east_portal", "hub_lamp_south_portal", "hub_lamp_north_portal" }) {
        auto it = scene.find(name);
        if (it != scene.end() && it->second)
            it->second->emissive_color = hub_powered ? lamp_bright : lamp_dim;
    }
    if (auto it = scene.find("hub_lamp_top_hex"); it != scene.end() && it->second)
        it->second->emissive_color = hub_powered ? top_lamp_bright : top_lamp_dim;

	if (game_over) return;

	// Sphere-AABB collision for enemies — only blocks at same Y level
	auto enemy_blocked = [&](const glm::vec3& pos) -> bool {
		constexpr float ENEMY_R = 0.4f;
		for (const auto& col : scene_colliders) {
			if (!col || !col->collides) continue;
			const glm::vec3 half = col->scale * 0.5f;
			const glm::vec3 mn   = col->pivot_position - half;
			const glm::vec3 mx   = col->pivot_position + half;
			if (pos.y < mn.y || pos.y > mx.y) continue;

			// Closest point on the AABB to the enemy sphere center.
			const float cx = std::clamp(pos.x, mn.x, mx.x);
			const float cz = std::clamp(pos.z, mn.z, mx.z);
			const float dx = pos.x - cx, dz = pos.z - cz;
			if (dx*dx + dz*dz < ENEMY_R * ENEMY_R) return true;
		}
		return false;
	};

	// Enemy behavior is intentionally simple: bob, face the player,
	// chase within range, and attack on cooldown when close enough.
	for (auto& enemy : enemies) {
		if (!enemy.alive || !enemy.model) continue;

		const float bob = std::sin(static_cast<float>(now) * 2.2f + enemy.bob_offset) * 0.15f;
		enemy.model->pivot_position.y = enemy.y_base + bob;

		const glm::vec3 to_player = camera.Position - enemy.model->pivot_position;
		const float xz_dist = glm::length(glm::vec3(to_player.x, 0.0f, to_player.z));

		constexpr float CHASE_RADIUS = 18.0f;
		constexpr float STOP_DIST    = 1.4f;
		constexpr float SPEED        = 1.8f;

		if (xz_dist > STOP_DIST && xz_dist < CHASE_RADIUS) {
			const glm::vec3 dir  = glm::normalize(glm::vec3(to_player.x, 0.0f, to_player.z));
			const glm::vec3 base = enemy.model->pivot_position;

			// Resolve X and Z independently so enemies can slide along walls
			// instead of stopping completely at shallow angles.
			glm::vec3 try_x = base; try_x.x += dir.x * SPEED * delta_t;
			glm::vec3 try_z = base; try_z.z += dir.z * SPEED * delta_t;
			if (!enemy_blocked(try_x)) enemy.model->pivot_position.x = try_x.x;
			if (!enemy_blocked(try_z)) enemy.model->pivot_position.z = try_z.z;
		}

		enemy.model->eulerAngles.y = glm::degrees(std::atan2(to_player.x, to_player.z));

		// Melee attack — 10 HP per hit, 1 second cooldown
		const float full_dist = glm::distance(camera.Position, enemy.model->pivot_position);
		if (full_dist < 2.1f && now - enemy.last_attack_time > 1.0) {
			enemy.last_attack_time = now;
			player_health = std::max(0, player_health - 10);
			audio_play_hurt();
			if (player_health <= 0)
				enter_game_over();
			else
				set_hud_message("SCP-871 instance — taking damage!");
		}
	}

	// Ambient particle sources are throttled so effects stay cheap.
	if (now - last_fire_particle_time > 0.12) {
		for (const auto& source : fire_sources)
			spawn_particles(source + glm::vec3(0.0f, 0.35f, 0.0f), 1);
		last_fire_particle_time = now;
	}

	// Once all reactors are active, the hub gate panels rise and fade.
	if (gate_unlocked) {
		for (size_t i = 0; i < hub_door_panels.size(); ++i) {
			auto& panel = hub_door_panels[i];
			if (!panel) continue;
			const float direction = i == 3 ? -1.0f : (i == 4 ? 1.0f : 0.0f);
			panel->pivot_position.y = std::min(6.0f, panel->pivot_position.y + delta_t * 1.8f);
			panel->pivot_position.x += direction * delta_t * 0.8f;
			panel->material_alpha = std::max(0.18f, panel->material_alpha - delta_t * 0.35f);
			panel->collides = false;
		}
	}

	if (gate_unlocked && gate_model) {
		gate_model->pivot_position.y = std::min(5.0f, gate_model->pivot_position.y + delta_t * 2.0f);
		gate_model->collides = false;
	}

	// Reactor button proximity hints
	for (auto& reactor : reactors) {
		if (reactor.active || !reactor.button) continue;
		const float rd = glm::distance(camera.Position, reactor.button->pivot_position);
		if (rd < 3.5f)
			set_hud_message("[ E ]  Activate reactor", 0.4f);
	}

	// Pulsing glow + hint for hidden door button
	if (!hidden_door_open && hidden_door_btn) {
		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now) * 3.5f);
		hidden_door_btn->emissive_color = glm::vec3(0.9f, 0.35f, 0.0f) * pulse;
		hidden_door_btn->eulerAngles.y += delta_t * 90.0f;
		const float dist = glm::distance(camera.Position, hidden_door_btn->pivot_position);
		if (dist < 4.5f)
			set_hud_message("[ E ]  Open maintenance hatch", 0.4f);
	}

	if (hidden_door_open && hidden_door_wall) {
		hidden_door_wall->pivot_position.y = std::min(6.0f, hidden_door_wall->pivot_position.y + delta_t * 2.0f);
		hidden_door_wall->collides = false;
	}

    // Trigger zones fire once and show large location text.
    for (auto& tz : trigger_zones) {
        if (!tz.fired && glm::distance(camera.Position, tz.position) < tz.radius) {
            tz.fired = true;
            show_location_text(tz.message, tz.duration);
        }
    }

    // Win condition: player passes the evacuation threshold behind the blast door.
    if (gate_unlocked && camera.Position.z < -41.5f)
        enter_win();
}

// Handles vertical player motion that the Camera class does not know about:
// sprint speed, jumping, gravity, and view bob.
void App::update_player_motion(float delta_t)
{
    const bool sprinting = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                           glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    const bool moving = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    camera.MovementSpeed = sprinting ? PLAYER_SPRINT_SPEED : PLAYER_WALK_SPEED;

    if (!collisions_enabled) {
        view_bob_offset = 0.0f;
        view_bob_phase = 0.0f;
        return;
    }

    if (player_on_ground && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        player_on_ground = false;
        player_vertical_velocity = PLAYER_JUMP_SPEED;
        player_vertical_offset = std::max(player_vertical_offset, 0.02f);
    }

    if (!player_on_ground) {
        player_vertical_velocity += PLAYER_GRAVITY * delta_t;
        player_vertical_offset += player_vertical_velocity * delta_t;

        // Landing is disabled over the open hub pit so the player keeps
        // falling until update_pit_state() respawns them below DEATH_PIT_Y.
        const bool over_open_pit = is_over_hub_pit() && !is_over_hub_walkway();
        if (player_vertical_offset <= 0.0f && !over_open_pit) {
            player_vertical_offset = 0.0f;
            player_vertical_velocity = 0.0f;
            player_on_ground = true;
        }
    }

    if (moving && player_on_ground) {
        view_bob_phase  += delta_t * (sprinting ? 13.5f : 9.5f);
        view_bob_offset  = std::sin(view_bob_phase) * (sprinting ? 0.070f : 0.045f);
    } else {
        view_bob_offset *= std::max(0.0f, 1.0f - delta_t * 12.0f);
    }

    camera.Position.y = current_camera_eye_y();
}

// Computes final camera eye height from base eye height, jump/fall offset,
// and view bob, clamped to the playable vertical range.
float App::current_camera_eye_y() const
{
    return std::clamp(PLAYER_EYE_HEIGHT + player_vertical_offset + view_bob_offset, DEATH_PIT_Y - 4.0f, MAP_MAX_Y);
}

// Detects falling through the open hub pit and respawns below the death plane.
void App::update_pit_state()
{
    if (!collisions_enabled) return;

    if (player_on_ground && player_vertical_offset <= 0.02f && is_over_hub_pit() && !is_over_hub_walkway()) {
        player_on_ground = false;
        player_vertical_velocity = -0.25f;
    }

    if (camera.Position.y < DEATH_PIT_Y)
        respawn_player("D-9341 fell into the containment pit.");
}

// Restores player position, view direction, vertical movement, and health.
// Used both after falling into the pit and when resetting the whole game.
void App::respawn_player(const std::string& message)
{
    camera.Position = glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 25.5f);
    camera.Front = glm::vec3(0.0f, 0.0f, -1.0f);
    camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
    camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
    camera.Yaw   = -90.0f;
    camera.Pitch = 0.0f;
    player_vertical_offset   = 0.0f;
    player_vertical_velocity = 0.0f;
    view_bob_phase  = 0.0f;
    view_bob_offset = 0.0f;
    player_on_ground = true;
    player_health = 100;
    firstMouse = true;
    if (!message.empty())
        set_hud_message(message);
}

// Resets gameplay state without rebuilding assets.
// This is used for "new game" and game-over restart.
void App::reset_game_world()
{
    game_over = false;
    game_over_time = 0.0;
    player_health = 100;
    reactors_active = 0;
    gate_unlocked = false;
    hidden_door_open = false;
    particles.clear();
    last_shot_time = -10.0;
    last_fire_particle_time = glfwGetTime();
    hud_message.clear();
    location_message.clear();

    respawn_player("");

    // One-shot trigger text should be visible again after restarting.
    for (auto& tz : trigger_zones)
        tz.fired = false;

    // Restore reactors/buttons to their inactive, collidable state.
    for (auto& reactor : reactors) {
        reactor.active = false;
        if (reactor.model) {
            reactor.model->material_alpha = 0.55f;
            reactor.model->collides = true;
        }
        if (reactor.button) {
            reactor.button->collides = true;
            reactor.button->scale = glm::vec3(0.8f, 0.35f, 0.8f);
            reactor.button->material_alpha = 1.0f;
        }
    }

    if (gate_model) {
        gate_model->pivot_position = glm::vec3(0.0f, 2.0f, -39.2f);
        gate_model->collides = true;
        gate_model->material_alpha = 1.0f;
    }

    for (auto& panel : hub_door_panels) {
        if (!panel) continue;
        panel->collides = true;
        panel->material_alpha = 1.0f;
    }

    if (hidden_door_wall) {
        hidden_door_wall->pivot_position = glm::vec3(81.5f, 2.0f, -24.0f);
        hidden_door_wall->collides = true;
        hidden_door_wall->material_alpha = 1.0f;
        hidden_door_wall->emissive_color = glm::vec3(0.04f, 0.04f, 0.05f);
    }
    if (hidden_door_btn) {
        hidden_door_btn->pivot_position  = glm::vec3(78.0f, 0.60f, -38.0f);
        hidden_door_btn->eulerAngles     = glm::vec3(0.0f);
        hidden_door_btn->scale           = glm::vec3(0.9f, 0.45f, 0.9f);
        hidden_door_btn->collides        = true;
        hidden_door_btn->material_alpha  = 1.0f;
        hidden_door_btn->emissive_color  = glm::vec3(0.6f, 0.2f, 0.0f);
    }

    // Enemy spawn snapshots were captured in init_assets().
    for (auto& enemy : enemies) {
        enemy.health = enemy.max_health;
        enemy.alive = true;
        enemy.last_attack_time = -10.0;
        if (!enemy.model) continue;
        enemy.model->pivot_position  = enemy.spawn_position;
        enemy.model->eulerAngles     = enemy.spawn_euler;
        enemy.model->scale           = enemy.spawn_scale;
        enemy.model->collides        = enemy.spawn_collides;
        enemy.model->is_transparent  = enemy.spawn_transparent;
        enemy.model->material_alpha  = enemy.spawn_alpha;
    }

    // Dim wing lights — each reactor will restore its section on activation
    for (size_t i = 6; i < point_lights.size() && i < point_lights_saved.size(); ++i) {
        point_lights[i].diffuse  = glm::vec3(0.0f);
        point_lights[i].specular = glm::vec3(0.0f);
        point_lights[i].ambient  = glm::vec3(0.012f, 0.008f, 0.008f);
    }

    set_hud_message("OBJECTIVE: activate all three reactors and disengage the blast door.", 6.0f);
}

// Enters the playable state from the menu or game-over screen.
void App::start_new_game()
{
    reset_game_world();
    game_state = GameState::Playing;
    game_state_enter_time = glfwGetTime();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    firstMouse = true;
}

// Triggers the mission-complete screen when the player crosses the evacuation threshold.
void App::enter_win()
{
    if (game_state == GameState::Won) return;
    game_state = GameState::Won;
    game_state_enter_time = glfwGetTime();
    hud_message.clear();
    location_message.clear();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

// Switches to game-over UI, releases the cursor, and stops HUD/location text.
void App::enter_game_over()
{
    if (game_state == GameState::GameOver) return;

    game_over = true;
    game_over_time = glfwGetTime();
    game_state = GameState::GameOver;
    game_state_enter_time = game_over_time;
    hud_message.clear();
    location_message.clear();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

// Finds the nearest inactive reactor button or hidden-door button in range
// and activates the matching interaction.
void App::activate_nearest_reactor()
{
	float best_distance = std::numeric_limits<float>::max();
	Reactor* nearest = nullptr;

	for (auto& reactor : reactors) {
		if (reactor.active || !reactor.button) continue;
		const float distance = glm::distance(camera.Position, reactor.button->pivot_position);
		if (distance < best_distance) {
			best_distance = distance;
			nearest = &reactor;
		}
	}

	if (!hidden_door_open && hidden_door_btn) {
		const float dist = glm::distance(camera.Position, hidden_door_btn->pivot_position);
		if (dist < 2.4f) {
			hidden_door_open = true;
			audio_play_door();
			set_hud_message("Maintenance hatch open.");
			return;
		}
	}

	if (!nearest || best_distance > 2.4f) {
		set_hud_message("No reactor button in range.");
		return;
	}

	const int reactor_idx = static_cast<int>(nearest - reactors.data());
	nearest->active = true;

	// Activated reactors become mostly visual: the glass fades up and the
	// button/collider no longer blocks movement.
	nearest->model->material_alpha = 0.85f;
	nearest->model->collides = false;
	if (nearest->button) {
		nearest->button->collides = false;
		nearest->button->scale = glm::vec3(0.75f, 0.18f, 0.75f);
	}
	reactors_active++;
	spawn_particles(nearest->model->pivot_position + glm::vec3(0.0f, 1.2f, 0.0f), 30);

	// Each reactor restores power to its section: R1→6-11, R2+junction→12-23, R3+east→24-35
	static constexpr int light_lo[3] = { 6, 12, 24 };
	static constexpr int light_hi[3] = { 11, 23, 35 };
	if (reactor_idx >= 0 && reactor_idx < 3) {
		for (int i = light_lo[reactor_idx]; i <= light_hi[reactor_idx] && i < static_cast<int>(point_lights_saved.size()); ++i)
			point_lights[i] = point_lights_saved[i];

	}

	if (reactors_active >= static_cast<int>(reactors.size())) {
		if (!gate_unlocked) {
			gate_unlocked = true;
			audio_play_door();
		}
		// Containment field neutralizes remaining SCP-871 instances
		for (auto& enemy : enemies) {
			if (!enemy.alive || !enemy.model) continue;
			enemy.alive = false;
			spawn_particles(enemy.model->pivot_position + glm::vec3(0.0f, 0.6f, 0.0f), 12);
			enemy.model->collides      = false;
			enemy.model->material_alpha = 0.0f;
			enemy.model->is_transparent = true;
			enemy.model->pivot_position.y = -20.0f;
		}
		set_hud_message("Blast door disengaged - proceed to evacuation point.", 8.0f);
	} else {
		set_hud_message("Reactor online.");
	}
}

// Developer shortcut for testing powered/unpowered hub behavior.
void App::toggle_all_reactors()
{
	const bool turn_on = reactors_active < static_cast<int>(reactors.size());
	reactors_active = turn_on ? static_cast<int>(reactors.size()) : 0;
	gate_unlocked = turn_on;
	if (turn_on) {
		audio_play_door();
	}

	for (auto& reactor : reactors) {
		reactor.active = turn_on;
		if (reactor.model) {
			reactor.model->material_alpha = turn_on ? 0.85f : 0.55f;
			reactor.model->collides = !turn_on;
		}
		if (reactor.button) {
			reactor.button->collides = !turn_on;
			reactor.button->scale = turn_on ? glm::vec3(0.75f, 0.18f, 0.75f) : glm::vec3(0.8f, 0.35f, 0.8f);
			reactor.button->material_alpha = 1.0f;
		}
	}

	if (!turn_on && gate_model) {
		gate_model->pivot_position = glm::vec3(0.0f, 1.8f, -39.2f);
		gate_model->collides = true;
		gate_model->material_alpha = 1.0f;
	}

	// Sync wing lights to match reactor state
	for (size_t i = 6; i < point_lights.size() && i < point_lights_saved.size(); ++i) {
		if (turn_on) {
			point_lights[i] = point_lights_saved[i];
		} else {
			point_lights[i].diffuse  = glm::vec3(0.0f);
			point_lights[i].specular = glm::vec3(0.0f);
			point_lights[i].ambient  = glm::vec3(0.012f, 0.008f, 0.008f);
		}
	}

	set_hud_message(turn_on ? "Dev: all reactors online." : "Dev: all reactors offline.");
}

// Fires the player weapon along the camera forward ray.
// Hits the closest alive enemy sphere within weapon range.
void App::fire_weapon()
{
	if (game_state != GameState::Playing || game_over) return;

	const double now = glfwGetTime();
	if (now - last_shot_time < 0.18) return;
	last_shot_time = now;

	const glm::vec3 ray_origin = camera.Position;
	const glm::vec3 ray_dir    = glm::normalize(camera.Front);
	float best_hit = std::numeric_limits<float>::max();
	Enemy* hit_enemy = nullptr;

	// Pick the closest enemy intersected by the ray so shots behave correctly
	// when enemies overlap in screen space.
	for (auto& enemy : enemies) {
		if (!enemy.alive || !enemy.model) continue;
		float hit_distance = 0.0f;
		if (ray_hits_sphere(ray_origin, ray_dir, enemy.model->pivot_position, enemy.model->bounding_radius, hit_distance)
		    && hit_distance < best_hit) {
			best_hit = hit_distance;
			hit_enemy = &enemy;
		}
	}

	audio_play_shoot();

	if (!hit_enemy) {
		set_hud_message("Missed.");
		spawn_particles(ray_origin + ray_dir * 2.0f, 4);
		return;
	}

	hit_enemy->health--;
	spawn_particles(hit_enemy->model->pivot_position + glm::vec3(0.0f, 0.6f, 0.0f), 18);

	if (hit_enemy->health <= 0) {
		hit_enemy->alive = false;
		hit_enemy->model->collides = false;
		hit_enemy->model->material_alpha = 0.0f;
		hit_enemy->model->is_transparent = true;
		hit_enemy->model->pivot_position.y = -20.0f;
		set_hud_message("SCP-871 instance neutralized.");
	} else {
		set_hud_message("SCP-871 instance hit.");
	}
}

// Ray/sphere intersection helper used by fire_weapon().
// Returns true only for forward hits within the weapon's max distance.
bool App::ray_hits_sphere(const glm::vec3& ray_origin,
                          const glm::vec3& ray_dir,
                          const glm::vec3& sphere_center,
                          float sphere_radius,
                          float& hit_distance) const
{
	const glm::vec3 oc = ray_origin - sphere_center;

	// Quadratic ray/sphere intersection with ray_dir assumed normalized.
	const float b = 2.0f * glm::dot(oc, ray_dir);
	const float c = glm::dot(oc, oc) - sphere_radius * sphere_radius;
	const float discriminant = b * b - 4.0f * c;

	if (discriminant < 0.0f) return false;

	const float root = std::sqrt(discriminant);
	const float t1 = (-b - root) * 0.5f;
	const float t2 = (-b + root) * 0.5f;
	hit_distance = t1 > 0.0f ? t1 : t2;
	return hit_distance > 0.0f && hit_distance < 35.0f;
}
