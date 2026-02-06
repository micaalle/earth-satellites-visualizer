#pragma once
#include <vector>
#include <glad/glad.h>

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    void upload(const std::vector<Vertex>& verts, const std::vector<uint32_t>& idx);
    void draw() const;

private:
    GLuint vao=0, vbo=0, ebo=0;
    GLsizei indexCount=0;
};
