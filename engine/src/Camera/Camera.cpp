#include "Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    
    if (Projection == ProjectionMode::Perspective) {
        // existing behavior: perspective using Zoom (fov), aspect, near/far
        return glm::perspective(glm::radians(Zoom), aspectRatio, NearPlane, FarPlane);
    }
    else {
        // Orthographic: compute left/right/top/bottom from OrthoSize and aspect
        float halfH = OrthoSize;
        float halfW = OrthoSize * aspectRatio;
        float left = -halfW;
        float right = halfW;
        float bottom = -halfH;
        float top = halfH;
        // glm::ortho(left, right, bottom, top, near, far)
        return glm::ortho(left, right, bottom, top, NearPlane, FarPlane);
    }

   // return glm::perspective(glm::radians(Zoom), aspectRatio, 0.1f, 100.0f);
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    float velocity = MovementSpeed * deltaTime;
    if (direction == FORWARD)
        Position += Front * velocity;
    if (direction == BACKWARD)
        Position -= Front * velocity;
    if (direction == LEFT)
        Position -= Right * velocity;
    if (direction == RIGHT)
        Position += Right * velocity;
    if (direction == UP)
        Position += Up * velocity;
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    if (constrainPitch) {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset) {
    Zoom -= (float)yoffset;
    if (Zoom < 1.0f)
        Zoom = 1.0f;
    if (Zoom > 45.0f)
        Zoom = 45.0f;
}



void Camera::SetOrthographicTopDown(const glm::vec3& center, float orthoHalfHeight, float heightAbove)
{
    Projection = ProjectionMode::Orthographic;
    OrthoSize = orthoHalfHeight;

    // Put camera directly above the center
    Position = center + glm::vec3(0.0f, heightAbove, 0.0f);

    // For top-down we want the camera to look straight down the -Y axis
    Front = glm::normalize(center - Position); // should result in (0, -1, 0)
    // Avoid using WorldUp when it's colinear with Front; pick stable Right/Up for top-down:
    Right = glm::vec3(1.0f, 0.0f, 0.0f);        // +X
    Up = glm::vec3(0.0f, 0.0f, -1.0f);       // -Z (so "up" in screen maps to -Z in world)

    // Update yaw/pitch from Front (keeps other code consistent)
    Yaw = glm::degrees(atan2(Front.z, Front.x));
    Pitch = glm::degrees(asin(glm::clamp(Front.y, -1.0f, 1.0f)));
    
}

void Camera::SetPerspectiveFromDefaults()
{
    Projection = ProjectionMode::Perspective;
    Zoom = ZOOM; // your default FOV constant
    // Optionally reset position/yaw/pitch or keep current values
    updateCameraVectors();
}

void Camera::SetPositionYawPitch(const glm::vec3& pos, float yaw, float pitch)
{
    Position = pos;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors(); // private method updates Front/Right/Up
}

void Camera::ResetToDefaults(const glm::vec3& pos, float yaw, float pitch)
{
    Zoom = ZOOM;
    SetPositionYawPitch(pos, yaw, pitch);
}

void Camera::FocusOn(const glm::vec3& target, float distance)
{
    glm::vec3 front = Front;
    if (glm::length(front) < 1e-6f) {
        front = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    else {
        front = glm::normalize(front);
    }

    // Place camera at target - front * distance so camera looks at target
    Position = target - front * distance;

    // Recompute Front (should point at target), Right and Up
    Front = glm::normalize(target - Position);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));

    // Update yaw/pitch to match new Front so other operations remain consistent
    // yaw = atan2(z, x), pitch = asin(y)
    Yaw = glm::degrees(atan2(Front.z, Front.x));
    Pitch = glm::degrees(asin(glm::clamp(Front.y, -1.0f, 1.0f)));

}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}