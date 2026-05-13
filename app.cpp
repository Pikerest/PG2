#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include <unordered_set>

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
#include "audio.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace {
constexpr float DRAW_ALPHA_EPSILON = 0.001f;
constexpr size_t MAX_PARTICLES = 128;
}

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
        audio_init();

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

void App::set_hud_message(const std::string& msg, float duration) {
    hud_message = msg;
    hud_message_time = glfwGetTime();
    hud_message_duration = duration;
}

void App::show_location_text(const std::string& msg, float duration) {
    location_message = msg;
    location_message_time = glfwGetTime();
    location_message_duration = duration;
}

void App::init_assets(void) {
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

    dir_light.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.2f));
    dir_light.ambient = glm::vec3(0.06f, 0.075f, 0.08f);
    dir_light.diffuse = glm::vec3(0.12f, 0.14f, 0.16f);
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

    spot_lights.clear();

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
    auto tex_enemy = std::make_shared<Texture>(glm::vec3(0.55f, 0.18f, 0.16f));
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
    // Right wing floors
    add_box("east_corridor_floor",  glm::vec3( 33.5f, -0.05f, -12.5f), glm::vec3(23.0f, 0.1f, 10.0f), tex_floor, true);
    add_box("warehouse_floor",      glm::vec3( 63.0f, -0.05f, -17.0f), glm::vec3(36.0f, 0.1f, 46.0f), tex_floor, true);
    add_box("reactor3_floor",       glm::vec3( 89.0f, -0.05f, -18.0f), glm::vec3(14.0f, 0.1f, 22.0f), tex_floor, true);
    add_box("escape_corridor_floor", glm::vec3(0.0f, -0.05f, -37.0f), glm::vec3(8.5f, 0.1f, 18.0f), tex_floor, true);

    add_box("spawn_room_back_wall", glm::vec3(0.0f, 2.0f, 31.2f), glm::vec3(7.8f, 4.0f, 0.7f), tex_dark, true, 2.0f);
    add_box("spawn_room_left_wall", glm::vec3(-4.0f, 2.0f, 25.0f), glm::vec3(0.8f, 4.0f, 12.0f), tex_wall, true, 1.5f);
    add_box("spawn_room_right_wall", glm::vec3(4.0f, 2.0f, 25.0f), glm::vec3(0.8f, 4.0f, 12.0f), tex_wall, true, 1.5f);
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
        auto m = std::make_shared<Model>("objects/" + path, shader_prog, tex);
        m->pivot_position = pos;
        m->eulerAngles    = euler;
        m->scale          = sc;
        scene[name] = m;
        return m;
    };

    add_obj("junc_computer",      "computer-system.obj",  glm::vec3(-56.0f, 0.0f, -22.0f), glm::vec3(0,  90, 0), glm::vec3(2.10f), tex_metal_plate_02);
    add_obj("junc_screen",        "computer-screen.obj",  glm::vec3(-56.0f, 0.0f, -19.5f), glm::vec3(0,  90, 0), glm::vec3(2.10f), tex_terminal);
    add_obj("junc_table",         "table-large.obj",      glm::vec3(-55.0f, 0.0f, -17.0f), glm::vec3(0,   0, 0), glm::vec3(1.95f), tex_metal_plate);
    add_obj("junc_chair_1",       "chair.obj",            glm::vec3(-53.0f, 0.0f, -19.5f), glm::vec3(0, 180, 0), glm::vec3(1.80f), tex_dark);
    add_obj("junc_chair_2",       "chair.obj",            glm::vec3(-53.0f, 0.0f, -22.0f), glm::vec3(0, 180, 0), glm::vec3(1.80f), tex_dark);

    // Containers NE corner — against east wall
    add_obj("junc_container_ne1", "container-tall.obj",   glm::vec3(-45.0f, 0.0f, -27.0f), glm::vec3(0,   0, 0), glm::vec3(1.95f), tex_corrugated_iron);
    add_obj("junc_container_ne2", "container.obj",        glm::vec3(-45.0f, 0.0f, -24.0f), glm::vec3(0,  90, 0), glm::vec3(1.80f), tex_metal_plate_02);
    add_obj("junc_box_ne",        "box-large.obj",        glm::vec3(-46.0f, 0.0f, -22.0f), glm::vec3(0,  45, 0), glm::vec3(1.65f), tex_box);

    // Containers SE corner — against east wall
    add_obj("junc_container_se1", "container.obj",        glm::vec3(-45.0f, 0.0f,  -6.5f), glm::vec3(0,   0, 0), glm::vec3(1.80f), tex_corrugated_iron);
    add_obj("junc_box_se1",       "box-large.obj",        glm::vec3(-45.5f, 0.0f,  -4.5f), glm::vec3(0,  20, 0), glm::vec3(1.50f), tex_box);
    add_obj("junc_box_se2",       "box-small.obj",        glm::vec3(-46.5f, 0.0f,  -3.5f), glm::vec3(0, -15, 0), glm::vec3(1.65f), tex_box);

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

    // === Reactor 1 room furniture — industrial lab with machines and server racks ===
    // (x=-67 to -87, z=-14 to +8, reactor at (-77,0.8,-3), button at (-71,0.55,-3))
    // Server racks against west wall
    add_obj("r1_server_a",   "computer-system.obj", glm::vec3(-86.5f, 0.0f, -10.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_metal_panel);
    add_obj("r1_server_b",   "computer-system.obj", glm::vec3(-86.5f, 0.0f,  -7.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_metal_panel);
    add_obj("r1_screen",     "screen-flat.obj",     glm::vec3(-86.5f, 0.0f,  -4.0f), glm::vec3(0,  90, 0), glm::vec3(2.0f), tex_terminal);
    // Work table + chair in SE corner (clear of reactor/button)
    add_obj("r1_table",      "table.obj",           glm::vec3(-73.0f, 0.0f,   5.5f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_metal_plate);
    add_obj("r1_chair_a",    "chair.obj",           glm::vec3(-70.5f, 0.0f,   5.5f), glm::vec3(0, 180, 0), glm::vec3(1.8f), tex_dark);
    // Industrial machine in NW corner
    add_obj("r1_machine",    "machine.obj",         glm::vec3(-85.5f, 0.0f, -12.0f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_corrugated_iron);
    // Containers along north wall
    add_obj("r1_cont_a",     "container.obj",       glm::vec3(-82.0f, 0.0f, -12.5f), glm::vec3(0,  90, 0), glm::vec3(1.8f), tex_corrugated_iron);
    add_obj("r1_cont_b",     "container.obj",       glm::vec3(-79.0f, 0.0f, -12.5f), glm::vec3(0,  90, 0), glm::vec3(1.8f), tex_metal_plate_02);
    // Hopper in south corner (away from button)
    add_obj("r1_hopper",     "hopper-square.obj",   glm::vec3(-85.0f, 0.0f,   6.5f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_rusty_metal);

    // === Reactor 2 room furniture — monitoring/control room style ===
    // (x=-67 to -87, z=-38 to -20, reactor at (-77,0.8,-29), button at (-71,0.55,-29))
    // Wide display wall along west wall (monitoring station)
    add_obj("r2_screen_a",   "screen-flat.obj",     glm::vec3(-86.5f, 0.0f, -32.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_terminal);
    add_obj("r2_screen_b",   "screen-flat.obj",     glm::vec3(-86.5f, 0.0f, -28.0f), glm::vec3(0,  90, 0), glm::vec3(2.2f), tex_terminal);
    add_obj("r2_computer",   "computer.obj",        glm::vec3(-86.5f, 0.0f, -25.0f), glm::vec3(0,  90, 0), glm::vec3(2.0f), tex_metal_panel);
    // Central monitoring table with two chairs
    add_obj("r2_table",      "table-large.obj",     glm::vec3(-79.0f, 0.0f, -29.0f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_sci_floor);
    add_obj("r2_chair_a",    "chair.obj",           glm::vec3(-76.0f, 0.0f, -27.5f), glm::vec3(0, 135, 0), glm::vec3(1.8f), tex_dark);
    add_obj("r2_chair_b",    "chair.obj",           glm::vec3(-76.0f, 0.0f, -30.5f), glm::vec3(0, 225, 0), glm::vec3(1.8f), tex_dark);
    // Storage boxes in NE corner (near north wall)
    add_obj("r2_box_a",      "box-large.obj",       glm::vec3(-70.0f, 0.0f, -36.5f), glm::vec3(0,  20, 0), glm::vec3(1.8f), tex_box);
    add_obj("r2_box_b",      "box-small.obj",       glm::vec3(-68.5f, 0.0f, -35.0f), glm::vec3(0, -30, 0), glm::vec3(1.6f), tex_box);
    // Industrial container against south wall
    add_obj("r2_cont",       "container-tall.obj",  glm::vec3(-84.0f, 0.0f, -21.5f), glm::vec3(0,   0, 0), glm::vec3(2.0f), tex_metal_plate_02);

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

    Enemy e1{ add_box("enemy_01", glm::vec3( -2.8f, 0.8f,  -6.2f), glm::vec3(0.8f, 1.6f, 0.8f), tex_enemy, true, 1.0f), 3, 0.0f };
    Enemy e3{ add_box("enemy_03", glm::vec3( 60.0f, 0.8f, -20.0f), glm::vec3(0.8f, 1.6f, 0.8f), tex_enemy, true, 1.0f), 3, 3.4f };
    Enemy e4{ add_box("enemy_04", glm::vec3(  0.0f, 0.8f, -28.0f), glm::vec3(0.8f, 1.6f, 0.8f), tex_enemy, true, 1.0f), 4, 5.1f };
    // Reactor enemies — orc OBJ model
    auto make_reactor_enemy = [&](const std::string& name, glm::vec3 pos, float bob_off) -> Enemy {
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
    enemies = { e1, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14 };

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
    particle_template = std::make_shared<Model>("objects/tetrahedron.obj", shader_prog, tex_particle);
    particle_template->scale = glm::vec3(0.1f);
    particle_template->is_transparent = true;
    particle_template->material_alpha = 0.8f;

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

		camera.Position = glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 25.5f);
		double last_frame_time = glfwGetTime();

		while (!glfwWindowShouldClose(window))
		{
			now = glfwGetTime();

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			if (show_imgui) {
				ImGui::SetNextWindowPos(ImVec2(10, 10));
				ImGui::SetNextWindowSize(ImVec2(360, 250));
				ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
				ImGui::Text("FPS: %.1f", FPS);
				ImGui::Text("Health: %d", player_health);
				ImGui::Text("Reactors: %d/3", reactors_active);
				ImGui::Text("Gate: %s", gate_unlocked ? "UNLOCKED" : "LOCKED");
				ImGui::Text("Collision: %s (N)", collisions_enabled ? "ON" : "NOCLIP");
				ImGui::Text("Collision debug: %s (C)", show_collision_debug ? "ON" : "OFF");
				ImGui::Text("Light debug: %s (L)", show_light_debug ? "ON" : "OFF");
				ImGui::Text("Trigger debug: %s (T)", show_trigger_debug ? "ON" : "OFF");
				ImGui::Text("V-Sync: %s (hit V to toggle)", is_vsync_on ? "ON" : "OFF");
				ImGui::Text("Multisample (AA): %s (hit M to toggle)", is_multisample_on ? "ON" : "OFF");
				ImGui::Text("LMB fire | E reactor button | P screenshot");
				ImGui::Text("Space jump | Shift sprint");
				ImGui::Text("Dev: R toggle all reactors");
				ImGui::Text("(press RMB to release mouse)");
				ImGui::Text("(hit G to show/hide info)");
				ImGui::End();
			}

			// HUD overlay — always visible, fades out after hud_message_duration seconds
			{
				const double elapsed = now - hud_message_time;
				if (elapsed < hud_message_duration && !hud_message.empty()) {
					float alpha = 1.0f;
					const double fade_start = hud_message_duration - 1.5;
					if (elapsed > fade_start)
						alpha = static_cast<float>(1.0 - (elapsed - fade_start) / 1.5);

					const ImGuiWindowFlags overlay_flags =
						ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
						ImGuiWindowFlags_NoNav       | ImGuiWindowFlags_NoMove  |
						ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
						ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground;

					ImGui::SetNextWindowPos(
						ImVec2(width * 0.5f, height * 0.83f),
						ImGuiCond_Always, ImVec2(0.5f, 0.5f));
					ImGui::SetNextWindowBgAlpha(0.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
					ImGui::Begin("##hud_overlay", nullptr, overlay_flags);
					ImGui::SetWindowFontScale(1.6f);
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.96f, 1.00f, alpha));
					ImGui::TextUnformatted(hud_message.c_str());
					ImGui::PopStyleColor();
					ImGui::End();
					ImGui::PopStyleVar(2);
				}
			}

			// Elden Ring–style cinematic location overlay (full-screen, centered, fade in/hold/fade out)
			{
				const double elapsed = now - location_message_time;
				if (elapsed < location_message_duration && !location_message.empty()) {
					// fade in 1.2s, hold, fade out last 1.5s
					float alpha;
					if (elapsed < 1.2)
						alpha = static_cast<float>(elapsed / 1.2);
					else if (elapsed < location_message_duration - 1.5)
						alpha = 1.0f;
					else
						alpha = static_cast<float>((location_message_duration - elapsed) / 1.5);
					alpha = std::clamp(alpha, 0.0f, 1.0f);

					const ImGuiWindowFlags cflags =
						ImGuiWindowFlags_NoDecoration  | ImGuiWindowFlags_NoInputs |
						ImGuiWindowFlags_NoNav          | ImGuiWindowFlags_NoMove  |
						ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_NoFocusOnAppearing |
						ImGuiWindowFlags_NoBackground;

					ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
					ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width), static_cast<float>(height)));
					ImGui::SetNextWindowBgAlpha(0.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
					ImGui::Begin("##loc_overlay", nullptr, cflags);

					const float scale = 3.2f;
					ImGui::SetWindowFontScale(scale);
					const char* txt = location_message.c_str();
					ImVec2 tsz = ImGui::CalcTextSize(txt);
					float tx = (width  - tsz.x) * 0.5f;
					float ty = (height - tsz.y) * 0.5f - tsz.y * 0.15f;

					ImDrawList* dl = ImGui::GetWindowDrawList();
					const float fs = ImGui::GetFontSize();

					// shadow
					dl->AddText(ImGui::GetFont(), fs,
						ImVec2(tx + 2.0f, ty + 2.0f),
						ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, alpha * 0.75f)),
						txt);
					// main text — cold white-blue, like the game's teal palette
					dl->AddText(ImGui::GetFont(), fs,
						ImVec2(tx, ty),
						ImGui::ColorConvertFloat4ToU32(ImVec4(0.88f, 0.97f, 1.00f, alpha)),
						txt);

					// thin horizontal line beneath the text
					const float line_y  = ty + tsz.y + fs * 0.18f;
					const float line_hw = tsz.x * 0.55f;
					const float mid_x   = width * 0.5f;
					const ImU32 line_col = ImGui::ColorConvertFloat4ToU32(
						ImVec4(0.55f, 0.85f, 0.90f, alpha * 0.55f));
					dl->AddLine(ImVec2(mid_x - line_hw, line_y),
					            ImVec2(mid_x + line_hw, line_y),
					            line_col, 1.5f);

					ImGui::End();
					ImGui::PopStyleVar(2);
				}
			}

			float delta_t = static_cast<float>(now - last_frame_time);
			last_frame_time = now;

			update_player_motion(delta_t);
			camera.ProcessInput(window, delta_t, !collisions_enabled);
			update_gameplay(delta_t, now);
			apply_collisions(delta_t);
			update_pit_state();

			if (model) {
				model->eulerAngles.y = now * 50.0f;
				model->eulerAngles.x = now * 30.0f;
			}
			if (inner_orb_model) {
				inner_orb_model->pivot_position = model ? model->pivot_position : glm::vec3(0.0f, 3.7f, -12.5f);
				inner_orb_model->eulerAngles = model ? model->eulerAngles : glm::vec3(now * 30.0f, now * 50.0f, 0.0f);
				inner_orb_model->material_alpha = 0.28f + 0.08f * std::sin(static_cast<float>(now) * 3.2f);
			}
			for (size_t i = 0; i < orb_layer_models.size(); ++i) {
				auto& layer = orb_layer_models[i];
				if (!layer) {
					continue;
				}
				layer->pivot_position = model ? model->pivot_position : glm::vec3(0.0f, 3.7f, -12.5f);
				layer->eulerAngles = model ? model->eulerAngles : glm::vec3(now * 30.0f, now * 50.0f, 0.0f);
				const float base_alpha = 0.32f - std::min(static_cast<float>(i) * 0.025f, 0.13f);
				layer->material_alpha = base_alpha + 0.045f * std::sin(static_cast<float>(now) * 3.2f + static_cast<float>(i) * 0.7f);
			}

			if (!spot_lights.empty()) {
				spot_lights[0].position = camera.Position + camera.Front * 0.18f - camera.Up * 0.12f;
				spot_lights[0].direction = glm::normalize(camera.Front);
			}

			update_particles(delta_t);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shader_prog->use();
			const glm::mat4 view = camera.GetViewMatrix();
			view_matrix = view;
			shader_prog->setUniform("uV_m", view);
			shader_prog->setUniform("uP_m", projection_matrix);

			// Frustum planes (Gribb-Hartmann) — normalized so dist = dot(n,p)+d
			{
				const glm::mat4 vp = projection_matrix * view;
				auto row4 = [&vp](int i) {
					return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
				};
				auto norm_plane = [](const glm::vec4& p) {
					return p / glm::length(glm::vec3(p));
				};
				frustum_planes[0] = norm_plane(row4(3) + row4(0)); // left
				frustum_planes[1] = norm_plane(row4(3) - row4(0)); // right
				frustum_planes[2] = norm_plane(row4(3) + row4(1)); // bottom
				frustum_planes[3] = norm_plane(row4(3) - row4(1)); // top
				frustum_planes[4] = norm_plane(row4(3) + row4(2)); // near
				frustum_planes[5] = norm_plane(row4(3) - row4(2)); // far
			}

			shader_prog->setUniform("dir_light_direction", glm::normalize(glm::mat3(view) * dir_light.direction));
			shader_prog->setUniform("dir_light_ambient", dir_light.ambient);
			shader_prog->setUniform("dir_light_diffuse", dir_light.diffuse);
			shader_prog->setUniform("dir_light_specular", dir_light.specular);

			const int uploaded_point_lights = static_cast<int>(std::min<size_t>(point_lights.size(), 24));
			shader_prog->setUniform("num_point_lights", uploaded_point_lights);
			if (uploaded_point_lights > 0) {
				static std::vector<glm::vec3> light_positions;
				static std::vector<glm::vec3> light_ambient;
				static std::vector<glm::vec3> light_diffuse;
				static std::vector<glm::vec3> light_specular;
				static std::vector<GLfloat> light_radius;
				light_positions.clear();
				light_ambient.clear();
				light_diffuse.clear();
				light_specular.clear();
				light_radius.clear();
				light_positions.reserve(uploaded_point_lights);
				light_ambient.reserve(uploaded_point_lights);
				light_diffuse.reserve(uploaded_point_lights);
				light_specular.reserve(uploaded_point_lights);
				light_radius.reserve(uploaded_point_lights);

				for (int i = 0; i < uploaded_point_lights; i++) {
					light_positions.emplace_back(glm::vec3(view * glm::vec4(point_lights[i].position, 1.0f)));
					light_ambient.emplace_back(point_lights[i].ambient);
					light_diffuse.emplace_back(point_lights[i].diffuse);
					light_specular.emplace_back(point_lights[i].specular);
					light_radius.emplace_back(point_lights[i].radius);
				}

				shader_prog->setUniform("point_light_position[0]", light_positions);
				shader_prog->setUniform("point_light_ambient[0]", light_ambient);
				shader_prog->setUniform("point_light_diffuse[0]", light_diffuse);
				shader_prog->setUniform("point_light_specular[0]", light_specular);
				shader_prog->setUniform("point_light_radius[0]", light_radius);
			}

			if (!spot_lights.empty()) {
				const auto& sl = spot_lights[0];
				shader_prog->setUniform("u_spot_active", 1);
				shader_prog->setUniform("spot_light_position", glm::vec3(view * glm::vec4(sl.position, 1.0f)));
				shader_prog->setUniform("spot_light_direction", glm::normalize(glm::mat3(view) * sl.direction));
				shader_prog->setUniform("spot_light_ambient", sl.ambient);
				shader_prog->setUniform("spot_light_diffuse", sl.diffuse);
				shader_prog->setUniform("spot_light_specular", sl.specular);
				shader_prog->setUniform("spot_light_cos_cutoff", std::cos(glm::radians(sl.cutoff)));
				shader_prog->setUniform("spot_light_cos_outer_cutoff", std::cos(glm::radians(sl.outer_cutoff)));
			}
			else {
				shader_prog->setUniform("u_spot_active", 0);
			}

			{
				auto in_frustum = [&](const glm::vec3& c, float r) {
					for (const auto& p : frustum_planes)
						if (p.x*c.x + p.y*c.y + p.z*c.z + p.w < -r) return false;
					return true;
				};

				// Reuse persistent vectors — no heap allocation per frame
				render_opaque.clear();
				render_transparent.clear();
				render_oit_orbs.clear();

				for (auto& [name, model_obj] : scene) {
					if (!model_obj || model_obj->material_alpha <= DRAW_ALPHA_EPSILON)
						continue;

					if (!model_obj->is_transparent) {
						if (in_frustum(model_obj->getPosition(), model_obj->get_cull_radius()))
							render_opaque.emplace_back(model_obj);
					}
					else if (orb_model_set.count(model_obj.get())) {
						render_oit_orbs.emplace_back(model_obj);
					}
					else {
						if (in_frustum(model_obj->getPosition(), model_obj->get_cull_radius()))
							render_transparent.emplace_back(model_obj);
					}
				}

				// Front-to-back: GPU Hi-Z discards occluded fragments before fragment shader
				std::sort(render_opaque.begin(), render_opaque.end(),
					[&](const std::shared_ptr<Model>& a, const std::shared_ptr<Model>& b) {
						const glm::vec3 da = camera.Position - a->getPosition();
						const glm::vec3 db = camera.Position - b->getPosition();
						return glm::dot(da, da) < glm::dot(db, db);
					});

				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				for (auto& m : render_opaque)
					m->draw();

				if (!render_oit_orbs.empty())
					draw_orb_oit(render_oit_orbs);

				std::sort(render_transparent.begin(), render_transparent.end(),
					[&](const std::shared_ptr<Model>& a, const std::shared_ptr<Model>& b) {
						const glm::vec3 da = camera.Position - a->getPosition();
						const glm::vec3 db = camera.Position - b->getPosition();
						const float fda = glm::dot(da, da), fdb = glm::dot(db, db);
						if (std::abs(fda - fdb) > 0.001f) return fda > fdb;
						return glm::length(a->scale) > glm::length(b->scale);
					});

				glEnable(GL_BLEND);
				glDepthMask(GL_FALSE);
				for (auto& p : render_transparent)
					p->draw();
				glDepthMask(GL_TRUE);
				glDisable(GL_BLEND);
			}

			if (show_collision_debug) {
				draw_collision_debug();
			}
			if (show_light_debug) {
				draw_light_debug();
			}
			if (show_trigger_debug) {
				draw_trigger_debug();
			}

			draw_enemy_health_bars();
			draw_particles();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
    audio_destroy();
    destroy_oit_buffers();
    if (fullscreen_vao != 0) {
        glDeleteVertexArrays(1, &fullscreen_vao);
        fullscreen_vao = 0;
    }

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

void App::destroy_oit_buffers()
{
    if (oit_depth_tex != 0) {
        glDeleteTextures(1, &oit_depth_tex);
        oit_depth_tex = 0;
    }
    if (oit_reveal_tex != 0) {
        glDeleteTextures(1, &oit_reveal_tex);
        oit_reveal_tex = 0;
    }
    if (oit_accum_tex != 0) {
        glDeleteTextures(1, &oit_accum_tex);
        oit_accum_tex = 0;
    }
    if (oit_fbo != 0) {
        glDeleteFramebuffers(1, &oit_fbo);
        oit_fbo = 0;
    }
    oit_width = 0;
    oit_height = 0;
}

void App::setup_oit_buffers(int buffer_width, int buffer_height)
{
    if (buffer_width <= 0 || buffer_height <= 0) {
        return;
    }
    if (oit_fbo != 0 && oit_width == buffer_width && oit_height == buffer_height) {
        return;
    }

    destroy_oit_buffers();
    oit_width = buffer_width;
    oit_height = buffer_height;

    glCreateFramebuffers(1, &oit_fbo);

    constexpr GLsizei oit_samples = 4;

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &oit_accum_tex);
    glTextureStorage2DMultisample(oit_accum_tex, oit_samples, GL_RGBA16F, oit_width, oit_height, GL_TRUE);
    glNamedFramebufferTexture(oit_fbo, GL_COLOR_ATTACHMENT0, oit_accum_tex, 0);

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &oit_reveal_tex);
    glTextureStorage2DMultisample(oit_reveal_tex, oit_samples, GL_R16F, oit_width, oit_height, GL_TRUE);
    glNamedFramebufferTexture(oit_fbo, GL_COLOR_ATTACHMENT1, oit_reveal_tex, 0);

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &oit_depth_tex);
    glTextureStorage2DMultisample(oit_depth_tex, oit_samples, GL_DEPTH_COMPONENT24, oit_width, oit_height, GL_TRUE);
    glNamedFramebufferTexture(oit_fbo, GL_DEPTH_ATTACHMENT, oit_depth_tex, 0);

    const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glNamedFramebufferDrawBuffers(oit_fbo, 2, draw_buffers);

    if (glCheckNamedFramebufferStatus(oit_fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Weighted OIT framebuffer is incomplete.\n";
    }
}

App::~App()
{
	destroy();

	std::cout << "Bye...\n";
}

void App::update_gameplay(float delta_t, double now)
{
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

    const bool hub_powered = reactors_active >= static_cast<int>(reactors.size());
    const float light_power = hub_powered ? 1.55f : 0.34f;
    const float top_light_power = hub_powered ? 2.05f : 0.48f;

    // Indices 0-4: affected by reactor state (dim before, bright after)
    // Index 5 (orb): always full intensity, small radius — the only constant light source
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

    // Reactor room light flicker (indices 6–17 = all R1+R2 lamps)
    if (point_lights.size() > 17) {
        const float t = static_cast<float>(now);
        // R1: deep flicker — sum can go negative → clamped to 0.05 (nearly off)
        const float r1 = std::max(0.05f, 0.50f + 0.45f * std::sin(t * 7.3f)
                                        + 0.25f * std::sin(t * 19.7f + 0.8f)
                                        + 0.10f * std::sin(t * 43.1f));
        // R2: different phase so rooms flicker independently
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
    }

    // Orb light — bright while reactors are off, fades out once hub is fully powered
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
        if (it != scene.end() && it->second) {
            it->second->emissive_color = hub_powered ? lamp_bright : lamp_dim;
        }
    }
    if (auto it = scene.find("hub_lamp_top_hex"); it != scene.end() && it->second) {
        it->second->emissive_color = hub_powered ? top_lamp_bright : top_lamp_dim;
    }

	// Sphere-AABB collision check for enemies — only walls/objects at same Y level
	auto enemy_blocked = [&](const glm::vec3& pos) -> bool {
		constexpr float ENEMY_R = 0.4f;
		for (const auto& col : scene_colliders) {
			const glm::vec3 half = col->scale * 0.5f;
			const glm::vec3 mn   = col->pivot_position - half;
			const glm::vec3 mx   = col->pivot_position + half;
			// Skip flat floors/ceilings — only block if enemy center is inside Y range
			if (pos.y < mn.y || pos.y > mx.y) continue;
			const float cx = std::clamp(pos.x, mn.x, mx.x);
			const float cz = std::clamp(pos.z, mn.z, mx.z);
			const float dx = pos.x - cx, dz = pos.z - cz;
			if (dx*dx + dz*dz < ENEMY_R * ENEMY_R) return true;
		}
		return false;
	};

	for (auto& enemy : enemies) {
		if (!enemy.alive || !enemy.model) continue;

		const float bob = std::sin(static_cast<float>(now) * 2.2f + enemy.bob_offset) * 0.15f;
		enemy.model->pivot_position.y = enemy.y_base + bob;

		const glm::vec3 to_player = camera.Position - enemy.model->pivot_position;
		const float xz_dist = glm::length(glm::vec3(to_player.x, 0.0f, to_player.z));

		// Only chase when player is within activation radius
		constexpr float CHASE_RADIUS = 18.0f;
		constexpr float STOP_DIST    = 1.4f;
		constexpr float SPEED        = 1.8f;

		if (xz_dist > STOP_DIST && xz_dist < CHASE_RADIUS) {
			const glm::vec3 dir  = glm::normalize(glm::vec3(to_player.x, 0.0f, to_player.z));
			const glm::vec3 base = enemy.model->pivot_position;

			// Slide along walls: try X then Z independently
			glm::vec3 try_x = base; try_x.x += dir.x * SPEED * delta_t;
			glm::vec3 try_z = base; try_z.z += dir.z * SPEED * delta_t;

			if (!enemy_blocked(try_x)) enemy.model->pivot_position.x = try_x.x;
			if (!enemy_blocked(try_z)) enemy.model->pivot_position.z = try_z.z;
		}

		// Always face the player
		enemy.model->eulerAngles.y = glm::degrees(std::atan2(to_player.x, to_player.z));

		// Melee attack — 10 HP per hit, 1 second cooldown
		const float full_dist = glm::distance(camera.Position, enemy.model->pivot_position);
		if (full_dist < 2.1f && now - enemy.last_attack_time > 1.0) {
			enemy.last_attack_time = now;
			player_health = std::max(0, player_health - 10);
			audio_play_hurt();
			set_hud_message("Specimen contact — taking damage!");
		}
	}

	if (now - last_fire_particle_time > 0.12) {
		for (const auto& source : fire_sources) {
			spawn_particles(source + glm::vec3(0.0f, 0.35f, 0.0f), 1);
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

    for (auto& tz : trigger_zones) {
        if (!tz.fired && glm::distance(camera.Position, tz.position) < tz.radius) {
            tz.fired = true;
            show_location_text(tz.message, tz.duration);
        }
    }
}

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

        const bool over_open_pit = is_over_hub_pit() && !is_over_hub_walkway();
        if (player_vertical_offset <= 0.0f && !over_open_pit) {
            player_vertical_offset = 0.0f;
            player_vertical_velocity = 0.0f;
            player_on_ground = true;
        }
    }

    if (moving && player_on_ground) {
        view_bob_phase += delta_t * (sprinting ? 13.5f : 9.5f);
        view_bob_offset = std::sin(view_bob_phase) * (sprinting ? 0.070f : 0.045f);
    } else {
        view_bob_offset *= std::max(0.0f, 1.0f - delta_t * 12.0f);
    }

    camera.Position.y = current_camera_eye_y();
}

float App::current_camera_eye_y() const
{
    return std::clamp(PLAYER_EYE_HEIGHT + player_vertical_offset + view_bob_offset, DEATH_PIT_Y - 4.0f, MAP_MAX_Y);
}

void App::update_pit_state()
{
    if (!collisions_enabled) {
        return;
    }

    if (player_on_ground && player_vertical_offset <= 0.02f && is_over_hub_pit() && !is_over_hub_walkway()) {
        player_on_ground = false;
        player_vertical_velocity = -0.25f;
    }

    if (camera.Position.y < DEATH_PIT_Y) {
        respawn_player("You fell into the containment pit.");
    }
}

void App::respawn_player(const std::string& message)
{
    camera.Position = glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 25.5f);
    player_vertical_offset = 0.0f;
    player_vertical_velocity = 0.0f;
    view_bob_phase = 0.0f;
    view_bob_offset = 0.0f;
    player_on_ground = true;
    player_health = 100;
    set_hud_message(message);
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
			set_hud_message("Hidden passage unlocked.");
			return;
		}
	}

	if (!nearest || best_distance > 2.4f) {
		set_hud_message("No reactor button in range.");
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
		set_hud_message("Containment gate unlocked.", 8.0f);
	} else {
		set_hud_message("Reactor online.");
	}
}

void App::toggle_all_reactors()
{
	const bool turn_on = reactors_active < static_cast<int>(reactors.size());
	reactors_active = turn_on ? static_cast<int>(reactors.size()) : 0;
	gate_unlocked = turn_on;

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

	set_hud_message(turn_on ? "Dev: all reactors online." : "Dev: all reactors offline.");
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

	audio_play_shoot();

	if (!hit_enemy) {
		set_hud_message("Shot missed.");
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
		set_hud_message("Specimen neutralized.");
	} else {
		set_hud_message("Specimen hit.");
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
			if (action == GLFW_PRESS) {
				this_inst->show_collision_debug = !this_inst->show_collision_debug;
				this_inst->set_hud_message(this_inst->show_collision_debug ? "Collision debug enabled." : "Collision debug disabled.");
			}
			break;
		case GLFW_KEY_L:
			if (action == GLFW_PRESS) {
				this_inst->show_light_debug = !this_inst->show_light_debug;
				this_inst->set_hud_message(this_inst->show_light_debug ? "Light debug enabled." : "Light debug disabled.");
			}
			break;
		case GLFW_KEY_T:
			if (action == GLFW_PRESS) {
				this_inst->show_trigger_debug = !this_inst->show_trigger_debug;
				this_inst->set_hud_message(this_inst->show_trigger_debug ? "Trigger debug enabled." : "Trigger debug disabled.");
			}
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
				this_inst->set_hud_message(this_inst->collisions_enabled ? "Collision enabled." : "Noclip enabled.");
				if (this_inst->collisions_enabled) {
					this_inst->player_vertical_offset = std::max(0.0f, this_inst->camera.Position.y - App::PLAYER_EYE_HEIGHT);
					this_inst->player_vertical_velocity = 0.0f;
					this_inst->player_on_ground = this_inst->player_vertical_offset <= 0.05f;
				}
			}
			break;
		case GLFW_KEY_E:
			this_inst->activate_nearest_reactor();
			break;
		case GLFW_KEY_R:
			if (action == GLFW_PRESS) {
				this_inst->toggle_all_reactors();
			}
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
    this_inst->setup_oit_buffers(width, height);
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
    camera.Position.y = current_camera_eye_y();

    const float CAMERA_RADIUS = 0.4f;
    bool supported = player_vertical_offset <= 0.02f && (!is_over_hub_pit() || is_over_hub_walkway());

    for (auto& obj : scene_colliders) {
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

bool App::is_over_hub_pit() const
{
    const glm::vec2 p(camera.Position.x, camera.Position.z + 12.5f);
    return std::abs(p.x) <= 24.0f && std::abs(p.y) <= 24.0f;
}

bool App::is_over_hub_walkway() const
{
    const glm::vec2 p(camera.Position.x, camera.Position.z + 12.5f);
    const bool center_platform = std::abs(p.x) <= 6.3f && std::abs(p.y) <= 6.3f;
    const bool orb_pedestal = std::abs(p.x) <= 3.15f && std::abs(p.y) <= 3.15f;
    const bool west_catwalk = p.x >= -23.7f && p.x <= -5.65f && std::abs(p.y) <= 1.55f;
    const bool east_catwalk = p.x >= 5.65f && p.x <= 23.7f && std::abs(p.y) <= 1.55f;
    const bool south_catwalk = p.y >= 5.65f && p.y <= 23.7f && std::abs(p.x) <= 1.55f;
    const bool north_catwalk = p.y >= -23.7f && p.y <= -5.65f && std::abs(p.x) <= 1.55f;
    return center_platform || orb_pedestal || west_catwalk || east_catwalk || south_catwalk || north_catwalk;
}

void App::draw_collision_debug()
{
    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 1);
    shader_prog->setUniform("u_debug_color", glm::vec4(0.05f, 0.95f, 1.0f, 0.72f));

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    for (auto& [name, obj] : scene) {
        if (!obj->collides) {
            continue;
        }
        obj->draw();
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 0);
}

void App::draw_light_debug()
{
    if (!light_debug_marker) {
        return;
    }

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

void App::draw_trigger_debug()
{
    if (!light_debug_marker) return;

    shader_prog->use();
    shader_prog->setUniform("u_debug_collision", 1);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    for (const auto& tz : trigger_zones) {
        // green = waiting, grey = already fired
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

// Draw a heart shape using ImGui DrawList primitives
static void draw_heart(ImDrawList* dl, float cx, float cy, float r, ImU32 col) {
    // Two circles for the top bumps
    dl->AddCircleFilled(ImVec2(cx - r * 0.5f, cy - r * 0.2f), r * 0.62f, col, 16);
    dl->AddCircleFilled(ImVec2(cx + r * 0.5f, cy - r * 0.2f), r * 0.62f, col, 16);
    // Triangle for the bottom point
    dl->AddTriangleFilled(
        ImVec2(cx - r, cy + r * 0.05f),
        ImVec2(cx + r, cy + r * 0.05f),
        ImVec2(cx,     cy + r), col);
}

void App::draw_enemy_health_bars()
{
    const glm::mat4 vp = projection_matrix * view_matrix;
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav        | ImGuiWindowFlags_NoMove   |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground;

    constexpr int   MAX_HP    = 3;
    constexpr float HEART_R   = 7.0f;   // heart radius in pixels
    constexpr float HEART_GAP = 18.0f;  // spacing between hearts

    for (int i = 0; i < (int)enemies.size(); ++i) {
        const auto& enemy = enemies[i];
        if (!enemy.alive || !enemy.model) continue;

        // Project 2 units above enemy pivot to screen space
        const glm::vec3 world_pos = enemy.model->pivot_position + glm::vec3(0.0f, 2.0f, 0.0f);
        const glm::vec4 clip = vp * glm::vec4(world_pos, 1.0f);
        if (clip.w <= 0.0f) continue;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f) continue;
        const float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
        const float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);

        // Alpha fades with distance
        const float dist  = glm::distance(camera.Position, enemy.model->pivot_position);
        const float alpha = std::clamp(1.0f - (dist - 5.0f) / 20.0f, 0.25f, 1.0f);
        const float total_w = MAX_HP * HEART_GAP;

        // Minimal invisible window so we get a DrawList
        ImGui::SetNextWindowPos(ImVec2(sx - total_w * 0.5f, sy - HEART_R * 2.5f),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(total_w, HEART_R * 2.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
        ImGui::Begin(("##ehp_" + std::to_string(i)).c_str(), nullptr, flags);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 wpos = ImGui::GetWindowPos();

        for (int h = 0; h < MAX_HP; ++h) {
            const float hx = wpos.x + (h + 0.5f) * HEART_GAP;
            const float hy = wpos.y + HEART_R;

            ImU32 col;
            if (h < enemy.health) {
                float r, g, b;
                if      (enemy.health == 1) { r=1.0f; g=0.15f; b=0.15f; } // red
                else if (enemy.health == 2) { r=1.0f; g=0.75f; b=0.0f;  } // orange
                else                        { r=0.9f; g=0.1f;  b=0.2f;  } // full = deep red
                col = IM_COL32(int(r*255), int(g*255), int(b*255), int(alpha*255));
            } else {
                col = IM_COL32(60, 60, 60, int(alpha * 130)); // empty = dark grey
            }
            draw_heart(dl, hx, hy, HEART_R, col);
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

void App::draw_orb_oit(const std::vector<std::shared_ptr<Model>>& oit_models)
{
    if (oit_models.empty() || oit_fbo == 0 || oit_composite_prog == nullptr) {
        return;
    }

    setup_oit_buffers(width, height);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oit_fbo);
    glBlitFramebuffer(0, 0, width, height,
                      0, 0, oit_width, oit_height,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    const GLfloat accum_clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
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
        if (orb && orb->material_alpha > DRAW_ALPHA_EPSILON) {
            orb->draw();
        }
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

void App::spawn_particles(const glm::vec3& position, int count) {
    if (particles.size() >= MAX_PARTICLES) {
        return;
    }

    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> angle_dist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> speed_dist(1.5f, 4.0f);
    std::uniform_real_distribution<float> upward_dist(2.0f, 6.0f);
    std::uniform_real_distribution<float> life_dist(0.8f, 1.8f);
    std::uniform_real_distribution<float> scale_dist(0.05f, 0.2f);

    const size_t free_slots = MAX_PARTICLES - particles.size();
    const int spawn_count = std::min<int>(count, static_cast<int>(free_slots));

    for (int i = 0; i < spawn_count; i++) {
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
