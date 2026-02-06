#pragma once
#include <string>
#include <glad/glad.h>

struct Texture2D {
    GLuint id = 0;
    int w=0, h=0, comp=0;

    bool loadFromFile(const std::string& path, bool srgb=false);
    void destroy();
};
