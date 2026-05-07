#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <memory> 

#include <GL/glew.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp> 
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "assets.hpp"
#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#include "OBJloader.hpp"
#include "Texture.hpp"

class Model {
public:
    // origin point of whole model
    glm::vec3 pivot_position{};
    glm::vec3 eulerAngles{};
    glm::vec3 scale{1.0f};

    bool is_transparent{false};
    float material_alpha{1.0f}; // 1.0 = full opaque 0 = transparent
    glm::vec3 emissive_color{0.0f};

    // Collision detection
    float bounding_radius{0.5f}; 
    bool collides{false};

    // Get world position of model
    glm::vec3 getPosition() const {
        return pivot_position;
    }

    // mesh  data
    struct mesh_package {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<ShaderProgram> shader;
        glm::vec3 origin;
        glm::vec3 eulerAngles;
        glm::vec3 scale;
        std::shared_ptr<Texture> texture;

        mesh_package(std::shared_ptr<Mesh> m, std::shared_ptr<ShaderProgram> s, glm::vec3 o, glm::vec3 e, glm::vec3 sc, std::shared_ptr<Texture> t = nullptr)
            : mesh(m), shader(s), origin(o), eulerAngles(e), scale(sc), texture(t) {}
    };
    std::vector<mesh_package> meshes;
    
    Model() = default;
    Model(const std::filesystem::path & filename, std::shared_ptr<ShaderProgram> shader, std::shared_ptr<Texture> texture = nullptr) {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        if (loadOBJ(filename, vertices, indices)) {
            auto mesh = std::make_shared<Mesh>(vertices, indices, GL_TRIANGLES);
            if (texture) mesh->setTexture(texture);
            addMesh(mesh, shader, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), texture);
        }
    }

    void addMesh(std::shared_ptr<Mesh> mesh,
                 std::shared_ptr<ShaderProgram> shader, 
                 glm::vec3 origin = glm::vec3(0.0f),      
                 glm::vec3 eulerAngles = glm::vec3(0.0f), 
                 glm::vec3 scale = glm::vec3(1.0f),
                 std::shared_ptr<Texture> texture = nullptr
                 ) {
        meshes.emplace_back(mesh, shader, origin, eulerAngles, scale, texture);
    }

    // update based on running time
    void update(const float delta_t) {
    }
    
    void draw() {
        // Calculate base model matrix for the model
        glm::mat4 T = glm::translate(glm::mat4(1.0f), pivot_position);
        glm::mat4 R = glm::yawPitchRoll(glm::radians(eulerAngles.y), glm::radians(eulerAngles.x), glm::radians(eulerAngles.z));
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
        glm::mat4 model_matrix = T * R * S;

        // call draw() on mesh
        for (auto const& mesh_pkg : meshes) {
            mesh_pkg.shader->use(); // select shader

            // Set material alpha
            mesh_pkg.shader->setUniform("u_material_alpha", material_alpha);
            mesh_pkg.shader->setUniform("u_emissive", emissive_color);
            
            // Calculate mesh for local transformation
            glm::mat4 mT = glm::translate(glm::mat4(1.0f), mesh_pkg.origin);
            glm::mat4 mR = glm::yawPitchRoll(glm::radians(mesh_pkg.eulerAngles.y), glm::radians(mesh_pkg.eulerAngles.x), glm::radians(mesh_pkg.eulerAngles.z));
            glm::mat4 mS = glm::scale(glm::mat4(1.0f), mesh_pkg.scale);
            glm::mat4 mesh_local_matrix = mT * mR * mS;

            // Set final model matrix uniform
            mesh_pkg.shader->setUniform("uM_m", model_matrix * mesh_local_matrix);

            mesh_pkg.mesh->draw();   // draw mesh
        }
    }

};
