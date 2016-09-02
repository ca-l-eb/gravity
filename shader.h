//
// Created by dechant on 7/7/16.
//

#ifndef GRAVITY_SHADER_H
#define GRAVITY_SHADER_H

#ifndef __APPLE__
#include <GL/gl.h>
#else
#include <OpenGL/gl.h>
#endif

class Shader {

public:
    Shader(const char* vertexFile, const char* fragFile);
    ~Shader();
    GLint getAttribLocation(const char* attribute);
    GLint getUniformLocation(const char* uniform);
    GLuint getProgram();
    void use();

private:
    enum { VERTEX_SHADER, FRAGMENT_SHADER, NUM_SHADERS };
    GLuint program;
    GLuint shaders[NUM_SHADERS];
};


#endif //GRAVITY_SHADER_H
