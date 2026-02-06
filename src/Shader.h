#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vsPath, const std::string& fsPath);
    ~Shader();

    bool load(const std::string& vsPath, const std::string& fsPath);
    void use() const;

    GLuint id() const { return m_id; }

    void setMat4(const char* name, const glm::mat4& m) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setInt(const char* name, int v) const;
    void setBool(const char* name, bool v) const;
    void setFloat(const char* name, float v) const;

private:
    GLuint m_id = 0;
    static std::string readFile(const std::string& path);
    static GLuint compile(GLenum type, const std::string& src, const std::string& debugName);
};
