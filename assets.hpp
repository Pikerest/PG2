#pragma once

#include <GL/glew.h> 
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

//vertex description
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;

    bool operator==(const Vertex& other) const {
        return Position == other.Position && Normal == other.Normal && TexCoords == other.TexCoords;
    }
};

