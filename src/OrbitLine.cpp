#include "OrbitLine.h"

OrbitLine::~OrbitLine(){
    if(vbo) glDeleteBuffers(1, &vbo);
    if(vao) glDeleteVertexArrays(1, &vao);
}

void OrbitLine::init(){
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);
}

void OrbitLine::update(const std::vector<glm::vec3>& pts){
    count = (GLsizei)pts.size();
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pts.size() * sizeof(glm::vec3)), pts.data(), GL_DYNAMIC_DRAW);
}

void OrbitLine::draw() const{
    glBindVertexArray(vao);
    glDrawArrays(GL_LINE_STRIP, 0, count);
    glBindVertexArray(0);
}
