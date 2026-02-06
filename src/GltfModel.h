#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

class Shader;

class GltfModel {
public:
    bool loadFromFile(const std::string& path, bool srgbBaseColor = true);
    void destroy();

    void drawEarthStyle(Shader& earthShader, int textureUnit = 0) const;

    float boundsRadius() const { return m_boundsRadius; }

private:
    struct Primitive {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
        bool hasIndices = false;
        GLenum mode = GL_TRIANGLES;
        int baseColorTexIndex = -1; 
    };

    struct Texture {
        GLuint id = 0;
        bool valid = false;
    };

    std::vector<Primitive> m_prims;
    std::vector<Texture>   m_textures;
    float m_boundsRadius = 1.0f;

private:
    GLuint createTextureFromMemoryRGBA(const unsigned char* rgba, int w, int h, bool srgb);
};
