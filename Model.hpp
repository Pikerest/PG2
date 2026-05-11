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

    float mesh_cull_radius{0.866f};  // max vertex distance from origin in model space

    float get_cull_radius() const {
        return mesh_cull_radius * std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)});
    }

    bool is_transparent{false};
    float material_alpha{1.0f}; // 1.0 = full opaque 0 = transparent
    glm::vec3 emissive_color{0.0f};
    glm::vec2 texture_scale{1.0f};
    bool swap_texture_axes{false};
    bool two_sided_lighting{false};

    // Collision detection
    float bounding_radius{0.5f};
    bool collides{false};

    // Get world position of model
    glm::vec3 getPosition() const {
        return pivot_position;
    }

    // mesh data
    struct mesh_package {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<ShaderProgram> shader;
        glm::vec3 origin;
        glm::vec3 eulerAngles;
        glm::vec3 scale;
        std::shared_ptr<Texture> texture;

        glm::mat4 local_matrix{glm::mat4(1.0f)};        // precomputed mT*mR*mS (never changes)
        mutable glm::mat4 cached_final_m{glm::mat4(1.0f)};
        mutable glm::mat3 cached_normal_m{glm::mat3(1.0f)};

        mesh_package(std::shared_ptr<Mesh> m, std::shared_ptr<ShaderProgram> s, glm::vec3 o, glm::vec3 e, glm::vec3 sc, std::shared_ptr<Texture> t = nullptr)
            : mesh(m), shader(s), origin(o), eulerAngles(e), scale(sc), texture(t)
        {
            glm::mat4 mT = glm::translate(glm::mat4(1.0f), o);
            glm::mat4 mR = glm::yawPitchRoll(glm::radians(e.y), glm::radians(e.x), glm::radians(e.z));
            glm::mat4 mS = glm::scale(glm::mat4(1.0f), sc);
            local_matrix = mT * mR * mS;
        }
    };
    std::vector<mesh_package> meshes;

    Model() = default;
    Model(const std::filesystem::path & filename, std::shared_ptr<ShaderProgram> shader, std::shared_ptr<Texture> texture = nullptr) {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        if (loadOBJ(filename, vertices, indices)) {
            mesh_cull_radius = 0.0f;
            for (const auto& v : vertices)
                mesh_cull_radius = std::max(mesh_cull_radius, glm::length(v.Position));
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
        if (mesh)
            mesh_cull_radius = std::max(mesh_cull_radius, mesh->get_mesh_bounding_radius());
        meshes.emplace_back(mesh, shader, origin, eulerAngles, scale, texture);
    }

    // update based on running time
    void update(const float delta_t) {
    }

    void draw() {
        // Recompute model matrix and normal matrices only when transform changed.
        // For static objects (most of the scene) this runs once at startup, then never again.
        if (pivot_position != _cpivot || eulerAngles != _ceuler || scale != _cscale) {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), pivot_position);
            glm::mat4 R = glm::yawPitchRoll(glm::radians(eulerAngles.y), glm::radians(eulerAngles.x), glm::radians(eulerAngles.z));
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            _cmodel = T * R * S;
            for (auto& pkg : meshes) {
                pkg.cached_final_m  = _cmodel * pkg.local_matrix;
                pkg.cached_normal_m = glm::mat3(glm::transpose(glm::inverse(pkg.cached_final_m)));
            }
            _cpivot = pivot_position;
            _ceuler = eulerAngles;
            _cscale = scale;
        }

        for (auto const& mesh_pkg : meshes) {
            mesh_pkg.shader->use();

            mesh_pkg.shader->setUniform("u_material_alpha", material_alpha);
            mesh_pkg.shader->setUniform("u_emissive", emissive_color);
            mesh_pkg.shader->setUniform("u_tex_scale", texture_scale);
            mesh_pkg.shader->setUniform("u_swap_tex_axes", swap_texture_axes ? 1 : 0);
            mesh_pkg.shader->setUniform("u_two_sided", two_sided_lighting ? 1 : 0);

            mesh_pkg.shader->setUniform("uM_m", mesh_pkg.cached_final_m);
            mesh_pkg.shader->setUniform("uNormal_m", mesh_pkg.cached_normal_m);

            mesh_pkg.mesh->draw();
        }
    }

private:
    mutable glm::vec3 _cpivot{1e30f, 1e30f, 1e30f};
    mutable glm::vec3 _ceuler{1e30f, 1e30f, 1e30f};
    mutable glm::vec3 _cscale{1e30f, 1e30f, 1e30f};
    mutable glm::mat4 _cmodel{glm::mat4(1.0f)};
};
