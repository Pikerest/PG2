#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

#if __has_include(<opencv2/opencv.hpp>)
    #include <opencv2/opencv.hpp>
#elif __has_include(<opencv4/opencv2/opencv.hpp>)
    #include <opencv4/opencv2/opencv.hpp>
#else
    #error "OpenCV header files not found!"
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include "assets.hpp"
#include "app.hpp"
#include "Texture.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

static void GLAPIENTRY gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    auto const src_str = [source]() {
        switch (source) {
        case GL_DEBUG_SOURCE_API: return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
        case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER: return "OTHER";
        default: return "Unknown";
        }
    }();

    auto const type_str = [type]() {
        switch (type) {
        case GL_DEBUG_TYPE_ERROR: return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER: return "MARKER";
        case GL_DEBUG_TYPE_OTHER: return "OTHER";
        default: return "Unknown";
        }
    }();

    auto const severity_str = [severity]() {
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
        case GL_DEBUG_SEVERITY_LOW: return "LOW";
        case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
        case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
        default: return "Unknown";
        }
    }();

    std::cout << "[GL CALLBACK]: " <<
        "source = " << src_str <<
        ", type = " << type_str <<
        ", severity = " << severity_str <<
        ", ID = '" << id << '\'' <<
        ", message = '" << message << '\'' << std::endl;
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

App::App()
{
    std::cout << "Constructed...\n";
}

bool App::init() {

    try {
        init_glfw();

        load_config("config.json");

        init_glew();

        init_gl_debug();

        glfwGetFramebufferSize(window, &width, &height);
        update_projection_matrix();
        glViewport(0, 0, width, height);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_MULTISAMPLE);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LEQUAL);

        print_glfw_info();
        print_opencv_info();
        print_glm_info();
        print_gl_info();

        std::cout << "Initialized...\n";

		init_assets();
		init_imgui();
		init_opencv();

        glfwShowWindow(window);

        return true;
    }
    catch (std::exception const& e) {
        std::cerr << "Init failed: " << e.what() << std::endl;
        return false;
    }
}
bool App::load_config(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open config file. Using defaults.\n";
        return false;
    }

    json config;
    file >> config;

    if (config.contains("window")) {
        window_width  = config["window"].value("width", 800);
        window_height = config["window"].value("height", 600);
        window_title  = config["window"].value("title", "OpenGL context");
    }

    is_vsync_on = config.value("vsync", true);

    return true;
}


void App::init_imgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
	std::cout << "ImGUI version: " << ImGui::GetVersion() << "\n";
}

void App::init_opencv()
{
}

void App::init_glfw(void)
{
	glfwSetErrorCallback(glfw_error_callback);

	if (!glfwInit())
		throw std::runtime_error("GLFW initialization failed.");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	window = glfwCreateWindow(window_width,
		window_height,
		window_title.c_str(),
		nullptr,
		nullptr);

	if (!window)
		throw std::runtime_error("Window creation failed.");

	glfwMakeContextCurrent(window);
	glfwSwapInterval(is_vsync_on ? 1 : 0);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwSetWindowUserPointer(window, this);
	glfwSetKeyCallback(window, glfw_key_callback);
	glfwSetFramebufferSizeCallback(window, glfw_fbsize_callback);
	glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetScrollCallback(window, glfw_scroll_callback);
}

void App::init_glew(void)
{
	glewExperimental = GL_TRUE;

	if (glewInit() != GLEW_OK)
		throw std::runtime_error("GLEW initialization failed.");

	if (!GLEW_ARB_direct_state_access)
		throw std::runtime_error("No Direct State Access support :-(");
}

void App::init_gl_debug()
{
	if (GLEW_ARB_debug_output) {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(gl_debug_callback, nullptr);
	}
}

void App::print_gl_info()
{
	std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
	std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL Version:    " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

void App::print_glfw_info(void)
{
	std::cout << "GLFW Version:    " << glfwGetVersionString() << std::endl;
}

void App::print_opencv_info()
{
	std::cout << "OpenCV Version:  " << CV_VERSION << std::endl;
}

void App::print_glm_info()
{
	std::cout << "GLM Version:     (not included)" << std::endl;
}

void App::init_assets(void) {
    shader_prog = ShaderProgram::from_files("shader.vert", "shader.frag");

    shader_prog->use();
    shader_prog->setUniform("uTexture", 0);

    dir_light.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.2f));
    dir_light.ambient = glm::vec3(0.08f, 0.10f, 0.12f);
    dir_light.diffuse = glm::vec3(0.25f, 0.28f, 0.32f);
    dir_light.specular = glm::vec3(1.0f, 1.0f, 1.0f);

    point_lights.clear();
    point_lights.push_back({ glm::vec3(-9.0f, 3.0f, -4.0f), glm::vec3(0.05f), glm::vec3(1.0f, 0.25f, 0.15f), glm::vec3(1.0f), 42.0f });
    point_lights.push_back({ glm::vec3(9.0f, 3.0f, -4.0f), glm::vec3(0.04f), glm::vec3(0.20f, 0.75f, 1.0f), glm::vec3(1.0f), 42.0f });
    point_lights.push_back({ glm::vec3(0.0f, 4.0f, -15.0f), glm::vec3(0.04f), glm::vec3(0.35f, 1.0f, 0.45f), glm::vec3(1.0f), 42.0f });
    point_lights.push_back({ glm::vec3(  0.0f, 8.5f, -12.5f), glm::vec3(0.015f, 0.045f, 0.035f), glm::vec3(0.18f, 0.95f, 0.62f), glm::vec3(0.25f, 0.9f, 0.65f), 38.0f });
    point_lights.push_back({ glm::vec3(-13.5f, 5.5f, -12.5f), glm::vec3(0.010f, 0.030f, 0.025f), glm::vec3(0.10f, 0.52f, 0.42f), glm::vec3(0.18f, 0.7f, 0.55f), 28.0f });
    point_lights.push_back({ glm::vec3( 13.5f, 5.5f, -12.5f), glm::vec3(0.010f, 0.030f, 0.025f), glm::vec3(0.10f, 0.52f, 0.42f), glm::vec3(0.18f, 0.7f, 0.55f), 28.0f });
    point_lights.push_back({ glm::vec3(  0.0f, 5.8f,   4.5f), glm::vec3(0.010f, 0.028f, 0.024f), glm::vec3(0.10f, 0.45f, 0.40f), glm::vec3(0.16f, 0.62f, 0.55f), 26.0f });

    SpotLight headlight;
    headlight.position = glm::vec3(0.0f, 0.0f, 0.0f);
    headlight.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    headlight.ambient = glm::vec3(0.02f, 0.02f, 0.02f);
    headlight.diffuse = glm::vec3(1.0f, 0.96f, 0.78f);
    headlight.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    headlight.cutoff = 15.0f;
    headlight.outer_cutoff = 23.0f;
    spot_lights.clear();
    spot_lights.push_back(headlight);

    auto tex_box = std::make_shared<Texture>("textures/box.png");
    auto tex_floor = std::make_shared<Texture>(glm::vec3(0.30f, 0.32f, 0.30f));
    auto tex_wall = std::make_shared<Texture>(glm::vec3(0.22f, 0.25f, 0.28f));
    auto tex_dark = std::make_shared<Texture>(glm::vec3(0.08f, 0.09f, 0.10f));
    auto tex_terminal = std::make_shared<Texture>(glm::vec3(0.12f, 0.45f, 0.55f));
    auto tex_enemy = std::make_shared<Texture>(glm::vec3(0.55f, 0.18f, 0.16f));
    auto tex_catwalk = std::make_shared<Texture>(glm::vec3(0.55f, 0.05f, 0.04f));
    auto tex_red_glass = std::make_shared<Texture>(glm::vec4(1.0f, 0.12f, 0.08f, 1.0f));
    auto tex_blue_glass = std::make_shared<Texture>(glm::vec4(0.15f, 0.45f, 1.0f, 1.0f));
    auto tex_green_glass = std::make_shared<Texture>(glm::vec4(0.25f, 1.0f, 0.25f, 1.0f));

    scene.clear();
    reactors.clear();
    enemies.clear();
    fire_sources.clear();
    hub_door_panels.clear();
    reactors_active = 0;
    gate_unlocked = false;

    add_box("spawn_floor", glm::vec3(0.0f, -0.05f, 25.0f), glm::vec3(8.0f, 0.1f, 12.0f), tex_floor);
    add_box("entry_corridor_floor", glm::vec3(0.0f, -0.05f, 12.0f), glm::vec3(7.2f, 0.1f, 25.0f), tex_floor);
    add_box("hub_pit_floor", glm::vec3(0.0f, -14.5f, -12.5f), glm::vec3(34.0f, 0.4f, 34.0f), tex_dark);
    // Left wing floors — wider corridor + T-junction + two reactor rooms
    add_box("left_main_floor",      glm::vec3(-33.0f, -0.05f, -12.5f), glm::vec3(22.0f, 0.1f, 10.0f), tex_floor);
    add_box("left_junction_floor",  glm::vec3(-50.5f, -0.05f, -18.0f), glm::vec3(13.0f, 0.1f, 32.0f), tex_floor);
    add_box("r1_corridor_floor",    glm::vec3(-62.0f, -0.05f,  -5.0f), glm::vec3(10.0f, 0.1f,  6.0f), tex_floor);
    add_box("reactor1_floor",       glm::vec3(-77.0f, -0.05f,  -3.0f), glm::vec3(20.0f, 0.1f, 22.0f), tex_floor);
    add_box("r2_corridor_floor",    glm::vec3(-62.0f, -0.05f, -29.0f), glm::vec3(10.0f, 0.1f,  6.0f), tex_floor);
    add_box("reactor2_floor",       glm::vec3(-77.0f, -0.05f, -29.0f), glm::vec3(20.0f, 0.1f, 18.0f), tex_floor);
    // Right wing floors
    add_box("east_corridor_floor",  glm::vec3( 33.5f, -0.05f, -12.5f), glm::vec3(23.0f, 0.1f, 10.0f), tex_floor);
    add_box("warehouse_floor",      glm::vec3( 63.0f, -0.05f, -17.0f), glm::vec3(36.0f, 0.1f, 46.0f), tex_floor);
    add_box("reactor3_floor",       glm::vec3( 89.0f, -0.05f, -18.0f), glm::vec3(14.0f, 0.1f, 22.0f), tex_floor);
    add_box("escape_corridor_floor", glm::vec3(0.0f, -0.05f, -37.0f), glm::vec3(8.5f, 0.1f, 18.0f), tex_floor);

    add_box("spawn_room_back_wall", glm::vec3(0.0f, 2.0f, 31.2f), glm::vec3(7.8f, 4.0f, 0.7f), tex_dark, true, 2.0f);
    add_box("spawn_room_left_wall", glm::vec3(-4.0f, 2.0f, 25.0f), glm::vec3(0.8f, 4.0f, 12.0f), tex_wall, true, 1.5f);
    add_box("spawn_room_right_wall", glm::vec3(4.0f, 2.0f, 25.0f), glm::vec3(0.8f, 4.0f, 12.0f), tex_wall, true, 1.5f);
    add_box("sealed_start_door", glm::vec3(0.0f, 2.0f, 28.0f), glm::vec3(6.5f, 4.0f, 0.7f), tex_dark, true, 2.0f);
    add_box("entry_cover_a", glm::vec3(-1.9f, 0.6f, 5.0f), glm::vec3(1.3f, 1.2f, 1.3f), tex_box, true, 1.0f);
    add_box("entry_cover_b", glm::vec3(2.0f, 0.6f, -0.5f), glm::vec3(1.3f, 1.2f, 1.3f), tex_box, true, 1.0f);

    const glm::vec3 hub_center(0.0f, 0.0f, -12.5f);

    // Solid hexagonal panel dome; portal panels are replaced by tunnel prisms.
    auto tex_hub_shell = std::make_shared<Texture>(glm::vec3(0.06f, 0.30f, 0.22f));
    auto hub_shell = std::make_shared<Model>("objects/hub_shell_portals.obj", shader_prog, tex_hub_shell);
    hub_shell->pivot_position = hub_center;
    hub_shell->scale = glm::vec3(1.45f);
    hub_shell->is_transparent = false;
    hub_shell->material_alpha = 1.0f;
    scene["hub_hex_shell"] = hub_shell;

    auto hub_shell_edges = std::make_shared<Model>("objects/hub_shell_edges.obj", shader_prog, tex_dark);
    hub_shell_edges->pivot_position = hub_center;
    hub_shell_edges->scale = glm::vec3(1.45f);
    scene["hub_hex_shell_edges"] = hub_shell_edges;

    constexpr float hub_dome_model_scale = 1.45f;
    constexpr float hub_portal_panel_radius = 16.3f * hub_dome_model_scale;
    constexpr float hub_portal_panel_y = 1.7931034483f * hub_dome_model_scale;
    constexpr float hub_portal_hex_radius = 2.35f * hub_dome_model_scale;

    auto tex_tunnel_shell = std::make_shared<Texture>(glm::vec3(0.08f, 0.48f, 0.34f));
    auto add_hex_tunnel_shell = [&](const std::string& name, glm::vec3 position, float length, float yaw_degrees) {
        auto tunnel = std::make_shared<Model>("objects/hex_tunnel_shell.obj", shader_prog, tex_tunnel_shell);
        tunnel->pivot_position = position;
        tunnel->eulerAngles.y = yaw_degrees;
        tunnel->scale = glm::vec3(length, hub_portal_hex_radius, hub_portal_hex_radius);
        tunnel->is_transparent = false;
        tunnel->material_alpha = 1.0f;
        scene[name] = tunnel;
        return tunnel;
    };

    constexpr float west_tunnel_len = 20.2f;
    constexpr float east_tunnel_len = 20.8f;
    constexpr float south_tunnel_len = 16.8f;
    constexpr float north_tunnel_len = 9.8f;

    add_hex_tunnel_shell("hub_tunnel_shell_west",  glm::vec3(-hub_portal_panel_radius - west_tunnel_len * 0.5f, hub_portal_panel_y, hub_center.z), west_tunnel_len, 0.0f);
    add_hex_tunnel_shell("hub_tunnel_shell_east",  glm::vec3( hub_portal_panel_radius + east_tunnel_len * 0.5f, hub_portal_panel_y, hub_center.z), east_tunnel_len, 0.0f);
    add_hex_tunnel_shell("hub_tunnel_shell_south", glm::vec3(0.0f, hub_portal_panel_y, hub_center.z + hub_portal_panel_radius + south_tunnel_len * 0.5f), south_tunnel_len, 90.0f);
    add_hex_tunnel_shell("hub_tunnel_shell_north", glm::vec3(0.0f, hub_portal_panel_y, hub_center.z - hub_portal_panel_radius - north_tunnel_len * 0.5f), north_tunnel_len, 90.0f);

    auto tex_collision_clear = std::make_shared<Texture>(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
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

    // Hub catwalks: center octagon gives flat entrances in all four directions.
    // The diagonal rail pieces are visual because collision is axis-aligned.
    constexpr float hub_oct_radius = 6.5f;
    constexpr float hub_oct_apothem = hub_oct_radius * 0.9238795f;
    constexpr float hub_oct_corner = hub_oct_radius * 0.3826834f;

    auto hub_platform = std::make_shared<Model>("objects/oct_platform.obj", shader_prog, tex_catwalk);
    hub_platform->pivot_position = glm::vec3(hub_center.x, 0.16f, hub_center.z);
    hub_platform->scale = glm::vec3(hub_oct_radius, 0.32f, hub_oct_radius);
    scene["hub_platform_center_oct"] = hub_platform;

    add_box("hub_catwalk_west",  glm::vec3(-14.5f, 0.12f, hub_center.z), glm::vec3(17.0f, 0.24f, 2.0f), tex_catwalk);
    add_box("hub_catwalk_east",  glm::vec3( 14.5f, 0.12f, hub_center.z), glm::vec3(17.0f, 0.24f, 2.0f), tex_catwalk);
    add_box("hub_catwalk_south", glm::vec3(  0.0f, 0.12f, 1.25f), glm::vec3(2.0f, 0.24f, 15.5f), tex_catwalk);
    add_box("hub_catwalk_north", glm::vec3(  0.0f, 0.12f, -24.75f), glm::vec3(2.0f, 0.24f, 12.5f), tex_catwalk);

    auto add_hub_rail = [&](const std::string& name, glm::vec3 position, float length, float angle_degrees, bool collides = false) {
        auto rail = add_box(name, position, glm::vec3(length, 0.25f, 0.22f), tex_dark, collides, 1.0f);
        rail->eulerAngles.y = angle_degrees;
        return rail;
    };

    auto add_hub_rail_between = [&](const std::string& name, glm::vec2 a, glm::vec2 b, bool collides = false, float angle_offset = 0.0f) {
        const glm::vec2 mid = (a + b) * 0.5f;
        const glm::vec2 delta = b - a;
        const float length = glm::length(delta);
        const float angle_degrees = glm::degrees(std::atan2(delta.y, delta.x)) + angle_offset;
        return add_hub_rail(name, glm::vec3(mid.x, 0.9f, hub_center.z + mid.y), length, angle_degrees, collides);
    };

    const float rail_y = 0.9f;
    add_box("hub_oct_rail_west_north", glm::vec3(-hub_oct_apothem, rail_y, hub_center.z - 1.8f), glm::vec3(0.22f, 0.25f, hub_oct_corner - 1.1f), tex_dark, true, 1.0f);
    add_box("hub_oct_rail_west_south", glm::vec3(-hub_oct_apothem, rail_y, hub_center.z + 1.8f), glm::vec3(0.22f, 0.25f, hub_oct_corner - 1.1f), tex_dark, true, 1.0f);
    add_box("hub_oct_rail_east_north", glm::vec3( hub_oct_apothem, rail_y, hub_center.z - 1.8f), glm::vec3(0.22f, 0.25f, hub_oct_corner - 1.1f), tex_dark, true, 1.0f);
    add_box("hub_oct_rail_east_south", glm::vec3( hub_oct_apothem, rail_y, hub_center.z + 1.8f), glm::vec3(0.22f, 0.25f, hub_oct_corner - 1.1f), tex_dark, true, 1.0f);
    add_hub_rail_between("hub_oct_rail_south_left",  glm::vec2(-hub_oct_corner,  hub_oct_apothem), glm::vec2(-1.1f,  hub_oct_apothem), true);
    add_hub_rail_between("hub_oct_rail_south_right", glm::vec2( 1.1f,  hub_oct_apothem), glm::vec2( hub_oct_corner,  hub_oct_apothem), true);
    add_hub_rail_between("hub_oct_rail_north_left",  glm::vec2(-hub_oct_corner, -hub_oct_apothem), glm::vec2(-1.1f, -hub_oct_apothem), true);
    add_hub_rail_between("hub_oct_rail_north_right", glm::vec2( 1.1f, -hub_oct_apothem), glm::vec2( hub_oct_corner, -hub_oct_apothem), true);
    add_hub_rail_between("hub_oct_rail_sw", glm::vec2(-hub_oct_apothem,  hub_oct_corner), glm::vec2(-hub_oct_corner,  hub_oct_apothem), false, 90.0f);
    add_hub_rail_between("hub_oct_rail_nw", glm::vec2(-hub_oct_apothem, -hub_oct_corner), glm::vec2(-hub_oct_corner, -hub_oct_apothem), false, 90.0f);
    add_hub_rail_between("hub_oct_rail_ne", glm::vec2( hub_oct_apothem, -hub_oct_corner), glm::vec2( hub_oct_corner, -hub_oct_apothem), false, 90.0f);
    add_hub_rail_between("hub_oct_rail_se", glm::vec2( hub_oct_apothem,  hub_oct_corner), glm::vec2( hub_oct_corner,  hub_oct_apothem), false, 90.0f);
    add_hub_rail("hub_rail_west_north", glm::vec3(-14.5f, rail_y, hub_center.z - 1.0f), 17.0f, 0.0f, true);
    add_hub_rail("hub_rail_west_south", glm::vec3(-14.5f, rail_y, hub_center.z + 1.0f), 17.0f, 0.0f, true);
    add_hub_rail("hub_rail_east_north", glm::vec3( 14.5f, rail_y, hub_center.z - 1.0f), 17.0f, 0.0f, true);
    add_hub_rail("hub_rail_east_south", glm::vec3( 14.5f, rail_y, hub_center.z + 1.0f), 17.0f, 0.0f, true);
    add_box("hub_rail_south_left",  glm::vec3(-1.0f, rail_y, 1.25f), glm::vec3(0.22f, 0.25f, 15.5f), tex_dark, true, 1.0f);
    add_box("hub_rail_south_right", glm::vec3( 1.0f, rail_y, 1.25f), glm::vec3(0.22f, 0.25f, 15.5f), tex_dark, true, 1.0f);
    add_box("hub_rail_north_left",  glm::vec3(-1.0f, rail_y, -24.75f), glm::vec3(0.22f, 0.25f, 12.5f), tex_dark, true, 1.0f);
    add_box("hub_rail_north_right", glm::vec3( 1.0f, rail_y, -24.75f), glm::vec3(0.22f, 0.25f, 12.5f), tex_dark, true, 1.0f);

    // === Left wing: main corridor → T-junction → Reactor 1 (south) + Reactor 2 (north) ===
    // Main corridor (x=-25 to -44, z=-17.5 to -7.5)

    // T-junction room (x=-44 to -57, z=-34 to -2)
    add_box("left_junc_north_wall",  glm::vec3(-50.5f, 2.0f, -34.5f), glm::vec3(13.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    add_box("left_junc_south_wall",  glm::vec3(-50.5f, 2.0f,  -1.5f), glm::vec3(13.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    // East wall of junction — gap at z=-17.5 to -7.5 for corridor entry
    add_box("left_junc_east_n",      glm::vec3(-44.5f, 2.0f, -26.0f), glm::vec3(0.8f, 4.0f, 16.0f), tex_wall, true, 1.5f);
    add_box("left_junc_east_s",      glm::vec3(-44.5f, 2.0f,  -4.5f), glm::vec3(0.8f, 4.0f,  5.0f), tex_wall, true, 1.5f);
    // West wall of junction — two gaps: R2 at z=-32 to -26, R1 at z=-8 to -2
    add_box("left_junc_west_n",      glm::vec3(-57.0f, 2.0f, -33.0f), glm::vec3(0.8f, 4.0f,  2.0f), tex_wall, true, 1.5f);
    add_box("left_junc_west_mid",    glm::vec3(-57.0f, 2.0f, -17.0f), glm::vec3(0.8f, 4.0f, 18.0f), tex_wall, true, 1.5f);

    // Reactor 1 corridor (south arm, z=-8 to -2, x=-57 to -67)
    add_box("r1_corr_north_wall",    glm::vec3(-62.0f, 2.0f,  -8.5f), glm::vec3(10.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    add_box("r1_corr_south_wall",    glm::vec3(-62.0f, 2.0f,  -1.5f), glm::vec3(10.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);

    // Reactor 1 room (x=-67 to -87, z=-14 to 8) — entry from east at z=-8 to -2
    add_box("reactor1_room_west",    glm::vec3(-87.5f, 2.0f,  -3.0f), glm::vec3(0.8f, 4.0f, 22.0f), tex_wall, true, 1.5f);
    add_box("reactor1_room_north",   glm::vec3(-77.0f, 2.0f, -14.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_wall, true, 1.5f);
    add_box("reactor1_room_south",   glm::vec3(-77.0f, 2.0f,   8.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_wall, true, 1.5f);
    add_box("reactor1_entry_n",      glm::vec3(-67.5f, 2.0f, -11.0f), glm::vec3(0.8f, 4.0f,  6.0f), tex_wall, true, 1.5f);
    add_box("reactor1_entry_s",      glm::vec3(-67.5f, 2.0f,   5.0f), glm::vec3(0.8f, 4.0f,  6.0f), tex_wall, true, 1.5f);

    // Reactor 2 corridor (north arm, z=-32 to -26, x=-57 to -67)
    add_box("r2_corr_north_wall",    glm::vec3(-62.0f, 2.0f, -32.5f), glm::vec3(10.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    add_box("r2_corr_south_wall",    glm::vec3(-62.0f, 2.0f, -25.5f), glm::vec3(10.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);

    // Reactor 2 room (x=-67 to -87, z=-38 to -20) — entry from east at z=-32 to -26
    add_box("reactor2_room_west",    glm::vec3(-87.5f, 2.0f, -29.0f), glm::vec3(0.8f, 4.0f, 18.0f), tex_wall, true, 1.5f);
    add_box("reactor2_room_north",   glm::vec3(-77.0f, 2.0f, -38.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_wall, true, 1.5f);
    add_box("reactor2_room_south",   glm::vec3(-77.0f, 2.0f, -19.5f), glm::vec3(20.0f, 4.0f,  0.8f), tex_wall, true, 1.5f);
    add_box("reactor2_entry_n",      glm::vec3(-67.5f, 2.0f, -35.0f), glm::vec3(0.8f, 4.0f,  6.0f), tex_wall, true, 1.5f);
    add_box("reactor2_entry_s",      glm::vec3(-67.5f, 2.0f, -23.0f), glm::vec3(0.8f, 4.0f,  6.0f), tex_wall, true, 1.5f);

    // === Right wing: east corridor → large warehouse → Reactor 3 behind hidden door ===
    // East corridor (x=25 to 45, z=-17.5 to -7.5)

    // Warehouse room (x=45 to 81, z=-40 to 6)
    add_box("warehouse_north_wall",  glm::vec3( 63.0f, 2.0f, -40.5f), glm::vec3(36.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    add_box("warehouse_south_wall",  glm::vec3( 63.0f, 2.0f,   6.5f), glm::vec3(36.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    // West wall — gap at z=-17.5 to -7.5 for corridor entry
    add_box("warehouse_west_n",      glm::vec3( 44.5f, 2.0f, -29.0f), glm::vec3(0.8f, 4.0f, 22.0f), tex_wall, true, 1.5f);
    add_box("warehouse_west_s",      glm::vec3( 44.5f, 2.0f,  -1.0f), glm::vec3(0.8f, 4.0f,  8.0f), tex_wall, true, 1.5f);
    // East wall — solid, with a hidden door panel at z=-28 to -20
    add_box("warehouse_east_n",      glm::vec3( 81.5f, 2.0f, -34.0f), glm::vec3(0.8f, 4.0f, 12.0f), tex_wall, true, 1.5f);
    add_box("warehouse_east_s",      glm::vec3( 81.5f, 2.0f,  -7.0f), glm::vec3(0.8f, 4.0f, 26.0f), tex_wall, true, 1.5f);
    hidden_door_wall = add_box("warehouse_door_panel", glm::vec3( 81.5f, 2.0f, -24.0f), glm::vec3(0.8f, 4.0f, 8.0f), tex_dark, true, 1.5f);
    hidden_door_btn  = add_box("warehouse_secret_btn", glm::vec3( 78.0f, 0.55f,-38.0f), glm::vec3(0.6f, 0.3f, 0.6f), tex_terminal, true, 0.5f);

    // Crates inside warehouse
    add_box("warehouse_crate_a",     glm::vec3( 55.0f, 0.8f, -30.0f), glm::vec3(3.4f, 1.6f, 3.4f), tex_box, true, 1.6f);
    add_box("warehouse_crate_b",     glm::vec3( 68.0f, 0.8f,  -5.0f), glm::vec3(4.2f, 1.6f, 3.0f), tex_box, true, 1.6f);
    add_box("warehouse_crate_c",     glm::vec3( 72.0f, 1.5f, -32.0f), glm::vec3(2.4f, 3.0f, 2.4f), tex_box, true, 1.5f);
    add_box("warehouse_terminal_l",  glm::vec3( 50.0f, 1.0f, -10.0f), glm::vec3(2.4f, 2.0f, 2.5f), tex_dark, true, 1.4f);
    add_box("warehouse_terminal_r",  glm::vec3( 75.0f, 1.0f, -35.0f), glm::vec3(2.4f, 2.0f, 2.5f), tex_dark, true, 1.4f);

    // Reactor 3 room (x=81 to 96, z=-40 to -8) — west wall is shared with warehouse east wall above
    add_box("reactor3_room_north",   glm::vec3( 88.5f, 2.0f, -40.5f), glm::vec3(14.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    add_box("reactor3_room_south",   glm::vec3( 88.5f, 2.0f,  -7.5f), glm::vec3(14.0f, 4.0f, 0.8f), tex_wall, true, 1.5f);
    add_box("reactor3_room_east",    glm::vec3( 96.5f, 2.0f, -24.0f), glm::vec3(0.8f, 4.0f, 32.0f), tex_wall, true, 1.5f);

    add_box("gate_frame_left", glm::vec3(-5.6f, 2.0f, -39.0f), glm::vec3(1.0f, 4.0f, 2.4f), tex_dark, true, 1.5f);
    add_box("gate_frame_right", glm::vec3(5.6f, 2.0f, -39.0f), glm::vec3(1.0f, 4.0f, 2.4f), tex_dark, true, 1.5f);

    auto tex_pedestal = std::make_shared<Texture>(glm::vec3(0.11f, 0.13f, 0.16f));
    auto hub_pedestal = std::make_shared<Model>("objects/oct_pedestal.obj", shader_prog, tex_pedestal);
    hub_pedestal->pivot_position = hub_center;
    scene["hub_orb_pedestal"] = hub_pedestal;

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

    Enemy e1{ add_box("enemy_01", glm::vec3( -2.8f, 0.8f,  -6.2f), glm::vec3(0.8f, 1.6f, 0.8f), tex_enemy, true, 1.0f), 3, 0.0f };
    Enemy e2{ add_box("enemy_02", glm::vec3(-50.0f, 0.8f, -12.5f), glm::vec3(0.8f, 1.6f, 0.8f), tex_enemy, true, 1.0f), 3, 1.7f };
    Enemy e3{ add_box("enemy_03", glm::vec3( 60.0f, 0.8f, -20.0f), glm::vec3(0.8f, 1.6f, 0.8f), tex_enemy, true, 1.0f), 3, 3.4f };
    Enemy e4{ add_box("enemy_04", glm::vec3(  0.0f, 0.8f, -28.0f), glm::vec3(0.8f, 1.6f, 0.8f), tex_enemy, true, 1.0f), 4, 5.1f };
    enemies = { e1, e2, e3, e4 };

    model = add_box("levitating_orb", glm::vec3(0.0f, 3.7f, -12.5f), glm::vec3(3.9f), tex_terminal, false, 1.0f, true, 0.72f);
    fire_sources = {
        glm::vec3( -7.0f, 0.1f,  -9.0f),
        glm::vec3(  7.0f, 0.1f, -16.0f),
        glm::vec3(-50.0f, 0.1f,  -5.0f),
        glm::vec3(-50.0f, 0.1f, -29.0f),
        glm::vec3( 58.0f, 0.1f, -32.0f),
        glm::vec3( 70.0f, 0.1f,  -5.0f),
        glm::vec3(  0.0f, 0.1f, -31.0f),
        glm::vec3( 89.0f, 0.1f, -18.0f)
    };

    auto tex_particle = std::make_shared<Texture>(glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));
    particle_template = std::make_shared<Model>("objects/tetrahedron.obj", shader_prog, tex_particle);
    particle_template->scale = glm::vec3(0.1f);
    particle_template->is_transparent = true;
    particle_template->material_alpha = 0.8f;
}

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



int App::run(void)
{
	try {
		shader_prog->use();

		double now = glfwGetTime();
		double fps_last_displayed = now;
		int fps_counter_frames = 0;
		double FPS = 0.0;

		glClearColor(0, 0, 0, 0);

		glCullFace(GL_BACK);
		glDisable(GL_CULL_FACE);

		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwGetCursorPos(window, &cursorLastX, &cursorLastY);

		update_projection_matrix();
		glViewport(0, 0, width, height);

		camera.Position = glm::vec3(0.0f, 1.2f, 25.5f);
		double last_frame_time = glfwGetTime();

		while (!glfwWindowShouldClose(window))
		{
			if (show_imgui) {
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				ImGui::SetNextWindowPos(ImVec2(10, 10));
				ImGui::SetNextWindowSize(ImVec2(360, 210));

				ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
				ImGui::Text("FPS: %.1f", FPS);
				ImGui::Text("Health: %d", player_health);
				ImGui::Text("Reactors: %d/3", reactors_active);
				ImGui::Text("Gate: %s", gate_unlocked ? "UNLOCKED" : "LOCKED");
				ImGui::Text("Collision: %s (N)", collisions_enabled ? "ON" : "NOCLIP");
				ImGui::Text("%s", hud_message.c_str());
				ImGui::Text("V-Sync: %s (hit V to toggle)", is_vsync_on ? "ON" : "OFF");
				ImGui::Text("Multisample (AA): %s (hit M to toggle)", is_multisample_on ? "ON" : "OFF");
				ImGui::Text("LMB fire | E reactor button | P screenshot");
				ImGui::Text("(press RMB to release mouse)");
				ImGui::Text("(hit G to show/hide info)");
				ImGui::End();
			}

			now = glfwGetTime();
			float delta_t = static_cast<float>(now - last_frame_time);
			last_frame_time = now;

			camera.ProcessInput(window, delta_t);
			update_gameplay(delta_t, now);
			apply_collisions(delta_t);

			if (model) {
				model->eulerAngles.y = now * 50.0f;
				model->eulerAngles.x = now * 30.0f;
			}

			if (!spot_lights.empty()) {
				spot_lights[0].position = camera.Position + camera.Front * 0.18f - camera.Up * 0.12f;
				spot_lights[0].direction = glm::normalize(camera.Front);
			}

			update_particles(delta_t);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shader_prog->use();
			const glm::mat4 view = camera.GetViewMatrix();
			shader_prog->setUniform("uV_m", view);
			shader_prog->setUniform("uP_m", projection_matrix);

			shader_prog->setUniform("dir_light_direction", glm::normalize(glm::mat3(view) * dir_light.direction));
			shader_prog->setUniform("dir_light_ambient", dir_light.ambient);
			shader_prog->setUniform("dir_light_diffuse", dir_light.diffuse);
			shader_prog->setUniform("dir_light_specular", dir_light.specular);

			const int uploaded_point_lights = static_cast<int>(std::min<size_t>(point_lights.size(), 8));
			shader_prog->setUniform("num_point_lights", uploaded_point_lights);
			for (int i = 0; i < uploaded_point_lights; i++) {
				std::string idx = std::to_string(i);
				shader_prog->setUniform("point_light_position[" + idx + "]", glm::vec3(view * glm::vec4(point_lights[i].position, 1.0f)));
				shader_prog->setUniform("point_light_ambient[" + idx + "]", point_lights[i].ambient);
				shader_prog->setUniform("point_light_diffuse[" + idx + "]", point_lights[i].diffuse);
				shader_prog->setUniform("point_light_specular[" + idx + "]", point_lights[i].specular);
				shader_prog->setUniform("point_light_radius[" + idx + "]", point_lights[i].radius);
			}

			if (!spot_lights.empty()) {
				shader_prog->setUniform("spot_light_position", glm::vec3(view * glm::vec4(spot_lights[0].position, 1.0f)));
				shader_prog->setUniform("spot_light_direction", glm::normalize(glm::mat3(view) * spot_lights[0].direction));
				shader_prog->setUniform("spot_light_ambient", spot_lights[0].ambient);
				shader_prog->setUniform("spot_light_diffuse", spot_lights[0].diffuse);
				shader_prog->setUniform("spot_light_specular", spot_lights[0].specular);
				shader_prog->setUniform("spot_light_cutoff", spot_lights[0].cutoff);
				shader_prog->setUniform("spot_light_outer_cutoff", spot_lights[0].outer_cutoff);
			}

			{
				std::vector<std::shared_ptr<Model>> transparent;
				transparent.reserve(scene.size());

				for (auto& [name, model_obj] : scene) {
					if (!model_obj->is_transparent) {
						model_obj->draw();
					}
					else {
						transparent.emplace_back(model_obj);
					}
				}

				std::sort(transparent.begin(), transparent.end(),
					[&](std::shared_ptr<Model> const a, std::shared_ptr<Model> const b) {
						return glm::distance(camera.Position, a->getPosition()) > glm::distance(camera.Position, b->getPosition());
					});

				glEnable(GL_BLEND);
				glDepthMask(GL_FALSE);

				for (auto& p : transparent) {
					p->draw();
				}

				glDepthMask(GL_TRUE);
				glDisable(GL_BLEND);
			}

			draw_particles();

			if (show_imgui) {
				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			}

			glfwSwapBuffers(window);

			glfwPollEvents();

			fps_counter_frames++;
			if (now - fps_last_displayed >= 1.0) {
				FPS = fps_counter_frames / (now - fps_last_displayed);
				fps_last_displayed = now;
				fps_counter_frames = 0;
				std::cout << "\r[FPS]" << FPS << "     ";
			}
		}
	}

	catch (std::exception const& e) {
		std::cerr << "App failed : " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void App::destroy(void)
{
    if (ImGui::GetCurrentContext()) {
	    ImGui_ImplOpenGL3_Shutdown();
	    ImGui_ImplGlfw_Shutdown();
	    ImGui::DestroyContext();
    }

	cv::destroyAllWindows();

	if (window) {
		glfwDestroyWindow(window);
		window = nullptr;
	}
	glfwTerminate();
}

App::~App()
{
	destroy();

	std::cout << "Bye...\n";
}

void App::update_gameplay(float delta_t, double now)
{
	camera.MovementSpeed = 5.0f;

	for (size_t i = 0; i < reactors.size() && i < point_lights.size(); ++i) {
		auto& reactor = reactors[i];
		const float angle = static_cast<float>(now) * 4.0f + static_cast<float>(i) * glm::two_pi<float>() / 3.0f;
		const float pulse = reactor.active ? 1.15f + 0.55f * std::sin(static_cast<float>(now) * 8.0f + static_cast<float>(i)) : 0.25f;
		const glm::vec3 base = reactor.active ? glm::vec3(0.15f, 1.0f, 0.25f) : glm::vec3(0.25f, 0.08f, 0.06f);
		const glm::vec3 orbit = reactor.active ? glm::vec3(std::cos(angle) * 1.1f, 0.0f, std::sin(angle) * 1.1f) : glm::vec3(0.0f);
		point_lights[i].position = reactor.model->pivot_position + glm::vec3(0.0f, 1.8f + 0.25f * std::sin(static_cast<float>(now) * 2.5f), 0.0f) + orbit;
		point_lights[i].diffuse = base * pulse;
		point_lights[i].ambient = base * 0.08f;

		if (reactor.active) {
			reactor.model->eulerAngles.y += delta_t * 120.0f;
			reactor.model->material_alpha = 0.78f + 0.18f * std::sin(static_cast<float>(now) * 10.0f);
			if (reactor.button) {
				reactor.button->eulerAngles.y += delta_t * 180.0f;
				reactor.button->material_alpha = 1.0f;
			}
		}
	}

	for (auto& enemy : enemies) {
		if (!enemy.alive || !enemy.model) {
			continue;
		}

		const float bob = std::sin(static_cast<float>(now) * 2.2f + enemy.bob_offset) * 0.15f;
		enemy.model->pivot_position.y = 0.8f + bob;
		enemy.model->eulerAngles.y += delta_t * 40.0f;

		const float distance_to_player = glm::distance(camera.Position, enemy.model->pivot_position);
		if (distance_to_player < 1.2f) {
			player_health = std::max(0, player_health - 1);
			hud_message = "Specimen contact detected.";
		}
	}

	if (now - last_fire_particle_time > 0.08) {
		for (const auto& source : fire_sources) {
			spawn_particles(source + glm::vec3(0.0f, 0.35f, 0.0f), 2);
		}
		last_fire_particle_time = now;
	}

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

	if (hidden_door_open && hidden_door_wall) {
		hidden_door_wall->pivot_position.y = std::min(6.0f, hidden_door_wall->pivot_position.y + delta_t * 2.0f);
		hidden_door_wall->collides = false;
	}
}

void App::activate_nearest_reactor()
{
	float best_distance = std::numeric_limits<float>::max();
	Reactor* nearest = nullptr;

	for (auto& reactor : reactors) {
		if (reactor.active || !reactor.button) {
			continue;
		}

		const float distance = glm::distance(camera.Position, reactor.button->pivot_position);
		if (distance < best_distance) {
			best_distance = distance;
			nearest = &reactor;
		}
	}

	// Check hidden door button
	if (!hidden_door_open && hidden_door_btn) {
		const float dist = glm::distance(camera.Position, hidden_door_btn->pivot_position);
		if (dist < 2.4f) {
			hidden_door_open = true;
			hud_message = "Hidden passage unlocked.";
			return;
		}
	}

	if (!nearest || best_distance > 2.4f) {
		hud_message = "No reactor button in range.";
		return;
	}

	nearest->active = true;
	nearest->model->material_alpha = 0.85f;
	nearest->model->collides = false;
	if (nearest->button) {
		nearest->button->collides = false;
		nearest->button->scale = glm::vec3(0.75f, 0.18f, 0.75f);
	}
	reactors_active++;
	spawn_particles(nearest->model->pivot_position + glm::vec3(0.0f, 1.2f, 0.0f), 30);

	if (reactors_active >= static_cast<int>(reactors.size())) {
		gate_unlocked = true;
		hud_message = "Containment gate unlocked.";
	} else {
		hud_message = "Reactor online.";
	}
}

void App::fire_weapon()
{
	const double now = glfwGetTime();
	if (now - last_shot_time < 0.18) {
		return;
	}
	last_shot_time = now;

	const glm::vec3 ray_origin = camera.Position;
	const glm::vec3 ray_dir = glm::normalize(camera.Front);
	float best_hit = std::numeric_limits<float>::max();
	Enemy* hit_enemy = nullptr;

	for (auto& enemy : enemies) {
		if (!enemy.alive || !enemy.model) {
			continue;
		}

		float hit_distance = 0.0f;
		if (ray_hits_sphere(ray_origin, ray_dir, enemy.model->pivot_position, enemy.model->bounding_radius, hit_distance) && hit_distance < best_hit) {
			best_hit = hit_distance;
			hit_enemy = &enemy;
		}
	}

	if (!hit_enemy) {
		hud_message = "Shot missed.";
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
		hud_message = "Specimen neutralized.";
	} else {
		hud_message = "Specimen hit.";
	}
}

bool App::ray_hits_sphere(const glm::vec3& ray_origin,
                          const glm::vec3& ray_dir,
                          const glm::vec3& sphere_center,
                          float sphere_radius,
                          float& hit_distance) const
{
	const glm::vec3 oc = ray_origin - sphere_center;
	const float b = 2.0f * glm::dot(oc, ray_dir);
	const float c = glm::dot(oc, oc) - sphere_radius * sphere_radius;
	const float discriminant = b * b - 4.0f * c;

	if (discriminant < 0.0f) {
		return false;
	}

	const float root = std::sqrt(discriminant);
	const float t1 = (-b - root) * 0.5f;
	const float t2 = (-b + root) * 0.5f;
	hit_distance = t1 > 0.0f ? t1 : t2;
	return hit_distance > 0.0f && hit_distance < 35.0f;
}

void App::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
	if ((action == GLFW_PRESS) || (action == GLFW_REPEAT)) {
		switch (key) {
		case GLFW_KEY_ESCAPE:
			{
				int mode = glfwGetInputMode(window, GLFW_CURSOR);
				if (mode == GLFW_CURSOR_DISABLED) {
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				}
				else {
					glfwSetWindowShouldClose(window, GLFW_TRUE);
				}
			}
			break;
		case GLFW_KEY_C:
			this_inst->bg_r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			this_inst->bg_g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			this_inst->bg_b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			break;
		case GLFW_KEY_V:
			this_inst->is_vsync_on = !this_inst->is_vsync_on;
			glfwSwapInterval(this_inst->is_vsync_on ? 1 : 0);
			std::cout << "VSync: " << (this_inst->is_vsync_on ? "ON" : "OFF") << "\n";
			break;
		case GLFW_KEY_F:
			this_inst->toggle_fullscreen();
			break;
		case GLFW_KEY_G:
			this_inst->show_imgui = !this_inst->show_imgui;
			break;
		case GLFW_KEY_N:
			if (action == GLFW_PRESS) {
				this_inst->collisions_enabled = !this_inst->collisions_enabled;
				this_inst->hud_message = this_inst->collisions_enabled ? "Collision enabled." : "Noclip enabled.";
			}
			break;
		case GLFW_KEY_E:
			this_inst->activate_nearest_reactor();
			break;
		case GLFW_KEY_M:
			this_inst->is_multisample_on = !this_inst->is_multisample_on;
			if (this_inst->is_multisample_on) glEnable(GL_MULTISAMPLE);
			else glDisable(GL_MULTISAMPLE);
			std::cout << "Multisampling: " << (this_inst->is_multisample_on ? "ON" : "OFF") << "\n";
			break;
		case GLFW_KEY_P:
			this_inst->take_screenshot();
			break;

		case GLFW_KEY_TAB:
			{
				int mode = glfwGetInputMode(window, GLFW_CURSOR);
				if (mode == GLFW_CURSOR_DISABLED)
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				else
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			break;
		default:
			break;
		}
	}
}

void App::glfw_fbsize_callback(GLFWwindow* window, int width, int height) {
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    this_inst->width = width;
    this_inst->height = height;

    glViewport(0, 0, width, height);
    this_inst->update_projection_matrix();
}

void App::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (action == GLFW_PRESS) {
		switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT: {
			int mode = glfwGetInputMode(window, GLFW_CURSOR);
			if (mode == GLFW_CURSOR_NORMAL) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			} else {
				auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
				this_inst->fire_weapon();
			}
			break;
		}
		case GLFW_MOUSE_BUTTON_RIGHT:
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		default:
			break;
		}
	}
}

void App::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
		app->firstMouse = true;
		return;
	}

    if (app->firstMouse) {
        app->cursorLastX = xpos;
        app->cursorLastY = ypos;
        app->firstMouse = false;
    }

    app->camera.ProcessMouseMovement(xpos - app->cursorLastX, (ypos - app->cursorLastY) * -1.0);
    app->cursorLastX = xpos;
    app->cursorLastY = ypos;
}

void App::glfw_cursor_position_callback(GLFWwindow* window, double xposIn, double yposIn) {
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));

	if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
		return;

	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (this_inst->firstMouse) {
		this_inst->lastX = xpos;
		this_inst->lastY = ypos;
		this_inst->firstMouse = false;
	}

	float xoffset = xpos - this_inst->lastX;
	float yoffset = this_inst->lastY - ypos;
	this_inst->lastX = xpos;
	this_inst->lastY = ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	this_inst->yaw += xoffset;
	this_inst->pitch += yoffset;

	if (this_inst->pitch > 89.0f)
		this_inst->pitch = 89.0f;
	if (this_inst->pitch < -89.0f)
		this_inst->pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(this_inst->yaw)) * cos(glm::radians(this_inst->pitch));
	front.y = sin(glm::radians(this_inst->pitch));
	front.z = sin(glm::radians(this_inst->yaw)) * cos(glm::radians(this_inst->pitch));
	this_inst->camera_front = glm::normalize(front);
}




void App::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	(void)xoffset;
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    this_inst->fov -= static_cast<float>(yoffset) * 3.0f;
    this_inst->fov = std::clamp(this_inst->fov, 35.0f, 90.0f);

    this_inst->update_projection_matrix();
}

void App::update_projection_matrix(void)
{
    if (height < 1)
        height = 1;

    fov = std::clamp(fov, 35.0f, 90.0f);
    float ratio = static_cast<float>(width) / height;

    projection_matrix = glm::perspective(
        glm::radians(fov),
        ratio,
        0.1f,
        20000.0f
    );
}

void App::toggle_fullscreen() {
    if (!fullscreen_enabled) {
		int xpos, ypos, width, height;
		glfwGetWindowPos(window, &xpos, &ypos);
		glfwGetWindowSize(window, &width, &height);

		int monitor_count;
		GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
		GLFWmonitor* current_monitor = glfwGetPrimaryMonitor();

		int center_x = xpos + width / 2;
		int center_y = ypos + height / 2;

		for (int i = 0; i < monitor_count; i++) {
			int mx, my;
			const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
			glfwGetMonitorPos(monitors[i], &mx, &my);
			if (center_x >= mx && center_x < mx + mode->width &&
				center_y >= my && center_y < my + mode->height) {
				current_monitor = monitors[i];
				break;
			}
		}

        const GLFWvidmode* mode = glfwGetVideoMode(current_monitor);

        saved_window_x = xpos;
		saved_window_y = ypos;
        saved_window_width = width;
		saved_window_height = height;

        glfwSetWindowMonitor(window, current_monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        fullscreen_enabled = true;
    } else {
        glfwSetWindowMonitor(window, nullptr, saved_window_x, saved_window_y, saved_window_width, saved_window_height, 0);
        fullscreen_enabled = false;
    }
}

void App::take_screenshot() {
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	std::stringstream ss;
	ss << "screenshot_" << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S")
	   << (is_multisample_on ? "_aa" : "_noaa") << ".png";
	std::string filename = ss.str();

	cv::Mat img(height, width, CV_8UC3);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img.data);

	cv::flip(img, img, 0);

	if (cv::imwrite(filename, img)) {
		std::cout << "Screenshot saved to: " << filename << std::endl;
	} else {
		std::cerr << "Failed to save screenshot: " << filename << std::endl;
	}
}

void App::apply_collisions(float delta_t) {
	(void)delta_t;

	if (!collisions_enabled) {
		return;
	}

    camera.Position.x = std::clamp(camera.Position.x, MAP_MIN_X, MAP_MAX_X);
    camera.Position.z = std::clamp(camera.Position.z, MAP_MIN_Z, MAP_MAX_Z);
    camera.Position.y = std::clamp(camera.Position.y, MAP_MIN_Y, MAP_MAX_Y);

    const float CAMERA_RADIUS = 0.4f;

    for (auto& [name, obj] : scene) {
        if (!obj->collides) continue;
        resolve_camera_box_collision(obj, CAMERA_RADIUS);
    }
}

void App::resolve_camera_box_collision(const std::shared_ptr<Model>& obj, float camera_radius)
{
    const glm::vec3 half_extents = obj->scale * 0.5f + glm::vec3(camera_radius, 0.0f, camera_radius);
    const glm::vec3 delta = camera.Position - obj->pivot_position;

    if (std::abs(delta.x) > half_extents.x ||
        std::abs(delta.y) > obj->scale.y * 0.5f + 1.0f ||
        std::abs(delta.z) > half_extents.z) {
        return;
    }

    const float overlap_x = half_extents.x - std::abs(delta.x);
    const float overlap_z = half_extents.z - std::abs(delta.z);

    if (overlap_x < overlap_z) {
        camera.Position.x += delta.x < 0.0f ? -overlap_x : overlap_x;
    } else {
        camera.Position.z += delta.z < 0.0f ? -overlap_z : overlap_z;
    }

    camera.Position.x = std::clamp(camera.Position.x, MAP_MIN_X, MAP_MAX_X);
    camera.Position.z = std::clamp(camera.Position.z, MAP_MIN_Z, MAP_MAX_Z);
    camera.Position.y = MAP_MIN_Y;
}

void App::spawn_particles(const glm::vec3& position, int count) {
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> angle_dist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> speed_dist(1.5f, 4.0f);
    std::uniform_real_distribution<float> upward_dist(2.0f, 6.0f);
    std::uniform_real_distribution<float> life_dist(0.8f, 1.8f);
    std::uniform_real_distribution<float> scale_dist(0.05f, 0.2f);

    for (int i = 0; i < count; i++) {
        Particle p;
        p.position = position;
        p.age      = 0.0f;
        p.lifetime = life_dist(rng);
        p.scale    = scale_dist(rng);

        float theta = angle_dist(rng);
        float speed = speed_dist(rng);
        p.velocity  = glm::vec3(
            speed * cos(theta),
            upward_dist(rng),
            speed * sin(theta)
        );
        particles.push_back(p);
    }
}

void App::update_particles(float delta_t) {
    static const float GRAVITY    = -9.8f;
    static const float ATTENUATION = 0.95f;

    for (auto& p : particles) {
        p.age += delta_t;

        p.velocity.y += GRAVITY * delta_t;

        p.position += p.velocity * delta_t;

        if (p.position.y < 0.0f) {
            p.position.y = 0.0f;
            p.velocity *= glm::vec3(ATTENUATION, -ATTENUATION, ATTENUATION);
        }
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.age >= p.lifetime; }),
        particles.end()
    );
}

void App::draw_particles() {
    if (particles.empty() || !particle_template) return;

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    for (auto& p : particles) {
        float life_ratio = 1.0f - (p.age / p.lifetime);

        particle_template->pivot_position = p.position;
        particle_template->scale          = glm::vec3(p.scale);
        particle_template->eulerAngles.y  = p.age * 180.0f;
        particle_template->material_alpha = life_ratio * 0.85f;

        particle_template->draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
