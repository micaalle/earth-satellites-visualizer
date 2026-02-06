#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::string Shader::readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if(!f.is_open()){
        std::cerr << "Failed to open shader file: " << path << "\n";
        return {};
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint Shader::compile(GLenum type, const std::string& src, const std::string& debugName){
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){
        GLint len = 0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "Shader compile failed (" << debugName << "):\n" << log << "\n";
    }
    return s;
}

Shader::Shader(const std::string& vsPath, const std::string& fsPath){
    load(vsPath, fsPath);
}

Shader::~Shader(){
    if(m_id) glDeleteProgram(m_id);
}

bool Shader::load(const std::string& vsPath, const std::string& fsPath){
    std::string vs = readFile(vsPath);
    std::string fs = readFile(fsPath);
    if(vs.empty() || fs.empty()) return false;

    GLuint v = compile(GL_VERTEX_SHADER, vs, vsPath);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs, fsPath);

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){
        GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::cerr << "Program link failed:\n" << log << "\n";
        glDeleteProgram(p);
        return false;
    }

    if(m_id) glDeleteProgram(m_id);
    m_id = p;
    return true;
}

void Shader::use() const { glUseProgram(m_id); }

void Shader::setMat4(const char* name, const glm::mat4& m) const {
    GLint loc = glGetUniformLocation(m_id, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}
void Shader::setVec3(const char* name, const glm::vec3& v) const {
    GLint loc = glGetUniformLocation(m_id, name);
    glUniform3fv(loc, 1, &v[0]);
}
void Shader::setInt(const char* name, int v) const {
    GLint loc = glGetUniformLocation(m_id, name);
    glUniform1i(loc, v);
}
void Shader::setBool(const char* name, bool v) const {
    GLint loc = glGetUniformLocation(m_id, name);
    glUniform1i(loc, v ? 1 : 0);
}
void Shader::setFloat(const char* name, float v) const {
    GLint loc = glGetUniformLocation(m_id, name);
    glUniform1f(loc, v);
}
