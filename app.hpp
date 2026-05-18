#pragma once

/*
 * Central App interface and shared state.
 * Declares the scene, game, render, input, and collision state used by the
 * App methods implemented across app.cpp and the app_*.cpp files.
 */

// Standard headers
#include <array>
#include <vector>
#include <string>
#include <unordered_set>

// GLFWwindow is stored only as a pointer here; implementation files include
// the full GLFW header before using GLFW functions/constants.
struct GLFWwindow;

// Project headers
#include "assets.hpp"
#include "ShaderProgram.hpp"
#include "Model.hpp"
#include <memory>

#include "camera.hpp"

// Directional light is the global fill/key light.
// It behaves like light from infinitely far away, so it has direction only.
struct DirectionalLight {
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, -1.0f);
    glm::vec3 ambient = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
    glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f);
};

// Point light is a local spherical light source with attenuation radius.
// Most lamps in the facility are represented by these.
struct PointLight {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 ambient = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
    glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f);
    float radius = 100.0f;
};

// Spotlight follows the player camera and acts like a flashlight.
struct SpotLight {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 ambient = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
    glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f);
    float cutoff = 12.5f;
    float outer_cutoff = 17.5f;
};

class App {

protected:
    // Projection/window-sized render state.
    int width{0}, height{0};
    float fov = 60.0f;

    // Store projection matrix here and update it only when size/FOV changes.
    glm::mat4 projection_matrix = glm::identity<glm::mat4>();

    // All named scene objects. The render loop builds sorted draw lists from
    // this map every frame.
    std::unordered_map<std::string, std::shared_ptr<Model>> scene;

    // Mouse-look state for relative cursor movement.
    bool firstMouse = true;

    glm::mat4 view_matrix = glm::identity<glm::mat4>();

    Camera camera;
    // remember last cursor position, move relative to that in the next frame
    double cursorLastX{ 0 };
    double cursorLastY{ 0 };


private:
    // Reactor pairs a glowing reactor model with its floor/button control.
    struct Reactor {
        std::shared_ptr<Model> model;
        std::shared_ptr<Model> button;
        glm::vec3 color;
        bool active{false};
    };

    // Enemy stores both runtime state and original spawn state so the game can
    // reset without reloading assets.
    struct Enemy {
        std::shared_ptr<Model> model;
        int health{3};
        float bob_offset{0.0f};
        bool alive{true};
        float y_base{0.8f};
        double last_attack_time{-10.0};
        int max_health{3};
        glm::vec3 spawn_position{0.0f};
        glm::vec3 spawn_euler{0.0f};
        glm::vec3 spawn_scale{1.0f};
        bool spawn_collides{false};
        bool spawn_transparent{false};
        float spawn_alpha{1.0f};
    };

    bool show_imgui{true};

    int window_width = 800;
    int window_height = 600;
    std::string window_title = "OpenGL context ";
    bool is_vsync_on = true;
    // Window/fullscreen state saved so fullscreen can restore exact windowed
    // position and size.
    bool fullscreen_enabled = false;
    int saved_window_x = 0;
    int saved_window_y = 0;
    int saved_window_width = 800;
    int saved_window_height = 600;

    bool is_multisample_on = true;

    void toggle_fullscreen();
    void take_screenshot();

    GLFWwindow* window = nullptr;

    // Core render resources: main material shader plus OIT composite shader.
    std::shared_ptr<ShaderProgram> shader_prog;
    std::shared_ptr<ShaderProgram> oit_composite_prog;

    // Important gameplay/scene object handles. Most objects are still in
    // scene; these pointers make frequently animated/interactive objects cheap
    // to access from gameplay/render code.
    std::shared_ptr<Model> model;
    std::shared_ptr<Model> inner_orb_model;
    std::vector<std::shared_ptr<Model>> orb_layer_models;
    std::shared_ptr<Model> gate_model;
    std::shared_ptr<Model> hidden_door_wall;
    std::shared_ptr<Model> hidden_door_btn;
    bool hidden_door_open{false};
    std::vector<std::shared_ptr<Model>> hub_door_panels;

    // Lighting data is stored in world space here, then uploaded in view space
    // by the main render loop.
    DirectionalLight dir_light;
    std::vector<PointLight> point_lights;
    std::vector<SpotLight> spot_lights;

    // Frustum planes (updated each frame) for cheap sphere culling.
    std::array<glm::vec4, 6> frustum_planes{};

    // Persistent render lists — cleared each frame, avoids per-frame heap alloc
    std::vector<std::shared_ptr<Model>> render_opaque;
    std::vector<std::shared_ptr<Model>> render_transparent;
    std::vector<std::shared_ptr<Model>> render_oit_orbs;
    // Objects with collides==true, built once in init_assets
    std::vector<std::shared_ptr<Model>> scene_colliders;
    // Orb model pointers, built once in init_assets
    std::unordered_set<Model*> orb_model_set;

    // Application and gameplay state.
    enum class GameState { Menu, Playing, GameOver } game_state{GameState::Menu};
    double game_state_enter_time{0.0};
    int player_health = 100;
    int reactors_active = 0;
    bool gate_unlocked = false;
    bool collisions_enabled = true;
    bool show_collision_debug = false;
    bool game_over{false};
    double game_over_time{0.0};
    bool show_light_debug = false;
    bool show_trigger_debug = false;
    bool player_on_ground = true;
    float player_vertical_offset = 0.0f;
    float player_vertical_velocity = 0.0f;
    float view_bob_phase = 0.0f;
    float view_bob_offset = 0.0f;
    double last_shot_time = -10.0;
    double last_fire_particle_time = 0.0;
    std::string hud_message;
    double hud_message_time{-100.0};
    float  hud_message_duration{6.0f};

    std::string location_message;
    double location_message_time{-100.0};
    float  location_message_duration{5.5f};

    // Trigger zones show large location text once when the player enters them.
    struct TriggerZone {
        glm::vec3 position;
        float radius;
        std::string message;
        float duration{6.0f};
        bool fired{false};
    };
    std::vector<TriggerZone> trigger_zones;
    std::vector<Reactor> reactors;
    std::vector<Enemy> enemies;
    std::vector<glm::vec3> fire_sources;
    std::shared_ptr<Model> light_debug_marker;

    // Map and player tuning constants shared by movement, collision, and
    // gameplay reset logic.
    static constexpr float MAP_MIN_X = -92.0f;
    static constexpr float MAP_MAX_X =  98.0f;
    static constexpr float MAP_MIN_Z = -55.0f;
    static constexpr float MAP_MAX_Z =  36.0f;
    static constexpr float MAP_MIN_Y =  1.2f;
    static constexpr float MAP_MAX_Y =  4.4f;
    static constexpr float PLAYER_WALK_SPEED = 5.0f;
    static constexpr float PLAYER_SPRINT_SPEED = 8.2f;
    static constexpr float PLAYER_JUMP_SPEED = 5.4f;
    static constexpr float PLAYER_GRAVITY = -14.0f;
    static constexpr float PLAYER_EYE_HEIGHT = 1.2f;
    static constexpr float DEATH_PIT_Y = -9.0f;

    // Particle System and OIT GPU resources.
    // OIT = weighted blended order-independent transparency for the hub orb.
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        float lifetime;
        float age;
        float scale;
    };
    std::vector<Particle> particles;
    std::shared_ptr<Model> particle_template;
    GLuint oit_fbo{0};
    GLuint oit_accum_tex{0};
    GLuint oit_reveal_tex{0};
    GLuint oit_depth_tex{0};
    GLuint fullscreen_vao{0};
    int oit_width{0};
    int oit_height{0};

    // Initialization, asset creation, and app lifecycle helpers.
    // Implemented mostly in app.cpp and app_assets.cpp.
    void init_imgui(void);
    void init_opencv(void);
    void init_glfw(void);
    void init_glew(void);
    void init_gl_debug(void);
    void setup_oit_buffers(int buffer_width, int buffer_height);
    void destroy_oit_buffers();
    std::shared_ptr<Model> add_box(const std::string& name,
                                   const glm::vec3& position,
                                   const glm::vec3& scale,
                                   const std::shared_ptr<Texture>& texture,
                                   bool collides = false,
                                   float radius = 1.0f,
                                   bool transparent = false,
                                   float alpha = 1.0f);
    void set_hud_message(const std::string& msg, float duration = 6.0f);
    void show_location_text(const std::string& msg, float duration = 5.5f);
    // Gameplay helpers implemented in app_gameplay.cpp.
    void start_new_game();
    void enter_game_over();
    void reset_game_world();
    void update_gameplay(float delta_t, double now);
    void update_player_motion(float delta_t);
    void update_pit_state();
    float current_camera_eye_y() const;
    void respawn_player(const std::string& message);
    void activate_nearest_reactor();
    void toggle_all_reactors();
    void fire_weapon();
    bool ray_hits_sphere(const glm::vec3& ray_origin,
                         const glm::vec3& ray_dir,
                         const glm::vec3& sphere_center,
                         float sphere_radius,
                         float& hit_distance) const;
    // Collision helpers implemented in app_collision.cpp.
    void resolve_camera_box_collision(const std::shared_ptr<Model>& obj, float camera_radius);
    bool try_resolve_camera_top_collision(const std::shared_ptr<Model>& obj, float camera_radius);
    bool is_over_hub_pit() const;
    bool is_over_hub_walkway() const;
    // Rendering helpers implemented in app_render.cpp.
    void draw_collision_debug();
    void draw_light_debug();
    void draw_trigger_debug();
    void draw_enemy_health_bars();
    void draw_player_hud();
    void draw_menu();
    void draw_orb_oit(const std::vector<std::shared_ptr<Model>>& oit_models);

    // information display routines
    void print_opencv_info(void);
    void print_glfw_info(void);
    void print_glm_info(void);
    void print_gl_info(void);

public:
    App();

    // Public lifecycle entry points used by main.cpp.
    bool init(void);
    void init_assets(void);
    bool load_config(const std::string& filename);
    int run(void);

    // Collision Detection
    void apply_collisions(float delta_t);

    // Particle System Spawning and management
    void spawn_particles(const glm::vec3& position, int count = 15);
    void update_particles(float delta_t);
    void draw_particles();

    // GLFW callbacks. Static callbacks recover App* from the GLFW user pointer.
    static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfw_fbsize_callback(GLFWwindow* window, int width, int height);
    static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void update_projection_matrix(void);

    void destroy(void);
    ~App();
private:
};
