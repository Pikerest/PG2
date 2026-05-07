// icp.cpp
// author: JJ

#include <iostream>
#include <chrono>
#include <stack>
#include <random>

// OpenCV (does not depend on GL)
#if __has_include(<opencv2/opencv.hpp>)
    #include <opencv2/opencv.hpp>
#elif __has_include(<opencv4/opencv2/opencv.hpp>)
    #include <opencv4/opencv2/opencv.hpp>
#else
    #error "OpenCV hlavičkové soubory nebyly nalezeny!"
#endif

// OpenGL Extension Wrangler: allow all multiplatform GL functions
#include <GL/glew.h>
// WGLEW = Windows GL Extension Wrangler (change for different platform)
// platform specific functions (in this case Windows)

// GLFW toolkit
// Uses GL calls to open GL context, i.e. GLEW __MUST__ be first.
#include <GLFW/glfw3.h>

// OpenGL math (and other additional GL libraries, at the end)
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "app.hpp"
// define our application
App app;

int main()
{
    using clock = std::chrono::steady_clock;
    using seconds = std::chrono::duration<double>;

    auto start_time = clock::now();

    try {
        if (app.init()) {
            int result = app.run();

            auto end_time = clock::now();
            seconds elapsed = end_time - start_time;

            std::cout << "Application runtime: "
                      << elapsed.count()
                      << " seconds" << std::endl;

            return result;
        }
    }
    catch (std::exception const& e) {
        std::cerr << "App failed : " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    auto end_time = clock::now();
    seconds elapsed = end_time - start_time;

    std::cout << "Application runtime: "
              << elapsed.count()
              << " seconds" << std::endl;

    return EXIT_SUCCESS;
}

