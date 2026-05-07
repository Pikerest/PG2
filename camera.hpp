#pragma once

#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // Added for glm::lookAt

enum class CameraMode {
    FREE_FLOATING,
    POV_LOCKED
};

class Camera
{
public:
    // Camera State
    CameraMode Mode = CameraMode::FREE_FLOATING;
    const glm::vec3* TargetPosition = nullptr; // Pointer to the tracked object's position
    glm::vec3 POVOffset = glm::vec3(0.0f, 0.0f, 0.0f); // Offset from the target (e.g., eye level)

    // Camera Attributes
    glm::vec3 Position{};
    glm::vec3 Front{};
    glm::vec3 Right{};
    glm::vec3 Up{}; // camera local UP vector
    glm::vec3 WorldUp{0.0f, 1.0f, 0.0f};

    // Euler Angles
    GLfloat Yaw = -90.0f;
    GLfloat Pitch =  0.0f;
    GLfloat Roll = 0.0f;

    // Camera options
    GLfloat MovementSpeed = 2.5f;
    GLfloat MouseSensitivity = 0.1f;

    // Default constructor
    Camera() {
        this->updateCameraVectors();
    }

    // Constructor with vectors
    Camera(glm::vec3 position) : Position(position) {
        this->updateCameraVectors();
    }

    // Call this every frame in your game loop
    void Update() {
        if (Mode == CameraMode::POV_LOCKED && TargetPosition != nullptr) {
            // Snap camera to the target's position plus the offset
            this->Position = *TargetPosition + POVOffset;
        }
    }

    // Attach the camera to an object for POV mode
    void AttachTo(const glm::vec3* targetPos, glm::vec3 offset = glm::vec3(0.0f, 0.5f, 0.0f)) {
        this->Mode = CameraMode::POV_LOCKED;
        this->TargetPosition = targetPos;
        this->POVOffset = offset;
    }

    // Detach and return to free fly mode
    void Detach() {
        this->Mode = CameraMode::FREE_FLOATING;
        this->TargetPosition = nullptr;
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(this->Position, this->Position + this->Front, this->Up);
    }

    // Processes keyboard input
    void ProcessInput(GLFWwindow* window, GLfloat deltaTime) {
        // If locked to an object, the object handles its own movement.
        // We shouldn't move the camera independently with keys.
        if (Mode == CameraMode::POV_LOCKED) return;

        glm::vec3 direction{0.0f};

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            direction += Front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            direction -= Front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            direction -= Right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            direction += Right;

        // Up/Down for true free-floating movement
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            direction += WorldUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            direction -= WorldUp;

        // Fix: Only normalize if the direction vector isn't zero to avoid NaN errors
        if (glm::length(direction) > 0.0f) {
            this->Position += glm::normalize(direction) * MovementSpeed * deltaTime;
        }
    }

    // Processes mouse input
    void ProcessMouseMovement(GLfloat xoffset, GLfloat yoffset, GLboolean constraintPitch = GL_TRUE) {
        xoffset *= this->MouseSensitivity;
        yoffset *= this->MouseSensitivity;

        this->Yaw   += xoffset;
        this->Pitch += yoffset;

        if (constraintPitch) {
            if (this->Pitch > 89.0f) this->Pitch = 89.0f;
            if (this->Pitch < -89.0f) this->Pitch = -89.0f;
        }

        this->updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(this->Yaw)) * cos(glm::radians(this->Pitch));
        front.y = sin(glm::radians(this->Pitch));
        front.z = sin(glm::radians(this->Yaw)) * cos(glm::radians(this->Pitch));

        this->Front = glm::normalize(front);
        this->Right = glm::normalize(glm::cross(this->Front, this->WorldUp));
        this->Up    = glm::normalize(glm::cross(this->Right, this->Front));
    }
};