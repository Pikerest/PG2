#pragma once

/*
 * Shared geometry types.
 * Vertex is the common CPU-side layout uploaded into Mesh VBOs and matched
 * by the shader attribute locations in Mesh/ShaderProgram.
 */

#include <GL/glew.h> 
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// One render vertex: position, normal for lighting, and texture coordinates.
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;

    // OBJ loading uses this to collapse duplicated face vertices into an
    // indexed mesh. Exact float comparison is acceptable because values come
    // directly from parsed file tokens, not from accumulated calculations.
    bool operator==(const Vertex& other) const {
        return Position == other.Position && Normal == other.Normal && TexCoords == other.TexCoords;
    }
};

