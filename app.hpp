// icp.cpp 
// author: JJ

#pragma once

#include <vector>
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "assets.hpp"

// Forward declarations for utility functions
void print_gl_info();
void test_time_measure();

class App {
public:
    App();

    GLFWwindow * window;
    bool init(void);
    int run(void);

    ~App();
private:
    void init_assets(void);
    //new GL stuff
    // GLFWwindow* window = nullptr;

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