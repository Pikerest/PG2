#pragma once

#include <string>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "assets.hpp"

// Forward declarations for utility functions
void print_gl_info();
void test_time_measure();

class App {
public:
    App();

    bool init();
    int run();

    ~App();

private:
    void init_assets();
    // Imgui
    void init_imgui();
    void shutdown_imgui();
    void load_config(const std::string& path);
    void init_debug_output();
    // Vsync and window management 
    void apply_vsync();
    void update_window_title(double fps = -1.0);
    void cache_windowed_state();
    void toggle_fullscreen();
    // Cursor cap
    void set_cursor_captured(bool captured);
    
    GLFWmonitor* pick_best_monitor_for_window() const;

    static void glfw_error_callback(int error, const char* description);
    static void key_callback_dispatch(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void framebuffer_size_callback_dispatch(GLFWwindow* window, int width, int height);
    static void mouse_button_callback_dispatch(GLFWwindow* window, int button, int action, int mods);
    static void cursor_position_callback_dispatch(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback_dispatch(GLFWwindow* window, double xoffset, double yoffset);

    void on_key(int key, int scancode, int action, int mods);
    void on_framebuffer_size(int width, int height);
    void on_mouse_button(int button, int action, int mods);
    void on_cursor_position(double xpos, double ypos);
    void on_scroll(double xoffset, double yoffset);

    GLFWwindow* window = nullptr;

    std::string window_title = "OpenGL context";
    int window_width = 800;
    int window_height = 600;
    bool vsync_on = true;

    bool show_imgui = true;
    bool imgui_initialized = false;
    bool cursor_captured = false;
    bool is_fullscreen = false;

    int windowed_x = 100;
    int windowed_y = 100;
    int windowed_width = 800;
    int windowed_height = 600;

    float clear_r = 0.08f;
    float clear_g = 0.10f;
    float clear_b = 0.14f;
    float clear_a = 1.0f;

    float color_phase_offset = 0.0f;
    float cursor_x_norm = 0.5f;
    float cursor_y_norm = 0.5f;

    GLuint shader_prog_ID{ 0 };
    GLuint VBO_ID{ 0 };
    GLuint VAO_ID{ 0 };

    std::vector<vertex> triangle_vertices =
    {
        {{0.0f,  0.5f,  0.0f}},
        {{0.5f, -0.5f,  0.0f}},
        {{-0.5f, -0.5f,  0.0f}}
    };
};
