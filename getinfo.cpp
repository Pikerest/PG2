#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept> 

void print_gl_info() {
    std::cout << "--- OpenGL Context Info ---\n";

    const char* vendor = (const char*)glGetString(GL_VENDOR);
    if (vendor == nullptr) std::cout << "Vendor: <Unknown>\n";
    else std::cout << "Vendor: " << vendor << '\n';

    const char* renderer = (const char*)glGetString(GL_RENDERER);
    if (renderer == nullptr) std::cout << "Renderer: <Unknown>\n";
    else std::cout << "Renderer: " << renderer << '\n';

    const char* version = (const char*)glGetString(GL_VERSION);
    if (version == nullptr) std::cout << "Version: <Unknown>\n";
    else std::cout << "Version: " << version << '\n';

    const char* glsl = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (glsl == nullptr) std::cout << "GLSL Version: <Unknown>\n";
    else std::cout << "GLSL Version: " << glsl << '\n';

    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    std::cout << "GL Numeric version: " << major << "." << minor << '\n';

    GLint profile;
    
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);

    if (profile & GL_CONTEXT_CORE_PROFILE_BIT) {
        std::cout << "Profile: CORE\n";
    }
    else if (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
        std::cout << "Profile: COMPATIBILITY\n";
    }
    else {
        throw std::runtime_error("What?? Unknown OpenGL profile!");
    }

    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);

    std::cout << "Context Flags: ";
    if (flags == 0) std::cout << "None";
    if (flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT) std::cout << "FORWARD_COMPATIBLE ";
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) std::cout << "DEBUG ";
    if (flags & GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT) std::cout << "ROBUST_ACCESS ";
    if (flags & GL_CONTEXT_FLAG_NO_ERROR_BIT) std::cout << "NO_ERROR ";
    std::cout << "\n---------------------------\n";
}