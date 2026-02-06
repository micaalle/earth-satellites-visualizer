#include "Camera.h"
#include <algorithm>
#include <cmath>

glm::vec3 Camera::front() const {
    float cy = cos(glm::radians(yaw));
    float sy = sin(glm::radians(yaw));
    float cp = cos(glm::radians(pitch));
    float sp = sin(glm::radians(pitch));
    glm::vec3 f(cy * cp, sp, sy * cp);
    return glm::normalize(f);
}

glm::mat4 Camera::view() const {
    return glm::lookAt(pos, pos + front(), glm::vec3(0,1,0));
}

void Camera::processMouseDelta(float dx, float dy) {
    yaw   += dx * mouseSens;
    pitch -= dy * mouseSens;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}
