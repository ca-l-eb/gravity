//
// Created by dechant on 7/7/16.
//

#include <iostream>
#include <string.h>
#include <GL/glew.h>

#ifndef __APPLE__
#include <GL/gl.h>
#else
#include <OpenGL/gl.h>
#endif


#include "shader.h"
#include "simpleio.h"

static GLuint setup_shader(const char* source, GLint type);
static void check_shader_errors(GLuint shader, GLuint flag);

GLShader::GLShader(const char* vertexFile, const char* fragFile) {
    program = glCreateProgram();

    const char* vertexSource = read_file(vertexFile);
    const char* fragSource = read_file(fragFile);

    shaders[VERTEX_SHADER] = setup_shader(vertexSource, GL_VERTEX_SHADER);
    shaders[FRAGMENT_SHADER] = setup_shader(fragSource, GL_FRAGMENT_SHADER);

    delete[] vertexSource;
    delete[] fragSource;

    for (unsigned int i = 0; i < NUM_SHADERS; i++) {
        glAttachShader(program, shaders[i]);
    }

    // Set outColor to be the output fragment
    glBindFragDataLocation(program, 0, "outColor");

    glLinkProgram(program);
    glValidateProgram(program);
    check_shader_errors(program, GL_LINK_STATUS);
}

GLShader::~GLShader() {
    for (unsigned int i = 0; i < NUM_SHADERS; i++ ) {
        glDetachShader(program, shaders[i]);
        glDeleteShader(shaders[i]);
    }
    glDeleteProgram(program);
}

GLuint GLShader::getProgram() {
    return program;
}

GLint GLShader::getAttribLocation(const char *attribute) {
    return glGetAttribLocation(program, attribute);
}

GLint GLShader::getUniformLocation(const char* uniform) {
    return glGetUniformLocation(program, uniform);
}

void GLShader::use() {
    glUseProgram(program);
}

static void check_shader_errors(GLuint shader, GLuint flag) {
    GLint status;
    glGetShaderiv(shader, flag, &status);
    if (status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(shader, 512, NULL, buffer);
        std::cout << buffer << std::endl;
    }
}

static GLuint setup_shader(const char* source, GLint type) {
    GLuint shader = glCreateShader(type); // Create a vertex shader reference
    glShaderSource(shader, 1, &source, NULL); // Associate the source with the shader reference
    glCompileShader(shader); // Compile the shader
    check_shader_errors(shader, GL_COMPILE_STATUS);
    return shader;
}