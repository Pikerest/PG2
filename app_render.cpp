/*
 * Rendering helpers for UI and special passes.
 * Draws debug overlays, menu/HUD, enemy health bars, OIT orb compositing,
 * and the simple particle system.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include <imgui.h>

#include "app.hpp"

namespace {
constexpr float DRAW_ALPHA_EPSILON = 0.001f;
constexpr std::size_t MAX_PARTICLES = 128;
}

// Helper — draws a heart icon using ImGui primitives
static void draw_heart(ImDrawList* dl, float cx, float cy, float r, ImU32 col) {
    dl->AddCircleFilled(ImVec2(cx - r * 0.5f, cy - r * 0.2f), r * 0.62f, col, 16);
    dl->AddCircleFilled(ImVec2(cx + r * 0.5f, cy - r * 0.2f), r * 0.62f, col, 16);
    dl->AddTriangleFilled(
        ImVec2(cx - r, cy + r * 0.05f),
        ImVec2(cx + r, cy + r * 0.05f),
        ImVec2(cx,     cy + r), col);
}

// Debug overlay for collision boxes.
// Draws collidable scene models as cyan wireframes without writing depth.
void App::draw_collision_debug()
{
    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 1);
    shader_prog->setUniform("u_debug_color", glm::vec4(0.05f, 0.95f, 1.0f, 0.72f));

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    for (auto& [name, obj] : scene) {
        if (!obj->collides) continue;
        obj->draw();
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 0);
}

// Debug overlay for point lights.
// Reuses a small cube model to mark every light position in the scene.
void App::draw_light_debug()
{
    if (!light_debug_marker) return;

    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 1);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    for (size_t i = 0; i < point_lights.size(); ++i) {
        const bool top_light = i == 0;
        const glm::vec3 color = top_light ? glm::vec3(1.0f, 0.95f, 0.35f) : glm::vec3(0.15f, 1.0f, 0.85f);
        shader_prog->setUniform("u_debug_color", glm::vec4(color, top_light ? 0.96f : 0.82f));
        light_debug_marker->pivot_position = point_lights[i].position;
        light_debug_marker->scale = glm::vec3(top_light ? 0.55f : 0.36f);
        light_debug_marker->draw();
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 0);
}

// Debug overlay for trigger zones.
// Renders their approximate volumes in 3D and labels them in screen space.
void App::draw_trigger_debug()
{
    if (!light_debug_marker) return;

    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 1);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    for (const auto& tz : trigger_zones) {
        const glm::vec4 col = tz.fired
            ? glm::vec4(0.5f, 0.5f, 0.5f, 0.35f)
            : glm::vec4(0.20f, 1.00f, 0.35f, 0.55f);
        shader_prog->setUniform("u_debug_color", col);
        light_debug_marker->pivot_position = tz.position;
        light_debug_marker->scale = glm::vec3(tz.radius);
        light_debug_marker->draw();
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    shader_prog->setUniform("u_debug_collision", 0);

    // ImGui labels projected to screen space
    const glm::mat4 vp = projection_matrix * view_matrix;
    for (const auto& tz : trigger_zones) {
        const glm::vec4 clip = vp * glm::vec4(tz.position, 1.0f);
        if (clip.w <= 0.0f) continue;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f) continue;
        const float sx = (ndc.x * 0.5f + 0.5f) * width;
        const float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * height;

        ImGui::SetNextWindowPos(ImVec2(sx, sy), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 2.0f));
        ImGui::Begin(("##tz_" + tz.message).c_str(), nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing);
        const ImVec4 tcol = tz.fired
            ? ImVec4(0.55f, 0.55f, 0.55f, 1.0f)
            : ImVec4(0.30f, 1.00f, 0.45f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, tcol);
        ImGui::Text("%s  r=%.1f  %s", tz.message.c_str(), tz.radius, tz.fired ? "[fired]" : "");
        ImGui::PopStyleColor();
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

// Fullscreen menu/game-over UI.
// The menu is drawn with ImGui primitives over the already-rendered scene.
void App::draw_menu()
{
    const bool is_go = (game_state == GameState::GameOver);
    const ImVec2 viewport_size(static_cast<float>(width), static_cast<float>(height));
    const ImGuiWindowFlags overlay_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport_size, ImGuiCond_Always);
    ImGui::Begin("##menu_backdrop", nullptr, overlay_flags);
    ImDrawList* bg = ImGui::GetWindowDrawList();
    const ImU32 shade = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, is_go ? 0.78f : 0.62f));
    bg->AddRectFilled(ImVec2(0.0f, 0.0f), viewport_size, shade);
    bg->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(viewport_size.x, viewport_size.y * 0.16f),
                      ImGui::ColorConvertFloat4ToU32(ImVec4(0.00f, 0.04f, 0.05f, 0.34f)));
    bg->AddRectFilled(ImVec2(0.0f, viewport_size.y * 0.78f), viewport_size,
                      ImGui::ColorConvertFloat4ToU32(ImVec4(0.02f, 0.02f, 0.02f, 0.38f)));
    ImGui::End();

    const float panel_w = std::clamp(viewport_size.x - 96.0f, 420.0f, 760.0f);
    const float panel_h = is_go ? 292.0f : 420.0f;
    const ImVec2 panel_pos(viewport_size.x * 0.5f - panel_w * 0.5f,
                           viewport_size.y * 0.5f - panel_h * 0.5f);
    const double t = glfwGetTime() - game_state_enter_time;
    const float pulse = 0.64f + 0.36f * std::sin(static_cast<float>(t) * 3.8f);

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(34.0f, 28.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.033f, 0.040f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, is_go ? ImVec4(0.95f, 0.18f, 0.12f, 0.70f)
                                                 : ImVec4(0.20f, 0.84f, 0.86f, 0.70f));
    ImGui::Begin("##menu", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoTitleBar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos();
    const ImVec2 s = ImGui::GetWindowSize();
    const ImU32 accent     = is_go ? IM_COL32(230, 52, 38, 230) : IM_COL32(60, 210, 220, 230);
    const ImU32 accent_dim = is_go ? IM_COL32(230, 52, 38, 78)  : IM_COL32(60, 210, 220, 78);
    dl->AddRectFilled(p, ImVec2(p.x + 7.0f, p.y + s.y), accent);
    dl->AddLine(ImVec2(p.x + 22.0f, p.y + 78.0f), ImVec2(p.x + s.x - 30.0f, p.y + 78.0f), accent_dim, 1.5f);
    dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), accent_dim, 0.0f, 0, 2.0f);

    if (is_go) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.20f, 0.15f, 1.0f));
        ImGui::SetWindowFontScale(2.1f);
        ImGui::TextUnformatted("GAME OVER");
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextWrapped("Your suit flatlined in the containment sector. The facility is resetting the simulation from the first checkpoint.");
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.86f, 0.72f, 0.92f));
        ImGui::TextUnformatted("SPACE / ENTER  Restart from the beginning");
        ImGui::TextUnformatted("ESC            Quit");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.98f, 1.0f, 1.0f));
        ImGui::SetWindowFontScale(2.15f);
        ImGui::TextUnformatted("FACILITY X-7");
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.86f, 0.90f, 1.0f));
        ImGui::TextUnformatted("Containment Recovery Protocol");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextWrapped("You wake inside an abandoned research complex after a containment failure. Specimens are loose, power is unstable, and the main gate is locked.");
        ImGui::Spacing();
        ImGui::TextWrapped("Mission: activate all three reactors, reopen the containment gate, and survive long enough to reach the exit.");
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.84f, 0.92f, 0.92f, 0.95f));
        ImGui::Columns(2, "##controls", false);
        ImGui::TextUnformatted("WASD");       ImGui::NextColumn(); ImGui::TextUnformatted("Move");              ImGui::NextColumn();
        ImGui::TextUnformatted("Mouse / LMB");ImGui::NextColumn(); ImGui::TextUnformatted("Look / Fire");       ImGui::NextColumn();
        ImGui::TextUnformatted("E / Shift");  ImGui::NextColumn(); ImGui::TextUnformatted("Interact / Sprint"); ImGui::NextColumn();
        ImGui::TextUnformatted("Space");      ImGui::NextColumn(); ImGui::TextUnformatted("Jump");
        ImGui::Columns(1);
        ImGui::PopStyleColor();
    }

    const char* prompt = is_go ? "PRESS SPACE TO RESTART" : "PRESS SPACE TO START";
    const ImVec2 prompt_size = ImGui::CalcTextSize(prompt);
    const float prompt_x = p.x + (s.x - prompt_size.x) * 0.5f;
    const float prompt_y = p.y + s.y - 48.0f;
    dl->AddText(ImVec2(prompt_x + 1.0f, prompt_y + 1.0f), IM_COL32(0, 0, 0, 200), prompt);
    dl->AddText(ImVec2(prompt_x, prompt_y),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.86f, 1.0f, 1.0f, 0.48f + 0.52f * pulse)),
                prompt);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// Bottom-left player health UI.
// Uses immediate-mode drawing so it can pulse/flash without extra assets.
void App::draw_player_hud()
{
    const float hp_pct  = std::clamp(player_health / 100.0f, 0.0f, 1.0f);
    const bool critical = player_health <= 25;
    const float panel_w = std::clamp(static_cast<float>(width) * 0.34f, 360.0f, 560.0f);
    const float panel_h = 76.0f;
    const float x = 28.0f;
    const float y = static_cast<float>(height) - panel_h - 26.0f;

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h), ImGuiCond_Always);
    ImGui::Begin("##hud_health", nullptr, flags);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos();
    const float pulse = critical ? (0.45f + 0.55f * std::sin(static_cast<float>(glfwGetTime()) * 9.0f)) : 0.0f;

    const ImU32 panel_bg   = IM_COL32(7, 12, 15, 205);
    const ImU32 panel_edge = critical ? IM_COL32(235, 58, 42, 210) : IM_COL32(55, 205, 214, 190);
    const ImU32 bar_bg     = IM_COL32(12, 18, 22, 235);
    const ImVec2 panel_min(p.x, p.y);
    const ImVec2 panel_max(p.x + panel_w, p.y + panel_h);
    dl->AddRectFilled(panel_min, panel_max, panel_bg, 4.0f);
    dl->AddRect(panel_min, panel_max, panel_edge, 4.0f, 0, 2.0f);
    dl->AddRectFilled(panel_min, ImVec2(panel_min.x + 6.0f, panel_max.y), panel_edge, 4.0f);

    const ImVec2 bar_min(p.x + 22.0f, p.y + 36.0f);
    const ImVec2 bar_max(p.x + panel_w - 22.0f, p.y + 60.0f);
    const float bar_w = bar_max.x - bar_min.x;
    const ImU32 fill_col =
        hp_pct > 0.55f ? IM_COL32(64, 218, 132, 245) :
        hp_pct > 0.25f ? IM_COL32(238, 184, 64, 245) :
                         IM_COL32(232, 56, 42, 245);
    dl->AddRectFilled(bar_min, bar_max, bar_bg, 3.0f);
    dl->AddRectFilled(bar_min, ImVec2(bar_min.x + bar_w * hp_pct, bar_max.y), fill_col, 3.0f);
    dl->AddRect(bar_min, bar_max, IM_COL32(220, 245, 245, 95), 3.0f, 0, 1.0f);

    for (int i = 1; i < 10; ++i) {
        const float sx = bar_min.x + bar_w * (static_cast<float>(i) / 10.0f);
        dl->AddLine(ImVec2(sx, bar_min.y + 3.0f), ImVec2(sx, bar_max.y - 3.0f), IM_COL32(0, 0, 0, 80), 1.0f);
    }

    if (critical) {
        dl->AddRect(panel_min, panel_max,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.12f, 0.08f, 0.18f + 0.32f * pulse)),
                    4.0f, 0, 4.0f);
    }

    char hp_text[32];
    snprintf(hp_text, sizeof(hp_text), "%d / 100", player_health);
    dl->AddText(ImVec2(p.x + 22.0f, p.y + 13.0f), IM_COL32(210, 242, 242, 235), "VITALS");
    const ImVec2 hp_size = ImGui::CalcTextSize(hp_text);
    dl->AddText(ImVec2(p.x + panel_w - 22.0f - hp_size.x, p.y + 13.0f),
                critical ? IM_COL32(255, 124, 96, 245) : IM_COL32(230, 252, 252, 235),
                hp_text);
    ImGui::End();
}

// Floating hearts above living enemies.
// Enemy world positions are projected to screen coordinates each frame.
void App::draw_enemy_health_bars()
{
    const glm::mat4 vp = projection_matrix * view_matrix;
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav        | ImGuiWindowFlags_NoMove   |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground;

    constexpr int   MAX_HP    = 3;
    constexpr float HEART_R   = 7.0f;
    constexpr float HEART_GAP = 18.0f;

    for (int i = 0; i < (int)enemies.size(); ++i) {
        const auto& enemy = enemies[i];
        if (!enemy.alive || !enemy.model) continue;

        const float dist_check = glm::distance(camera.Position, enemy.model->pivot_position);
        if (dist_check > 22.0f) continue;

        const glm::vec3 world_pos = enemy.model->pivot_position + glm::vec3(0.0f, 2.0f, 0.0f);
        const glm::vec4 clip = vp * glm::vec4(world_pos, 1.0f);
        if (clip.w <= 0.0f) continue;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f) continue;
        const float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
        const float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);

        const float alpha  = std::clamp(1.0f - (dist_check - 12.0f) / 10.0f, 0.3f, 1.0f);
        const float total_w = MAX_HP * HEART_GAP;

        ImGui::SetNextWindowPos(ImVec2(sx - total_w * 0.5f, sy - HEART_R * 2.5f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(total_w, HEART_R * 2.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
        ImGui::Begin(("##ehp_" + std::to_string(i)).c_str(), nullptr, flags);

        ImDrawList* dl  = ImGui::GetWindowDrawList();
        const ImVec2 wpos = ImGui::GetWindowPos();

        for (int h = 0; h < MAX_HP; ++h) {
            const float hx = wpos.x + (h + 0.5f) * HEART_GAP;
            const float hy = wpos.y + HEART_R;
            ImU32 col;
            if (h < enemy.health) {
                float r, g, b;
                if      (enemy.health == 1) { r=1.0f; g=0.15f; b=0.15f; }
                else if (enemy.health == 2) { r=1.0f; g=0.75f; b=0.0f;  }
                else                        { r=0.9f; g=0.1f;  b=0.2f;  }
                col = IM_COL32(int(r*255), int(g*255), int(b*255), int(alpha*255));
            } else {
                col = IM_COL32(60, 60, 60, int(alpha * 130));
            }
            draw_heart(dl, hx, hy, HEART_R, col);
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

// Weighted blended order-independent transparency pass for orb layers.
// The orb is special-cased so nested transparent shells do not sort badly.
void App::draw_orb_oit(const std::vector<std::shared_ptr<Model>>& oit_models)
{
    if (oit_models.empty() || oit_fbo == 0 || oit_composite_prog == nullptr) return;

    setup_oit_buffers(width, height);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oit_fbo);
    glBlitFramebuffer(0, 0, width, height,
                      0, 0, oit_width, oit_height,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    const GLfloat accum_clear[]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    const GLfloat reveal_clear[] = { 1.0f, 0.0f, 0.0f, 0.0f };
    glClearNamedFramebufferfv(oit_fbo, GL_COLOR, 0, accum_clear);
    glClearNamedFramebufferfv(oit_fbo, GL_COLOR, 1, reveal_clear);

    glBindFramebuffer(GL_FRAMEBUFFER, oit_fbo);
    const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, draw_buffers);
    glViewport(0, 0, oit_width, oit_height);

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

    shader_prog->use();
    shader_prog->setUniform("u_oit_pass", 1);
    for (const auto& orb : oit_models) {
        if (orb && orb->material_alpha > DRAW_ALPHA_EPSILON)
            orb->draw();
    }
    shader_prog->setUniform("u_oit_pass", 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    oit_composite_prog->use();
    glBindTextureUnit(0, oit_accum_tex);
    glBindTextureUnit(1, oit_reveal_tex);
    glBindVertexArray(fullscreen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    shader_prog->use();
}

// Adds short-lived particles at a world position.
// Used for shots, reactor activation, enemy hits, and ambient fire effects.
void App::spawn_particles(const glm::vec3& position, int count) {
    if (particles.size() >= MAX_PARTICLES) return;

    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> angle_dist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> speed_dist(1.5f, 4.0f);
    std::uniform_real_distribution<float> upward_dist(2.0f, 6.0f);
    std::uniform_real_distribution<float> life_dist(0.8f, 1.8f);
    std::uniform_real_distribution<float> scale_dist(0.05f, 0.2f);

    const size_t free_slots  = MAX_PARTICLES - particles.size();
    const int    spawn_count = std::min<int>(count, static_cast<int>(free_slots));

    for (int i = 0; i < spawn_count; i++) {
        Particle p;
        p.position = position;
        p.age      = 0.0f;
        p.lifetime = life_dist(rng);
        p.scale    = scale_dist(rng);
        const float theta = angle_dist(rng);
        const float speed = speed_dist(rng);
        p.velocity = glm::vec3(speed * cos(theta), upward_dist(rng), speed * sin(theta));
        particles.push_back(p);
    }
}

// Integrates particle velocity/lifetime and removes expired particles.
void App::update_particles(float delta_t) {
    static const float GRAVITY     = -9.8f;
    static const float ATTENUATION =  0.95f;

    for (auto& p : particles) {
        p.age        += delta_t;
        p.velocity.y += GRAVITY * delta_t;
        p.position   += p.velocity * delta_t;
        if (p.position.y < 0.0f) {
            p.position.y = 0.0f;
            p.velocity  *= glm::vec3(ATTENUATION, -ATTENUATION, ATTENUATION);
        }
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.age >= p.lifetime; }),
        particles.end()
    );
}

// Draws all live particles using one reusable model instance.
// Each particle updates the template transform before drawing.
void App::draw_particles() {
    if (particles.empty() || !particle_template) return;

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    for (auto& p : particles) {
        const float life_ratio = 1.0f - (p.age / p.lifetime);
        particle_template->pivot_position = p.position;
        particle_template->scale          = glm::vec3(p.scale);
        particle_template->eulerAngles.y  = p.age * 180.0f;
        particle_template->material_alpha = life_ratio * 0.85f;
        particle_template->draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
