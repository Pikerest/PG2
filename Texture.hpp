#pragma once 

/*
 * OpenGL texture wrapper.
 * Textures can be loaded from disk through OpenCV or generated as 1x1 solid
 * colors for procedural scene materials.
 */

#include <filesystem>
#include <opencv2/opencv.hpp>
#include <GL/glew.h> 
#include <glm/glm.hpp>

#include "non_copyable.hpp"

class Texture : private NonCopyable
{
public:
    // Minification/magnification setup used after uploading image data.
    enum class Interpolation {
        nearest,
        linear,
        linear_mipmap_linear,
    };

    Texture() = default;

    // Build a texture from an existing OpenCV image.
    Texture(const cv::Mat & image, Interpolation interpolation = Interpolation::linear_mipmap_linear);

    // Convenience constructors for solid-color materials.
    Texture(const glm::vec3 & vec);
    Texture(const glm::vec4 & vec);

    // Load image from disk and upload it to the GPU.
    Texture(const std::filesystem::path & path, Interpolation interpolation = Interpolation::linear_mipmap_linear);

    ~Texture();

    // Bind to the requested texture unit for the active shader.
    void bind(GLuint unit = 0) const;

    // Returns a valid OpenGL texture name. Empty textures fall back to a small
    // checkerboard so missing bindings are visible during debugging.
    GLuint get_name() const;
    int get_height(void) const;
    int get_width(void) const;
    void set_interpolation(Interpolation interpolation);
    void replace_image(const cv::Mat& image);

private:
    cv::Mat load_image(const std::filesystem::path& path);

    // Shared fallback texture used when name_ is zero.
    static GLuint gen_ckboard(void);
    static inline GLuint ckboard_{ 0 }; 
    GLuint name_{ 0 }; 
};
