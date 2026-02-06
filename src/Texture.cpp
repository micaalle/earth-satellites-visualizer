#include "Texture.h"
#include <iostream>

#include "stb_image.h"

bool Texture2D::loadFromFile(const std::string& path, bool srgb){
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 0);
    if(!data){
        std::cerr << "Texture load failed: " << path << "\n";
        return false;
    }

    GLenum fmt = (comp == 4) ? GL_RGBA : GL_RGB;
    GLenum internal = fmt;
    if(srgb){
        internal = (fmt == GL_RGBA) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
    }

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    return true;
}

void Texture2D::destroy(){
    if(id){
        glDeleteTextures(1, &id);
        id = 0;
    }
}
