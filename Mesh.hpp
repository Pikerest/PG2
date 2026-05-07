#pragma once

#include <string>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp> 
#include <glm/ext.hpp>

#include "assets.hpp"
#include "non_copyable.hpp"
#include "Texture.hpp"
#include <memory>

class Mesh: private NonCopyable
{
public:
    // force attribute slots in shaders for all meshes, shaders etc.
    static constexpr GLuint attribute_location_position{0};
    static constexpr GLuint attribute_location_normal{1};
    static constexpr GLuint attribute_location_texture_coords{2};

    // No default constructor. RAII - if constructed, it will be correctly initialized
    // and can be rendered. OpenGL resources are guaranteed to be deallocated using destructor. 
    // Double-free errors are prevented by making class non-copyable (therefore 
    // double destruction of the same OpenGL buffer is prevented). 
    Mesh() = delete; 
    
    // Simple mesh from vertices
    Mesh(std::vector<Vertex> const &vertices, GLenum primitive_type) : primitive_type_{primitive_type}, vertices_{vertices}
    {
        glCreateVertexArrays(1, &vao_);
        glCreateBuffers(1, &vbo_);

        glNamedBufferStorage(vbo_, sizeof(Vertex) * vertices_.size(), vertices_.data(), 0);

        glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(Vertex));

        glEnableVertexArrayAttrib(vao_, attribute_location_position);
        glEnableVertexArrayAttrib(vao_, attribute_location_normal);
        glEnableVertexArrayAttrib(vao_, attribute_location_texture_coords);

        glVertexArrayAttribFormat(vao_, attribute_location_position, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Position));
        glVertexArrayAttribFormat(vao_, attribute_location_normal, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Normal));
        glVertexArrayAttribFormat(vao_, attribute_location_texture_coords, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, TexCoords));

        glVertexArrayAttribBinding(vao_, attribute_location_position, 0);
        glVertexArrayAttribBinding(vao_, attribute_location_normal, 0);
        glVertexArrayAttribBinding(vao_, attribute_location_texture_coords, 0);
    }
         
    // Mesh with indirect vertex addressing. Needs compiled shader for attributes setup. 
    Mesh(std::vector<Vertex> const &vertices, std::vector<GLuint> const &indices, GLenum primitive_type) :
        Mesh{vertices, primitive_type}
    {
        indices_ = indices;
        glCreateBuffers(1, &ebo_);
        glNamedBufferStorage(ebo_, sizeof(GLuint) * indices_.size(), indices_.data(), 0);
        glVertexArrayElementBuffer(vao_, ebo_);
    }    

    void draw() {
        if (texture_) {
            texture_->bind(0);
        }
        glBindVertexArray(vao_);
        
    	if (ebo_ == 0) {
    		glDrawArrays(primitive_type_, 0, vertices_.size());
    	} else {
    		glDrawElements(primitive_type_, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
    	}
    }

    void setTexture(std::shared_ptr<Texture> texture) {
        texture_ = texture;
    }

    ~Mesh() {
    	glDeleteBuffers(1, &ebo_);
    	glDeleteBuffers(1, &vbo_);
    	glDeleteVertexArrays(1, &vao_);
    };
private:
    // safe defaults
    GLenum primitive_type_{GL_POINTS}; 

    // keep the data
    std::vector<Vertex> vertices_;
    std::vector<GLuint> indices_;
    
    // OpenGL buffer IDs
    // ID = 0 is reserved (i.e. uninitalized)
    GLuint vao_{0};
    GLuint vbo_{0};
    GLuint ebo_{0};

    std::shared_ptr<Texture> texture_;
};
