#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

class OrbitLine {
public:
    OrbitLine() = default;
    ~OrbitLine();

    void init();
    void update(const std::vector<glm::vec3>& pts);
    void draw() const;

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
};
