#pragma once

/*
 * Simple first-person camera.
 * The App owns collision/gravity; Camera only converts keyboard/mouse input
 * into view vectors and free-fly or ground-plane movement.
 */

#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class CameraMode {
    // Normal player-controlled camera.
    FREE_FLOATING,

    // Optional mode for attaching the camera to another object's position.
    POV_LOCKED
};

class Camera
{
public:
    // Camera state. TargetPosition is not owned; caller must keep it alive.
    CameraMode Mode = CameraMode::FREE_FLOATING;
    const glm::vec3* TargetPosition = nullptr;
    glm::vec3 POVOffset = glm::vec3(0.0f, 0.0f, 0.0f);

    // View basis vectors. Front/Right/Up are derived from yaw/pitch.
    glm::vec3 Position{};
    glm::vec3 Front{};
    glm::vec3 Right{};
    glm::vec3 Up{};
    glm::vec3 WorldUp{0.0f, 1.0f, 0.0f};

    // Euler angles in degrees. Roll is kept for completeness but currently
    // the gameplay camera only changes yaw and pitch.
    GLfloat Yaw = -90.0f;
    GLfloat Pitch =  0.0f;
    GLfloat Roll = 0.0f;

    // Runtime-tuned by gameplay: sprinting changes MovementSpeed.
    GLfloat MovementSpeed = 2.5f;
    GLfloat MouseSensitivity = 0.1f;

    // Default camera looks down -Z from the origin.
    Camera() {
        this->updateCameraVectors();
    }

    // Construct at a specific world position.
    Camera(glm::vec3 position) : Position(position) {
        this->updateCameraVectors();
    }

    // Call each frame only when POV_LOCKED mode is used.
    void Update() {
        if (Mode == CameraMode::POV_LOCKED && TargetPosition != nullptr) {
            this->Position = *TargetPosition + POVOffset;
        }
    }

    // Attach camera to an external position pointer, for future cutscene/POV use.
    void AttachTo(const glm::vec3* targetPos, glm::vec3 offset = glm::vec3(0.0f, 0.5f, 0.0f)) {
        this->Mode = CameraMode::POV_LOCKED;
        this->TargetPosition = targetPos;
        this->POVOffset = offset;
    }

    // Detach and return to normal player-controlled movement.
    void Detach() {
        this->Mode = CameraMode::FREE_FLOATING;
        this->TargetPosition = nullptr;
    }

    // View matrix consumed by shader uniforms.
    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(this->Position, this->Position + this->Front, this->Up);
    }

    // Reads WASD and optionally vertical movement.
    // flyMode=true is used for noclip; otherwise movement is projected onto
    // the XZ plane so looking up/down does not change walking height.
    void ProcessInput(GLFWwindow* window, GLfloat deltaTime, bool flyMode = false) {
        if (Mode == CameraMode::POV_LOCKED) return;

        glm::vec3 direction{0.0f};
        glm::vec3 moveFront = Front;
        glm::vec3 moveRight = Right;

        if (!flyMode) {
            // Ground movement ignores camera pitch and rebuilds a horizontal
            // forward/right basis from the current look direction.
            moveFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
            moveRight = glm::normalize(glm::cross(moveFront, WorldUp));
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            direction += moveFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            direction -= moveFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            direction -= moveRight;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            direction += moveRight;

        if (flyMode) {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                direction += WorldUp;
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                direction -= WorldUp;
        }

        if (glm::length(direction) > 0.0f) {
            this->Position += glm::normalize(direction) * MovementSpeed * deltaTime;
        }
    }

    // Converts mouse deltas into yaw/pitch and rebuilds the view basis.
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
    // Rebuilds Front/Right/Up after yaw or pitch changes.
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
