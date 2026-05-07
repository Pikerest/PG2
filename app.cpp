// app.cpp
// Main application implementation for the pg2 project
// author: JJ

// --- Standard library headers ------------------------------------------------
#include <iostream>   // i/o streams
#include <fstream>    // file input/output (used by JSON loader)
#include <chrono>     // timing utilities
#include <stack>      // example container (unused?)
#include <random>     // random numbers
#include <string>     // std::string used in window title
#include <sstream>
#include <iomanip>

// --- Third-party libraries ---------------------------------------------------

// OpenCV: conditionally include whichever installed path is available
#if __has_include(<opencv2/opencv.hpp>)
    #include <opencv2/opencv.hpp>
#elif __has_include(<opencv4/opencv2/opencv.hpp>)
    #include <opencv4/opencv2/opencv.hpp>
#else
    #error "OpenCV header files not found!"
#endif

// OpenGL Extension Wrangler (GLEW) - provides modern GL functions
#include <GL/glew.h>
// Note: using WGLEW on Windows; adjust for other platforms if necessary

// GLFW toolkit for window/context creation and input handling
#include <GLFW/glfw3.h>

// GLM: OpenGL mathematics library
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>  // glm::two_pi - needed for particles

// Assets and project-specific headers
#include "assets.hpp"
#include "app.hpp"
#include "Texture.hpp"

// ImGUI: immediate-mode GUI for debug interfaces
#include <imgui.h>               // core
#include <imgui_impl_glfw.h>     // GLFW binding
#include <imgui_impl_opengl3.h>  // OpenGL3 binding

// JSON parsing library
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// OpenGL debug callback (Task 1) - from lecture
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

// // GLFW error callback (Task 2)
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

App::App()
{
    // default constructor
    // nothing to do here (so far...)
    std::cout << "Constructed...\n";
}

bool App::init() {

    try {
        // -------------------------
        // GLFW INIT
        // -------------------------
        init_glfw();

        load_config("config.json");

        // -------------------------
        // GLEW INIT
        // -------------------------
        init_glew();

        // -------------------------
        // OPENGL DEBUG
        // -------------------------
        init_gl_debug();

        // -------------------------
        // VIEWPORT + BASIC STATE
        // -------------------------
        glfwGetFramebufferSize(window, &width, &height);
        update_projection_matrix();
        glViewport(0, 0, width, height);


        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE); // Ensure triangle is visible from both sides
        glEnable(GL_MULTISAMPLE); // Task 1: Enable multisampling by default

        // Task 1 (Transparency): Set up blending function and depth comparison
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LEQUAL);

        // -------------------------
        // PRINT INFO
        // -------------------------
        print_glfw_info();
        print_opencv_info();
        print_glm_info();
        print_gl_info();

        std::cout << "Initialized...\n";

		// init assets (models, sounds, textures, level map, ...)
		init_assets();

		// Initialize ImGUI
		init_imgui();

		// Initialize OpenCV (if needed)
		init_opencv();

        // Task 1.3: show window after all is loaded
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


// ---------------------------------------------------------------------------
// GUI initialization
// ---------------------------------------------------------------------------
void App::init_imgui()
{
	// see https://github.com/ocornut/imgui/wiki/Getting-Started

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
	std::cout << "ImGUI version: " << ImGui::GetVersion() << "\n";
}

// ---------------------------------------------------------------------------
// OpenCV support stub
// ---------------------------------------------------------------------------
void App::init_opencv()
{
	// Placeholder for any OpenCV-specific setup (e.g. allocate windows, set
	// parameters).  At the moment the application does not create any
	// OpenCV windows during initialization.
}

// ---------------------------------------------------------------------------
// GLFW initialization and window creation
// ---------------------------------------------------------------------------
void App::init_glfw(void)
{
	// register error callback before any GLFW calls
	glfwSetErrorCallback(glfw_error_callback);

	if (!glfwInit())
		throw std::runtime_error("GLFW initialization failed.");

	// OpenGL 4.6 core
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	glfwWindowHint(GLFW_SAMPLES, 4); // Task 1: set MSAA level to 4

	// Task 1.3: hide window during initialization
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

	// Task 1.2: initial mouse capture
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Register callbacks
	glfwSetWindowUserPointer(window, this);
	glfwSetKeyCallback(window, glfw_key_callback);
	glfwSetFramebufferSizeCallback(window, glfw_fbsize_callback);
	glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetScrollCallback(window, glfw_scroll_callback);
}

// ---------------------------------------------------------------------------
// GLEW initialization
// ---------------------------------------------------------------------------
void App::init_glew(void)
{
	// enable experimental features to get modern functionality
	glewExperimental = GL_TRUE;

	if (glewInit() != GLEW_OK)
		throw std::runtime_error("GLEW initialization failed.");

	// make sure necessary extension for Direct State Access is available
	if (!GLEW_ARB_direct_state_access)
		throw std::runtime_error("No Direct State Access support :-(");
}

// ---------------------------------------------------------------------------
// OpenGL debug output setup
// ---------------------------------------------------------------------------
void App::init_gl_debug()
{
	if (GLEW_ARB_debug_output) {
		// enable synchronous debug messages and register our callback
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(gl_debug_callback, nullptr);
	}
}

// ---------------------------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------------------------
void App::print_gl_info()
{
	std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
	std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL Version:    " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

void App::print_glfw_info(void)
{
	// display the version of GLFW used for diagnostics
	std::cout << "GLFW Version:    " << glfwGetVersionString() << std::endl;
}

void App::print_opencv_info()
{
	// report OpenCV version if OpenCV is in use
	std::cout << "OpenCV Version:  " << CV_VERSION << std::endl;
}

void App::print_glm_info()
{
	// GLM is currently not included, so version info is not available
	// Uncomment glm includes in app.cpp and the header if you need this
	std::cout << "GLM Version:     (not included)" << std::endl;
}

void App::init_assets(void) {
    shader_prog = ShaderProgram::from_files("shader.vert", "shader.frag");

    shader_prog->use();
    shader_prog->setUniform("uTexture", 0);

    // Initialize directional light (sun)
    dir_light.direction = glm::normalize(glm::vec3(1.0f, -1.0f, -1.0f));
    dir_light.ambient = glm::vec3(0.2f, 0.2f, 0.2f);
    dir_light.diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
    dir_light.specular = glm::vec3(1.0f, 1.0f, 1.0f);

    // Initialize 3 point lights at different positions
    PointLight light1;
    light1.position = glm::vec3(5.0f, 3.0f, 3.0f);
    light1.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    light1.diffuse = glm::vec3(1.0f, 0.5f, 0.5f);
    light1.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    point_lights.push_back(light1);

    PointLight light2;
    light2.position = glm::vec3(-5.0f, 3.0f, 3.0f);
    light2.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    light2.diffuse = glm::vec3(0.5f, 1.0f, 0.5f);
    light2.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    point_lights.push_back(light2);

    PointLight light3;
    light3.position = glm::vec3(0.0f, -3.0f, 3.0f);
    light3.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    light3.diffuse = glm::vec3(0.5f, 0.5f, 1.0f);
    light3.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    point_lights.push_back(light3);

    // Initialize spot light (headlight attached to camera)
    SpotLight headlight;
    headlight.position = glm::vec3(0.0f, 0.0f, 0.0f);
    headlight.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    headlight.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    headlight.diffuse = glm::vec3(1.0f, 1.0f, 0.8f);
    headlight.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    headlight.cutoff = 12.5f;
    headlight.outer_cutoff = 17.5f;
    spot_lights.push_back(headlight);

    // === SCENE SETUP ===

    // Textures
    auto tex_box = std::make_shared<Texture>("textures/box.png");
    auto tex_gray = std::make_shared<Texture>(glm::vec3(0.5f, 0.5f, 0.5f));
    auto tex_dark = std::make_shared<Texture>(glm::vec3(0.3f, 0.3f, 0.35f));
    auto tex_white = std::make_shared<Texture>(glm::vec3(0.9f, 0.9f, 0.9f));

    // --- OPAQUE OBJECTS ---

    // Ground plane
    auto ground = std::make_shared<Model>("objects/plane.obj", shader_prog, tex_gray);
    ground->pivot_position = glm::vec3(0.0f, 0.0f, 0.0f);
    ground->scale = glm::vec3(20.0f, 1.0f, 20.0f);
    ground->material_alpha = 1.0f;
    ground->is_transparent = false;
    scene["ground"] = ground;

    // Main rotating cube (the original model, now part of scene)
    model = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_box);
    model->pivot_position = glm::vec3(0.0f, 1.5f, 0.0f);
    model->scale = glm::vec3(1.0f);
    model->is_transparent = false;
    model->collides = true;
    model->bounding_radius = 0.8f;
    scene["rotating_cube"] = model;

    // Obstacle cubes (opaque, collidable)
    auto obstacle1 = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_dark);
    obstacle1->pivot_position = glm::vec3(5.0f, 1.0f, -3.0f);
    obstacle1->scale = glm::vec3(2.0f, 2.0f, 2.0f);
    obstacle1->is_transparent = false;
    obstacle1->collides = true;
    obstacle1->bounding_radius = 1.8f;
    scene["obstacle1"] = obstacle1;

    auto obstacle2 = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_dark);
    obstacle2->pivot_position = glm::vec3(-4.0f, 1.0f, 5.0f);
    obstacle2->scale = glm::vec3(1.5f, 3.0f, 1.5f);
    obstacle2->is_transparent = false;
    obstacle2->collides = true;
    obstacle2->bounding_radius = 1.5f;
    scene["obstacle2"] = obstacle2;

    auto obstacle3 = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_white);
    obstacle3->pivot_position = glm::vec3(8.0f, 1.0f, 7.0f);
    obstacle3->scale = glm::vec3(1.0f, 1.0f, 4.0f);
    obstacle3->is_transparent = false;
    obstacle3->collides = true;
    obstacle3->bounding_radius = 2.2f;
    scene["obstacle3"] = obstacle3;

    // --- TRANSPARENT OBJECTS (Task 1: at least 3 semi-transparent objects) ---
    // Using material with A < 1.0 (NOT if(alpha<0.1){discard;})

    // Transparent Red Glass Panel
    auto tex_red_glass = std::make_shared<Texture>(glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
    auto glass_red = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_red_glass);
    glass_red->pivot_position = glm::vec3(-3.0f, 2.0f, -5.0f);
    glass_red->scale = glm::vec3(3.0f, 4.0f, 0.1f);   // flat panel
    glass_red->is_transparent = true;
    glass_red->material_alpha = 0.4f;   // 40% opaque
    scene["glass_red"] = glass_red;

    // Transparent Blue Glass Panel
    auto tex_blue_glass = std::make_shared<Texture>(glm::vec4(0.2f, 0.3f, 1.0f, 1.0f));
    auto glass_blue = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_blue_glass);
    glass_blue->pivot_position = glm::vec3(3.0f, 2.0f, -2.0f);
    glass_blue->scale = glm::vec3(0.1f, 4.0f, 3.0f);  // flat panel, rotated
    glass_blue->is_transparent = true;
    glass_blue->material_alpha = 0.35f;  // 35% opaque
    scene["glass_blue"] = glass_blue;

    // Transparent Green Glass Cube
    auto tex_green_glass = std::make_shared<Texture>(glm::vec4(0.2f, 1.0f, 0.3f, 1.0f));
    auto glass_green = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_green_glass);
    glass_green->pivot_position = glm::vec3(0.0f, 3.0f, 4.0f);
    glass_green->scale = glm::vec3(2.0f, 2.0f, 2.0f);
    glass_green->is_transparent = true;
    glass_green->material_alpha = 0.3f;  // 30% opaque
    scene["glass_green"] = glass_green;

    // Transparent Yellow Glass Panel (4th transparent object for bonus)
    auto tex_yellow_glass = std::make_shared<Texture>(glm::vec4(1.0f, 0.9f, 0.1f, 1.0f));
    auto glass_yellow = std::make_shared<Model>("objects/cube_triangles.obj", shader_prog, tex_yellow_glass);
    glass_yellow->pivot_position = glm::vec3(-7.0f, 1.5f, 0.0f);
    glass_yellow->scale = glm::vec3(0.1f, 3.0f, 5.0f);
    glass_yellow->is_transparent = true;
    glass_yellow->material_alpha = 0.25f;  // 25% opaque
    scene["glass_yellow"] = glass_yellow;

    // --- PARTICLE TEMPLATE (Task 3) ---
    auto tex_particle = std::make_shared<Texture>(glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));
    particle_template = std::make_shared<Model>("objects/tetrahedron.obj", shader_prog, tex_particle);
    particle_template->scale = glm::vec3(0.1f);
    particle_template->is_transparent = true;
    particle_template->material_alpha = 0.8f;
}



int App::run(void)
{
	/*
	* Typical game loop:

			// INIT: Initial positions and state
			while (application_should_not_close)
			{
				// UPDATE: Update game state
				// RENDER: Render content
				// SWAP: Swap back/front buffer
				// VSYNC: Wait for vertical retrace (e.g. 1/60 of a second)
				// POLL: Poll events, dispatch
			}
	*/
	try {
		// Setup shader program and get uniform location
		shader_prog->use();

		double now = glfwGetTime();
		// FPS related
		double fps_last_displayed = now;
		int fps_counter_frames = 0;
		double FPS = 0.0;

		// animation related
		double frame_begin_timepoint = now;
		double previous_frame_render_time{};

		// Clear color saved to OpenGL state machine

		glClearColor(0, 0, 0, 0);

		glCullFace(GL_BACK);
		glDisable(GL_CULL_FACE); // Ujisti se, ze spatne natocene steny nam neschovaji model!

		// disable cursor, so that it can not leave window, and we can process movement
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		// get first position of mouse cursor
		glfwGetCursorPos(window, &cursorLastX, &cursorLastY);

		update_projection_matrix();
		glViewport(0, 0, width, height);

		// Kamera byla hrozně daleko (1000 jednotek), krychle je moc malá. Nastavení na 5 zajistí, že bude vidět!
		camera.Position = glm::vec3(0, 0, 5.0f);
		double last_frame_time = glfwGetTime();

		while (!glfwWindowShouldClose(window))
		{
			// ImGui prepare render (only if required)
			if (show_imgui) {
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				ImGui::SetNextWindowPos(ImVec2(10, 10));
				ImGui::SetNextWindowSize(ImVec2(300, 150));

				ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
				ImGui::Text("FPS: %.1f", FPS);
				ImGui::Text("V-Sync: %s (hit V to toggle)", is_vsync_on ? "ON" : "OFF");
				ImGui::Text("Multisample (AA): %s (hit M to toggle)", is_multisample_on ? "ON" : "OFF");
				ImGui::Text("(hit P for Screenshot)");
				ImGui::Text("(press RMB to release mouse)");
				ImGui::Text("(hit G to show/hide info)");
				ImGui::End();
			}

			//
			// UPDATE: recompute objects state, players position etc.
			//
			now = glfwGetTime();
			float delta_t = static_cast<float>(now - last_frame_time);
			last_frame_time = now;

			//########## react to user  ##########
			camera.ProcessInput(window, delta_t); // process keys etc.

			// ====== Task 2: Apply collision detection (wall sliding + object collision) ======
			apply_collisions(delta_t);

			// Animate the rotating cube
			if (model) {
				model->eulerAngles.y = now * 50.0f;
				model->eulerAngles.x = now * 30.0f;
			}

			// Animate point lights around the cube
			if (!point_lights.empty()) {
				float radius = 5.0f;
				point_lights[0].position = glm::vec3(radius * sin(now), 3.0f, radius * cos(now));
				if (point_lights.size() > 1) {
					point_lights[1].position = glm::vec3(radius * sin(now + 2.0f), 3.0f, radius * cos(now + 2.0f));
				}
				if (point_lights.size() > 2) {
					point_lights[2].position = glm::vec3(radius * sin(now + 4.0f), -3.0f, radius * cos(now + 4.0f));
				}
			}

			// Update spotlight - attach to camera (headlight)
			if (!spot_lights.empty()) {
				spot_lights[0].position = camera.Position;
				spot_lights[0].direction = camera.Front;
			}

			// ====== Task 3: Update particles ======
			update_particles(delta_t);

			//
			// RENDER: GL drawCalls
			//

			// Clear OpenGL canvas, both color buffer and Z-buffer
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//########## create and set View Matrix according to camera settings  ##########
			shader_prog->use(); // Always activate our shader before drawing, because ImGui switches to its own shader!
			shader_prog->setUniform("uV_m", camera.GetViewMatrix());
			shader_prog->setUniform("uP_m", projection_matrix);

			// Set up DIRECTIONAL LIGHT uniforms
			shader_prog->setUniform("dir_light_direction", dir_light.direction);
			shader_prog->setUniform("dir_light_ambient", dir_light.ambient);
			shader_prog->setUniform("dir_light_diffuse", dir_light.diffuse);
			shader_prog->setUniform("dir_light_specular", dir_light.specular);

			// Set up POINT LIGHTS uniforms
			shader_prog->setUniform("num_point_lights", (int)point_lights.size());
			for (size_t i = 0; i < point_lights.size() && i < 3; i++) {
				std::string idx = std::to_string(i);
				shader_prog->setUniform("point_light_position[" + idx + "]", point_lights[i].position);
				shader_prog->setUniform("point_light_ambient[" + idx + "]", point_lights[i].ambient);
				shader_prog->setUniform("point_light_diffuse[" + idx + "]", point_lights[i].diffuse);
				shader_prog->setUniform("point_light_specular[" + idx + "]", point_lights[i].specular);
			}

			// Set up SPOTLIGHT uniforms
			if (!spot_lights.empty()) {
				shader_prog->setUniform("spot_light_position", spot_lights[0].position);
				shader_prog->setUniform("spot_light_direction", spot_lights[0].direction);
				shader_prog->setUniform("spot_light_ambient", spot_lights[0].ambient);
				shader_prog->setUniform("spot_light_diffuse", spot_lights[0].diffuse);
				shader_prog->setUniform("spot_light_specular", spot_lights[0].specular);
				shader_prog->setUniform("spot_light_cutoff", spot_lights[0].cutoff);
				shader_prog->setUniform("spot_light_outer_cutoff", spot_lights[0].outer_cutoff);
			}

			// ====================================================================
			// Task 1: TRANSPARENCY - Painter's Algorithm
			// Draw scene in two passes:
			//   1) Draw all NON-transparent objects (any order, depth test ON)
			//   2) Sort transparent objects by distance from camera (far to near)
			//      and draw them with blending ON, depth write OFF
			// ====================================================================
			{
				std::vector<std::shared_ptr<Model>> transparent;
				transparent.reserve(scene.size());

				// FIRST PASS: draw all non-transparent objects + collect transparent ones
				for (auto& [name, model_obj] : scene) {
					if (!model_obj->is_transparent) {
						model_obj->draw();
					}
					else {
						transparent.emplace_back(model_obj);
					}
				}

				// SECOND PASS: sort transparent objects by distance (painter's algorithm)
				// Sort from FAR to NEAR (descending distance from camera)
				std::sort(transparent.begin(), transparent.end(),
					[&](std::shared_ptr<Model> const a, std::shared_ptr<Model> const b) {
						return glm::distance(camera.Position, a->getPosition()) > glm::distance(camera.Position, b->getPosition());
					});

				// Enable blending, disable depth writing for transparent objects
				glEnable(GL_BLEND);
				glDepthMask(GL_FALSE);  // don't write to depth buffer

				// Draw sorted transparent objects (far to near)
				for (auto& p : transparent) {
					p->draw();
				}

				// Restore GL state for non-transparent objects
				glDepthMask(GL_TRUE);   // re-enable depth writing
				glDisable(GL_BLEND);
			}

			// ====== Task 3: Draw particles (with blending) ======
			draw_particles();

			// ImGui display
			if (show_imgui) {
				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			}

			// SWAP + VSYNC
			glfwSwapBuffers(window);

			// POLL events
			glfwPollEvents();

			// Time/FPS measurement
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
	// clean up ImGUI
    if (ImGui::GetCurrentContext()) {
	    ImGui_ImplOpenGL3_Shutdown();
	    ImGui_ImplGlfw_Shutdown();
	    ImGui::DestroyContext();
    }

	// clean up OpenCV
	cv::destroyAllWindows();

	// clean-up GLFW
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

// ----------------------------------------------------------------------------
// CALLBACKS IMPLEMENTATION
// ----------------------------------------------------------------------------

void App::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
	if ((action == GLFW_PRESS) || (action == GLFW_REPEAT)) {
		switch (key) {
		case GLFW_KEY_ESCAPE:
			// Task 1.2: capture/release mouse OR exit
			{
				int mode = glfwGetInputMode(window, GLFW_CURSOR);
				if (mode == GLFW_CURSOR_DISABLED) {
					// first ESC uvolní kurzor
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				}
				else {
					// druhý ESC (nebo při uvolněném) ukončí aplikaci
					glfwSetWindowShouldClose(window, GLFW_TRUE);
				}
			}
			break;
		case GLFW_KEY_C:
			// Task 3: Background color change
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
			// Toggle ImGUI display
			this_inst->show_imgui = !this_inst->show_imgui;
			break;
		case GLFW_KEY_M:
			// Task 1: Toggle Multisampling
			this_inst->is_multisample_on = !this_inst->is_multisample_on;
			if (this_inst->is_multisample_on) glEnable(GL_MULTISAMPLE);
			else glDisable(GL_MULTISAMPLE);
			std::cout << "Multisampling: " << (this_inst->is_multisample_on ? "ON" : "OFF") << "\n";
			break;
		case GLFW_KEY_P:
			// Task 2: Take Screenshot
			this_inst->take_screenshot();
			break;

		case GLFW_KEY_TAB:
			// Task 1.2: capture/release mouse
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
    // glViewport(0, 0, width, height);
		auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    this_inst->width = width;
    this_inst->height = height;

    // set viewport
    glViewport(0, 0, width, height);
    //now your canvas has [0,0] in bottom left corner, and its size is [width x height]

    this_inst->update_projection_matrix();
}

void App::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
if (action == GLFW_PRESS) {
		switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT: {
			int mode = glfwGetInputMode(window, GLFW_CURSOR);
			if (mode == GLFW_CURSOR_NORMAL) {
				// we are outside of application, catch the cursor
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			else {
				// we are already inside our game: shoot, click, etc.
				std::cout << "Bang!\n";
			}
			break;
		}
		case GLFW_MOUSE_BUTTON_RIGHT:
            // release the cursor
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		default:
			break;
		}
	}
}

void App::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

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

	// Only move camera if cursor is disabled (Task 2, point 4)
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
	float yoffset = this_inst->lastY - ypos; // reversed since y-coordinates go from bottom to top
	this_inst->lastX = xpos;
	this_inst->lastY = ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	this_inst->yaw += xoffset;
	this_inst->pitch += yoffset;

	// make sure that when pitch is out of bounds, screen doesn't get flipped
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
    // if (yoffset > 0.0) {
    //     std::cout << "wheel up...\n";
    // } else if (yoffset < 0.0) {
    //     std::cout << "wheel down...\n";
    // }
		auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    this_inst->fov += 10*yoffset; // yoffset is mostly +1 or -1; one degree difference in fov is not visible
    this_inst->fov = std::clamp(this_inst->fov, 20.0f, 170.0f); // limit FOV to reasonable values...

    this_inst->update_projection_matrix();
}

void App::update_projection_matrix(void)
{
    if (height < 1)
        height = 1;   // avoid division by 0

    float ratio = static_cast<float>(width) / height;

    projection_matrix = glm::perspective(
        glm::radians(fov),   // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
        ratio,               // Aspect Ratio. Depends on the size of your window.
        0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
        20000.0f             // Far clipping plane. Keep as little as possible.
    );
}

void App::toggle_fullscreen() {
    if (!fullscreen_enabled) {
        // Switch to fullscreen
		int xpos, ypos, width, height;
		glfwGetWindowPos(window, &xpos, &ypos);
		glfwGetWindowSize(window, &width, &height);

		// Find current monitor (the one where the window center is)
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
        // Switch back to windowed
        glfwSetWindowMonitor(window, nullptr, saved_window_x, saved_window_y, saved_window_width, saved_window_height, 0);
        fullscreen_enabled = false;
    }
}

// ---------------------------------------------------------------------------
// Screenshot logic
// ---------------------------------------------------------------------------
void App::take_screenshot() {
	// Generate unique filename
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	std::stringstream ss;
	ss << "screenshot_" << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S")
	   << (is_multisample_on ? "_aa" : "_noaa") << ".png";
	std::string filename = ss.str();

	// Allocate OpenCV matrix
	cv::Mat img(height, width, CV_8UC3);

	// Read pixels from GL FRONT buffer or back buffer.
	// Make sure we are aligned to 1 byte
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img.data);

	// OpenGL origin is bottom-left, OpenCV is top-left
	cv::flip(img, img, 0);

	// Write to file
	if (cv::imwrite(filename, img)) {
		std::cout << "Screenshot saved to: " << filename << std::endl;
	} else {
		std::cerr << "Failed to save screenshot: " << filename << std::endl;
	}
}

// ---------------------------------------------------------------------------
// Task 2: Collision Detection
// ---------------------------------------------------------------------------
void App::apply_collisions(float delta_t) {
    // --- Collision Type 1: Wall boundary (map limits) with sliding ---
    // Camera cannot leave the defined map boundaries.
    // We clamp each axis independently = sliding along walls.
    camera.Position.x = std::clamp(camera.Position.x, MAP_MIN_X, MAP_MAX_X);
    camera.Position.z = std::clamp(camera.Position.z, MAP_MIN_Z, MAP_MAX_Z);
    camera.Position.y = std::clamp(camera.Position.y, MAP_MIN_Y, MAP_MAX_Y);

    // --- Collision Type 2: Sphere-sphere detection with scene objects ---
    // Camera bounding radius treated as 0.4 units (roughly head + body)
    const float CAMERA_RADIUS = 0.4f;

    for (auto& [name, obj] : scene) {
        if (!obj->collides) continue;

        glm::vec3 obj_pos = obj->getPosition();
        float dist = glm::distance(camera.Position, obj_pos);
        float min_dist = CAMERA_RADIUS + obj->bounding_radius;

        if (dist < min_dist && dist > 0.0001f) {
            // Push camera away from the object (sliding: only move along the
            // direction of collision, preserving tangential movement)
            glm::vec3 push_dir = glm::normalize(camera.Position - obj_pos);
            float overlap = min_dist - dist;
            camera.Position += push_dir * overlap;

            // Spawn particle effect at collision point (Task 3)
            glm::vec3 contact_point = obj_pos + push_dir * obj->bounding_radius;
            spawn_particles(contact_point, 8);
        }
    }
}

// ---------------------------------------------------------------------------
// Task 3: Particle System
// ---------------------------------------------------------------------------
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

        // Random velocity on hemisphere (upward bias)
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

        // Apply gravity
        p.velocity.y += GRAVITY * delta_t;

        // Move particle
        p.position += p.velocity * delta_t;

        // Simple floor bounce
        if (p.position.y < 0.0f) {
            p.position.y = 0.0f;
            p.velocity *= glm::vec3(ATTENUATION, -ATTENUATION, ATTENUATION);
        }
    }

    // Remove dead particles (lifetime exceeded)
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.age >= p.lifetime; }),
        particles.end()
    );
}

void App::draw_particles() {
    if (particles.empty() || !particle_template) return;

    // Enable blending for transparent particles
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    for (auto& p : particles) {
        // Fade out as particle ages (alpha decreases toward end of life)
        float life_ratio = 1.0f - (p.age / p.lifetime);

        particle_template->pivot_position = p.position;
        particle_template->scale          = glm::vec3(p.scale);
        particle_template->eulerAngles.y  = p.age * 180.0f; // spin
        particle_template->material_alpha = life_ratio * 0.85f;

        particle_template->draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
