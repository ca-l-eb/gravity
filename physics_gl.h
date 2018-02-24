//
// Created by Caleb Dechant on 9/5/16.
//

#ifndef GRAVITY_PHYSICS_GL_H
#define GRAVITY_PHYSICS_GL_H

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <mutex>

#include "display.h"
#include "pobject.h"
#include "shader.h"

class physics_gl
{
public:
    physics_gl();
    physics_gl(int width, int height, int num_bodies, float dt);
    ~physics_gl();

    // private:
    GLint view_uniform, project_uniform;
    GLint positions_attrib, colors_attrib;
    GLuint positions_vbo, colors_vbo, vao;
    glm::mat4 perspective_matrix;
    std::mutex mutex;
    float step_dt, step_camera;
    int num_particles;

    GLDisplay disp;
    GLShader shader;
    PBodies bodies;

    void make_gl_buffers();
    void init_bodies();

    const int DEFAULT_BODIES_COUNT = 1000;
    const float DEFAULT_STEP_DT = 0.001f;
    const float DEFAULT_STEP_CAMERA = 0.01;
};

#endif  // GRAVITY_PHYSICS_GL_H
