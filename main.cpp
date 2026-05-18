/*
 * Program entry point.
 * Creates the App instance, asks Windows laptops to prefer the dedicated GPU,
 * and then starts the application lifecycle from main().
 */

#include <iostream>
#include <chrono>
#include <cstdlib>

#include "app.hpp"

#ifdef _WIN32
// Hint for hybrid-GPU laptops: prefer the dedicated NVIDIA/AMD GPU
// instead of the integrated adapter when creating the OpenGL context.
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

App app;

// Minimal entry point:
// initialize App, run the frame loop, print total runtime, and return the
// application exit code from App::run().
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

