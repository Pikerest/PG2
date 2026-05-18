/*
 * GLFW input and window callbacks.
 * Routes keyboard/mouse actions into App state, updates projection and
 * fullscreen mode, and contains screenshot capture.
 */

#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

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

// Central keyboard callback.
// Handles menu/game-over shortcuts first, then routes gameplay/debug keys.
void App::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (this_inst->game_state == App::GameState::Menu && action == GLFW_PRESS &&
	    (key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER)) {
		this_inst->start_new_game();
		return;
	}
	if (this_inst->game_state == App::GameState::GameOver && action == GLFW_PRESS &&
	    (key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER)) {
		this_inst->start_new_game();
		return;
	}
	if (this_inst->game_state == App::GameState::Won && action == GLFW_PRESS &&
	    (key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER)) {
		this_inst->start_new_game();
		return;
	}
	if (this_inst->game_state != App::GameState::Playing) {
		if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		if (action == GLFW_PRESS && key == GLFW_KEY_G) {
			this_inst->show_imgui = !this_inst->show_imgui;
		}
		return;
	}
	if ((action == GLFW_PRESS) || (action == GLFW_REPEAT)) {
		switch (key) {
		case GLFW_KEY_ESCAPE:
			{
				// ESC first releases mouse capture; pressing it again exits.
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
					// When returning from noclip, rebuild the vertical player
					// state from the current camera height so gravity continues
					// without a sudden snap.
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

// Window resize callback.
// Keeps viewport, projection matrix, and OIT buffers in sync with framebuffer size.
void App::glfw_fbsize_callback(GLFWwindow* window, int width, int height) {
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    this_inst->width = width;
    this_inst->height = height;

    glViewport(0, 0, width, height);
    this_inst->update_projection_matrix();
    this_inst->setup_oit_buffers(width, height);
}

// Mouse button callback.
// Left click captures/fires, right click releases the cursor.
void App::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (action == GLFW_PRESS) {
		auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
		if (this_inst->game_state != App::GameState::Playing) {
			return;
		}
		switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT: {
			int mode = glfwGetInputMode(window, GLFW_CURSOR);
			if (mode == GLFW_CURSOR_NORMAL) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			} else {
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

// Active mouse-look callback used by init_glfw().
// Sends relative cursor movement into the Camera class.
void App::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
		// Reset delta tracking while cursor is free so the next capture does
		// not produce a huge camera jump.
		app->firstMouse = true;
		return;
	}

    if (app->firstMouse) {
        // First captured mouse event seeds the previous position only.
        app->cursorLastX = xpos;
        app->cursorLastY = ypos;
        app->firstMouse = false;
    }

    app->camera.ProcessMouseMovement(xpos - app->cursorLastX, (ypos - app->cursorLastY) * -1.0);
    app->cursorLastX = xpos;
    app->cursorLastY = ypos;
}

// Mouse wheel zoom.
// Adjusts field of view and immediately rebuilds the projection matrix.
void App::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	(void)xoffset;
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    this_inst->fov -= static_cast<float>(yoffset) * 3.0f;
    this_inst->fov = std::clamp(this_inst->fov, 35.0f, 90.0f);

    this_inst->update_projection_matrix();
}

// Recomputes the perspective matrix after FOV or framebuffer size changes.
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

// Toggles between windowed and fullscreen on the monitor containing the window.
// Saves windowed position/size so the user can return to the previous layout.
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

		// Pick the monitor that currently contains the window center. This
		// avoids jumping to the primary monitor in multi-monitor setups.
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

// Captures the current framebuffer and writes a timestamped PNG.
// OpenGL returns pixels upside-down, so OpenCV flips them before saving.
void App::take_screenshot() {
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	std::stringstream ss;
	ss << "screenshot_" << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S")
	   << (is_multisample_on ? "_aa" : "_noaa") << ".png";
	std::string filename = ss.str();

	cv::Mat img(height, width, CV_8UC3);

	// Read the backbuffer after the frame has been rendered. GL_BGR matches
	// OpenCV's default channel order, avoiding an extra color conversion.
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img.data);

	cv::flip(img, img, 0);

	if (cv::imwrite(filename, img)) {
		std::cout << "Screenshot saved to: " << filename << std::endl;
	} else {
		std::cerr << "Failed to save screenshot: " << filename << std::endl;
	}
}
