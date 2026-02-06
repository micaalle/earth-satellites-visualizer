#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 pos {0.0f, 0.0f, 6.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 55.0f;

    float speed = 5.0f;
    float mouseSens = 0.10f;

    glm::vec3 front() const;
    glm::mat4 view() const;

    void processMouseDelta(float dx, float dy);
};
