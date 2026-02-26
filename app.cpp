// author: JJ

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifdef __has_include
    #if __has_include(<nlohmann/json.hpp>)
        #include <nlohmann/json.hpp>
        #define APP_HAS_JSON 1
    #else
        #define APP_HAS_JSON 0
    #endif
#else
    #define APP_HAS_JSON 0
#endif

#include "app.hpp"
#include "gl_err_callback.h"

#if APP_HAS_JSON
using json = nlohmann::json;
#endif

namespace {
constexpr float PI_F = 3.14159265359f;

float clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}
}

App::App()
{
    std::cout << "Constructed...\n";
}

void App::load_config(const std::string& path)
{
#if APP_HAS_JSON
    std::ifstream input(path);
    if (!input.is_open()) {
        input.open("../" + path);
    }

    if (!input.is_open()) {
        std::cout << "Config file '" << path << "' not found. Using defaults (800x600, vsync on).\n";
        return;
    }

    json cfg;
    input >> cfg;

    window_width = std::max(1, cfg.value("width", window_width));
    window_height = std::max(1, cfg.value("height", window_height));
    vsync_on = cfg.value("vsync", vsync_on);

    if (cfg.contains("title") && cfg["title"].is_string()) {
        window_title = cfg["title"].get<std::string>();
    }
#else
    (void)path;
    std::cout << "nlohmann/json.hpp not found. JSON config loading skipped, defaults are used.\n";
#endif
}

void App::glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW error [" << error << "]: " << (description ? description : "<no description>") << std::endl;
}

void App::apply_vsync()
{
    glfwSwapInterval(vsync_on ? 1 : 0);
}

void App::update_window_title(double fps)
{
    if (!window) {
        return;
    }

    std::ostringstream title;
    title << window_title
          << " | " << window_width << "x" << window_height
          << " | VSync " << (vsync_on ? "ON" : "OFF");

    if (fps >= 0.0) {
        title << " | FPS: " << std::fixed << std::setprecision(1) << fps;
    }

    glfwSetWindowTitle(window, title.str().c_str());
}

void App::init_debug_output()
{
    const bool has_debug_output = (GLEW_KHR_debug == GL_TRUE) || (GLEW_ARB_debug_output == GL_TRUE);
    if (!has_debug_output) {
        std::cout << "GL debug output not supported by this context.\n";
        return;
    }

    GLint flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) == 0) {
        std::cout << "Debug extension exists, but the current context is not a debug context.\n";
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);

    // suppress notification spam, keep useful warnings/errors
    glDebugMessageControl(GL_DONT_CARE,
                          GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION,
                          0,
                          nullptr,
                          GL_FALSE);

    std::cout << "GL debug callback enabled.\n";
}

bool App::init()
{
    try {
        glfwSetErrorCallback(&App::glfw_error_callback);
        if (!glfwInit()) {
            throw std::runtime_error("glfwInit() failed.");
        }

        load_config("config.json");

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

        window = glfwCreateWindow(window_width, window_height, window_title.c_str(), nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("glfwCreateWindow() failed.");
        }

        glfwMakeContextCurrent(window);
        glfwSetWindowUserPointer(window, this);

        glewExperimental = GL_TRUE;
        const GLenum glew_error = glewInit();
        if (glew_error != GLEW_OK) {
            throw std::runtime_error(
                std::string("glewInit() failed: ") + reinterpret_cast<const char*>(glewGetErrorString(glew_error))
            );
        }

        // Some drivers produce a harmless error during GLEW init on core profile.
        glGetError();

        init_debug_output();

        if (!GLEW_ARB_direct_state_access) {
            throw std::runtime_error("Required extension missing: GL_ARB_direct_state_access");
        }

        glfwSetKeyCallback(window, &App::key_callback_dispatch);
        glfwSetFramebufferSizeCallback(window, &App::framebuffer_size_callback_dispatch);
        glfwSetMouseButtonCallback(window, &App::mouse_button_callback_dispatch);
        glfwSetCursorPosCallback(window, &App::cursor_position_callback_dispatch);
        glfwSetScrollCallback(window, &App::scroll_callback_dispatch);

        apply_vsync();
        glViewport(0, 0, window_width, window_height);
        update_window_title();

        print_gl_info();
        test_time_measure();
        init_assets();
    }
    catch (const std::exception& e) {
        std::cerr << "Init failed: " << e.what() << std::endl;
        throw;
    }

    std::cout << "Initialized...\n";
    return true;
}

void App::init_assets()
{
    const char* vertex_shader =
        "#version 460 core\n"
        "in vec3 attribute_Position;\n"
        "void main() {\n"
        "  gl_Position = vec4(attribute_Position, 1.0);\n"
        "}\n";

    const char* fragment_shader =
        "#version 460 core\n"
        "uniform vec4 uniform_Color;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "  FragColor = uniform_Color;\n"
        "}\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, nullptr);
    glCompileShader(fs);

    shader_prog_ID = glCreateProgram();
    glAttachShader(shader_prog_ID, fs);
    glAttachShader(shader_prog_ID, vs);
    glLinkProgram(shader_prog_ID);

    glDetachShader(shader_prog_ID, fs);
    glDetachShader(shader_prog_ID, vs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glCreateVertexArrays(1, &VAO_ID);

    const GLint position_attrib_location = glGetAttribLocation(shader_prog_ID, "attribute_Position");
    if (position_attrib_location < 0) {
        throw std::runtime_error("attribute_Position not found in shader program.");
    }

    vertex v;

    glEnableVertexArrayAttrib(VAO_ID, static_cast<GLuint>(position_attrib_location));
    glVertexArrayAttribFormat(
        VAO_ID,
        static_cast<GLuint>(position_attrib_location),
        v.position.length(),
        GL_FLOAT,
        GL_FALSE,
        offsetof(vertex, position)
    );
    glVertexArrayAttribBinding(VAO_ID, static_cast<GLuint>(position_attrib_location), 0);

    glCreateBuffers(1, &VBO_ID);
    glNamedBufferData(VBO_ID,
                      static_cast<GLsizeiptr>(triangle_vertices.size() * sizeof(vertex)),
                      triangle_vertices.data(),
                      GL_STATIC_DRAW);

    glVertexArrayVertexBuffer(VAO_ID, 0, VBO_ID, 0, sizeof(vertex));
}

void App::key_callback_dispatch(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->on_key(key, scancode, action, mods);
    }
}

void App::framebuffer_size_callback_dispatch(GLFWwindow* window, int width, int height)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->on_framebuffer_size(width, height);
    }
}

void App::mouse_button_callback_dispatch(GLFWwindow* window, int button, int action, int mods)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->on_mouse_button(button, action, mods);
    }
}

void App::cursor_position_callback_dispatch(GLFWwindow* window, double xpos, double ypos)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->on_cursor_position(xpos, ypos);
    }
}

void App::scroll_callback_dispatch(GLFWwindow* window, double xoffset, double yoffset)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->on_scroll(xoffset, yoffset);
    }
}

void App::on_key(int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }

    switch (key) {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;

    case GLFW_KEY_V:
        vsync_on = !vsync_on;
        apply_vsync();
        update_window_title();
        std::cout << "VSync " << (vsync_on ? "ON" : "OFF") << std::endl;
        break;

    case GLFW_KEY_C:
        clear_r = 0.08f;
        clear_g = 0.10f;
        clear_b = 0.14f;
        break;

    default:
        break;
    }
}

void App::on_framebuffer_size(int width, int height)
{
    window_width = std::max(width, 1);
    window_height = std::max(height, 1);
    glViewport(0, 0, window_width, window_height);
    update_window_title();
}

void App::on_mouse_button(int button, int action, int mods)
{
    (void)mods;

    if (action != GLFW_PRESS) {
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        clear_b = clamp01(clear_b + 0.10f);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        clear_r = 0.0f;
        clear_g = 0.0f;
        clear_b = 0.0f;
    }
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        clear_r = 0.08f;
        clear_g = 0.10f;
        clear_b = 0.14f;
    }
}

void App::on_cursor_position(double xpos, double ypos)
{
    if (window_width <= 0 || window_height <= 0) {
        return;
    }

    cursor_x_norm = clamp01(static_cast<float>(xpos / static_cast<double>(window_width)));
    cursor_y_norm = clamp01(static_cast<float>(ypos / static_cast<double>(window_height)));

    clear_r = cursor_x_norm;
    clear_g = 1.0f - cursor_y_norm;
}

void App::on_scroll(double xoffset, double yoffset)
{
    color_phase_offset += static_cast<float>(0.25 * yoffset);
    clear_b = clamp01(clear_b + static_cast<float>(0.05 * (xoffset + yoffset)));
}

int App::run()
{
    try {
        glUseProgram(shader_prog_ID);

        const GLint uniform_color_location = glGetUniformLocation(shader_prog_ID, "uniform_Color");
        if (uniform_color_location == -1) {
            throw std::runtime_error("uniform_Color not found in active shader program.");
        }

        double fps_timer_start = glfwGetTime();
        int frame_counter = 0;

        while (!glfwWindowShouldClose(window)) {
            const double now = glfwGetTime();
            const float t = static_cast<float>(now) + color_phase_offset;

            const GLfloat r = 0.5f + 0.5f * std::sin(t);
            const GLfloat g = 0.5f + 0.5f * std::sin(t + (2.0f * PI_F / 3.0f));
            const GLfloat b = 0.5f + 0.5f * std::sin(t + (4.0f * PI_F / 3.0f));
            const GLfloat a = 1.0f;

            glClearColor(clear_r, clear_g, clear_b, clear_a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUniform4f(uniform_color_location, r, g, b, a);
            glBindVertexArray(VAO_ID);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangle_vertices.size()));

            glfwSwapBuffers(window);
            glfwPollEvents();

            ++frame_counter;
            const double elapsed = now - fps_timer_start;
            if (elapsed >= 0.25) {
                const double fps = static_cast<double>(frame_counter) / elapsed;
                update_window_title(fps);
                fps_timer_start = now;
                frame_counter = 0;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "App failed: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Finished OK...\n";
    return EXIT_SUCCESS;
}

App::~App()
{
    if (shader_prog_ID != 0) {
        glDeleteProgram(shader_prog_ID);
    }
    if (VBO_ID != 0) {
        glDeleteBuffers(1, &VBO_ID);
    }
    if (VAO_ID != 0) {
        glDeleteVertexArrays(1, &VAO_ID);
    }

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}
