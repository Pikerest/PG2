/*
 * App lifecycle and main frame loop.
 * This file keeps startup/configuration, the per-frame orchestration,
 * shutdown, and OIT framebuffer lifetime. Feature-specific App methods
 * live in the app_*.cpp files next to this one.
 */

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <vector>

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

#include "app.hpp"
#include "audio.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace {
constexpr float DRAW_ALPHA_EPSILON = 0.001f;

// A directory is considered the asset root when it contains the files/folders
// used by the renderer. This avoids hard-coding one launch directory.
bool is_asset_root(const std::filesystem::path& dir) {
    return std::filesystem::exists(dir / "shader.vert") &&
           std::filesystem::exists(dir / "shader.frag") &&
           std::filesystem::is_directory(dir / "objects") &&
           std::filesystem::is_directory(dir / "textures");
}

std::filesystem::path find_asset_root() {
    auto dir = std::filesystem::current_path();

    // First walk upward from the current process directory. This covers normal
    // launches from build/, build/Debug/, or the repository root.
    for (auto candidate = dir; !candidate.empty(); candidate = candidate.parent_path()) {
        if (is_asset_root(candidate)) {
            return candidate;
        }
        if (candidate == candidate.root_path()) {
            break;
        }
    }

#ifdef PG2_SOURCE_DIR
    // CMake provides this path as a final reliable fallback for IDE runs.
    const auto source_dir = std::filesystem::path(PG2_SOURCE_DIR);
    if (!source_dir.empty() && is_asset_root(source_dir)) {
        return source_dir;
    }
#endif

    return dir;
}

// Finds the project asset directory and makes it the working directory.
// This lets relative paths like "objects/..." and "textures/..." work
// both from the IDE build folder and from the project root.
void set_asset_working_directory() {
    const auto asset_root = find_asset_root();
    const auto current = std::filesystem::current_path();
    if (asset_root != current) {
        std::filesystem::current_path(asset_root);
        std::cout << "Working directory: " << asset_root.string() << '\n';
    }
}
}

// Prints detailed OpenGL debug messages when the driver reports API errors,
// undefined behavior, deprecated calls, or performance warnings.
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

// GLFW calls this when window/context creation or platform integration fails.
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// App owns all runtime state. Heavy initialization is delayed to init(),
// because it can fail and needs a ready OpenGL/GLFW environment.
App::App()
{
    std::cout << "Constructed...\n";
}

// Initializes the application in dependency order:
// config -> window/context -> GL loader/debug -> audio -> assets/UI.
bool App::init() {

    try {
        set_asset_working_directory();
        load_config("config.json");

        init_glfw();
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
// Reads window and VSync settings from config.json.
// Missing config is non-fatal, so development builds can run with defaults.
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


// Creates the ImGui context and binds it to the current GLFW/OpenGL backend.
void App::init_imgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
	std::cout << "ImGUI version: " << ImGui::GetVersion() << "\n";
}

// Placeholder for OpenCV setup. The project currently uses OpenCV mostly
// for screenshots/info, so there is no persistent OpenCV state to initialize.
void App::init_opencv()
{
}

// Creates the GLFW window and wires static GLFW callbacks back to this App.
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

// Initializes GLEW after the OpenGL context is current.
// Direct State Access is required by framebuffer/texture setup code.
void App::init_glew(void)
{
	glewExperimental = GL_TRUE;

	if (glewInit() != GLEW_OK)
		throw std::runtime_error("GLEW initialization failed.");

	if (!GLEW_ARB_direct_state_access)
		throw std::runtime_error("No Direct State Access support :-(");
}

// Enables OpenGL driver debug callbacks in debug-capable contexts.
void App::init_gl_debug()
{
	if (GLEW_ARB_debug_output) {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(gl_debug_callback, nullptr);
	}
}

// Prints OpenGL driver, renderer, API, and GLSL versions.
void App::print_gl_info()
{
	std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
	std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL Version:    " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

// Prints linked GLFW version string.
void App::print_glfw_info(void)
{
	std::cout << "GLFW Version:    " << glfwGetVersionString() << std::endl;
}

// Prints OpenCV version compiled into the application.
void App::print_opencv_info()
{
	std::cout << "OpenCV Version:  " << CV_VERSION << std::endl;
}

// Keeps GLM diagnostics grouped with the other startup version messages.
void App::print_glm_info()
{
	std::cout << "GLM Version:     (not included)" << std::endl;
}

// Stores short HUD text plus timing information used by the ImGui overlay.
void App::set_hud_message(const std::string& msg, float duration) {
    hud_message = msg;
    hud_message_time = glfwGetTime();
    hud_message_duration = duration;
}

// Stores large cinematic location text shown when trigger zones fire.
void App::show_location_text(const std::string& msg, float duration) {
    location_message = msg;
    location_message_time = glfwGetTime();
    location_message_duration = duration;
}

// Asset and world construction - see app_assets.cpp

// Main application loop.
// Orchestrates ImGui, gameplay updates, camera/light uniforms, 3D rendering,
// UI rendering, buffer presentation, and FPS tracking.
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

		// Start in menu — show cursor
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwGetCursorPos(window, &cursorLastX, &cursorLastY);

		update_projection_matrix();
		glViewport(0, 0, width, height);

		camera.Position = glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 25.5f);
		game_state_enter_time = glfwGetTime();
		double last_frame_time = glfwGetTime();

		while (!glfwWindowShouldClose(window))
		{
			now = glfwGetTime();

			// Begin a new UI frame. ImGui draw commands are collected during
			// the frame and submitted after the 3D scene has been rendered.
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// Developer overlay with live state and debug toggles.
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

			// HUD overlay — visible during gameplay, fades out after hud_message_duration seconds
			if (game_state == GameState::Playing) {
				const double elapsed = now - hud_message_time;
				if (elapsed < hud_message_duration && !hud_message.empty()) {
					// HUD messages stay fully visible, then fade during the
					// last 1.5 seconds of their configured lifetime.
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

			// Cinematic location overlay (full-screen, centered, fade in/hold/fade out)
			if (game_state == GameState::Playing) {
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

					// Manual draw-list text gives precise shadow/line styling
					// without creating extra ImGui widgets.
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

			// Frame delta drives all time-based movement and animation.
			float delta_t = static_cast<float>(now - last_frame_time);
			last_frame_time = now;

			// Gameplay update order:
			// player motion first, then world logic, then collision/pit checks.
			if (game_state == GameState::Playing) {
				if (!game_over) {
					update_player_motion(delta_t);
					camera.ProcessInput(window, delta_t, !collisions_enabled);
				}
				update_gameplay(delta_t, now);
				apply_collisions(delta_t);
				update_pit_state();
			}

			// Animate central orb layers independently of gameplay state.
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
				// Flashlight follows the camera with a slight offset so it
				// appears to come from the player's hands/suit.
				spot_lights[0].position = camera.Position + camera.Front * 0.18f - camera.Up * 0.12f;
				spot_lights[0].direction = glm::normalize(camera.Front);
			}

			// Particle simulation runs after gameplay can spawn new particles.
			update_particles(delta_t);

			// Start the 3D pass and upload camera matrices.
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

			// Upload lights in view space because the shader does lighting there.
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
				// Build per-frame render lists:
				// opaque front-to-back, transparent back-to-front, orb layers via OIT.
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

					// Orb layers are routed to OIT because ordinary back-to-front
					// sorting breaks down for nested transparent shells.
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

						// Back-to-front sorting is still used for ordinary glass
						// and UI-like transparent props.
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

			// Optional world-space debug visualization.
			if (show_collision_debug) {
				draw_collision_debug();
			}
			if (show_light_debug) {
				draw_light_debug();
			}
			if (show_trigger_debug) {
				draw_trigger_debug();
			}

			// Gameplay HUD rendered after 3D so it stays readable.
			if (game_state == GameState::Playing) {
				draw_player_hud();
				draw_enemy_health_bars();
			}
			if (game_state == GameState::Playing) {
				// Floating hints above reactor buttons
				for (int ri = 0; ri < (int)reactors.size(); ++ri) {
					const auto& reactor = reactors[ri];
					if (reactor.active || !reactor.button) continue;
					const float rd = glm::distance(camera.Position, reactor.button->pivot_position);
					if (rd > 6.0f) continue;
					const glm::mat4 vpr = projection_matrix * view_matrix;
					const glm::vec3 rwp = reactor.button->pivot_position + glm::vec3(0.0f, 1.2f, 0.0f);
					const glm::vec4 rcl = vpr * glm::vec4(rwp, 1.0f);
					if (rcl.w <= 0.0f) continue;
					const glm::vec3 rnd = glm::vec3(rcl) / rcl.w;
					if (std::abs(rnd.x) < 1.0f && std::abs(rnd.y) < 1.0f) {
						// Project the 3D button position into ImGui pixel space
						// for a floating interaction prompt.
						const float rsx = (rnd.x * 0.5f + 0.5f) * width;
						const float rsy = (1.0f - (rnd.y * 0.5f + 0.5f)) * height;
						const float rfa = std::clamp(1.0f - (rd - 3.0f) / 3.0f, 0.0f, 1.0f);
						constexpr ImGuiWindowFlags rf =
							ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
							ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
							ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
							ImGuiWindowFlags_NoFocusOnAppearing;
						ImGui::SetNextWindowPos(ImVec2(rsx, rsy), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
						ImGui::SetNextWindowBgAlpha(0.55f * rfa);
						ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
						ImGui::Begin(("##rbtn_" + std::to_string(ri)).c_str(), nullptr, rf);
						ImGui::SetWindowFontScale(1.15f);
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 1.0f, rfa));
						ImGui::TextUnformatted("[ E ]  Activate reactor");
						ImGui::PopStyleColor();
						ImGui::End();
						ImGui::PopStyleVar(2);
					}
				}
				// Floating world-space hint above interactive button
				if (!hidden_door_open && hidden_door_btn) {
					const float d = glm::distance(camera.Position, hidden_door_btn->pivot_position);
					if (d < 8.0f) {
						const glm::mat4 vp2 = projection_matrix * view_matrix;
						const glm::vec3 wp = hidden_door_btn->pivot_position + glm::vec3(0.0f, 1.4f, 0.0f);
						const glm::vec4 cl = vp2 * glm::vec4(wp, 1.0f);
						if (cl.w > 0.0f) {
							const glm::vec3 nd = glm::vec3(cl) / cl.w;
							if (std::abs(nd.x) < 1.0f && std::abs(nd.y) < 1.0f) {
								const float sx2 = (nd.x * 0.5f + 0.5f) * width;
								const float sy2 = (1.0f - (nd.y * 0.5f + 0.5f)) * height;
								const float fa  = std::clamp(1.0f - (d - 4.0f) / 4.0f, 0.0f, 1.0f);
								constexpr ImGuiWindowFlags hf =
									ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
									ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
									ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
									ImGuiWindowFlags_NoFocusOnAppearing;
								ImGui::SetNextWindowPos(ImVec2(sx2, sy2), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
								ImGui::SetNextWindowBgAlpha(0.55f * fa);
								ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
								ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
								ImGui::Begin("##btn_hint", nullptr, hf);
								ImGui::SetWindowFontScale(1.15f);
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.1f, fa));
								ImGui::TextUnformatted("[ E ]  Open hidden passage");
								ImGui::PopStyleColor();
								ImGui::End();
								ImGui::PopStyleVar(2);
							}
						}
					}
				}
			}
			if (game_state != GameState::Playing) {
				draw_menu();
			}
			draw_particles();

			// Submit ImGui draw lists, present the backbuffer, then poll input.
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glfwSwapBuffers(window);

			glfwPollEvents();

			// Console FPS counter, updated once per second to avoid spam.
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

// Releases subsystems in reverse-ish creation order.
// Safe to call even during partial initialization failure.
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

// Deletes all textures/framebuffers used by weighted blended OIT.
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

// Creates or recreates the multisampled OIT framebuffer used for transparent
// orb layers. Called at startup and whenever the framebuffer size changes.
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

    // Weighted blended OIT needs two color targets: accumulated color and
    // revealage. A depth texture is attached so the pass can depth-test.
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

// Destructor delegates cleanup to destroy(); destroy() guards all handles so
// repeated cleanup during shutdown is harmless.
App::~App()
{
	destroy();

	std::cout << "Bye...\n";
}

// Gameplay logic (enemies, reactors, player motion, weapon) — see app_gameplay.cpp

// Input handling — see app_input.cpp

// Collision detection and resolution — see app_collision.cpp

// Rendering (HUD, menu, particles, debug, OIT) — see app_render.cpp
