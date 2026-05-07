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
    glm::vec3 pivot_position{}; // [0,0,0] of the object
    glm::vec3 eulerAngles{};    // pitch, yaw, roll
    glm::vec3 scale{1.0f};

    // Transparency (Task 1 from Instructions)
    // - transparency = final fragment alpha < 1.0; this can happen because
    //   - model has transparent material (material_alpha < 1.0)
    //   - model has transparent texture
    // -> when updating material or texture, check alpha and set to TRUE when needed
    bool is_transparent{false};
    float material_alpha{1.0f}; // 1.0 = fully opaque, 0.0 = fully transparent

    // Collision detection (Task 2 from Instructions)
    float bounding_radius{0.5f}; // radius of bounding sphere for collision
    bool collides{false};        // whether this object participates in collision

    // Get world position of this model
    glm::vec3 getPosition() const {
        return pivot_position;
    }

    // mesh related data
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
        // Calculate base model matrix for the whole model
        glm::mat4 T = glm::translate(glm::mat4(1.0f), pivot_position);
        glm::mat4 R = glm::yawPitchRoll(glm::radians(eulerAngles.y), glm::radians(eulerAngles.x), glm::radians(eulerAngles.z));
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
        glm::mat4 model_matrix = T * R * S;

        // call draw() on mesh (all meshes)
        for (auto const& mesh_pkg : meshes) {
            mesh_pkg.shader->use(); // select proper shader

            // Set material alpha for transparency (Task 1)
            mesh_pkg.shader->setUniform("u_material_alpha", material_alpha);
            
            // Calculate mesh-local transformation
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
