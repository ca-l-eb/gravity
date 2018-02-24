//
// Created by Caleb Dechant on 9/5/16.
//
#include <glm/glm.hpp>
#include <random>

#include "physics_gl.h"

// Call other constructor if given no parameters
physics_gl::physics_gl() : physics_gl(640, 480, DEFAULT_BODIES_COUNT, DEFAULT_STEP_DT) {}

physics_gl::physics_gl(int width, int height, int num_bodies, float dt)
    : disp(width, height, "Gravity Physics GL")
    , shader("res/simple_mesh.vs", "res/simple_mesh.fs")
    , bodies(num_bodies)
{
    num_particles = num_bodies;
    init_bodies();

    positions_attrib = shader.getAttribLocation("position");
    colors_attrib = shader.getAttribLocation("inColor");
    view_uniform = shader.getUniformLocation("view");
    project_uniform = shader.getUniformLocation("projection");

    make_gl_buffers();
    step_dt = dt;
    step_camera = DEFAULT_STEP_CAMERA;  // Just something for now
}

physics_gl::~physics_gl()
{
    // TODO: Delete OpenGL data
}

void physics_gl::make_gl_buffers()
{
    glGenVertexArrays(1, &vao);  // Generate vao, stores info about layout
    glGenBuffers(1, &positions_vbo);
    glGenBuffers(1, &colors_vbo);

    glBindVertexArray(vao);  // Make sure we remember how data is formatted, etc

    // Set up color of circles
    glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
    glBufferData(GL_ARRAY_BUFFER, bodies.size() * sizeof(glm::vec3), bodies.color.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(colors_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(colors_attrib);

    // Set up offsets (positions of circles), needs to be updated every iteration
    glBindBuffer(GL_ARRAY_BUFFER, positions_vbo);
    glBufferData(GL_ARRAY_BUFFER, bodies.size() * sizeof(glm::vec3), bodies.pos.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(positions_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(positions_attrib);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Tell OpenGL about the instanced data (color and position)
    glVertexAttribDivisor(colors_attrib, 1);
    glVertexAttribDivisor(positions_attrib, 1);
}

void physics_gl::init_bodies()
{
    auto count = bodies.size();
    auto range = 0.2f;
    std::random_device rd;
    auto gen = std::mt19937(rd());
    auto dist = std::uniform_real_distribution<float>(-range, range);
    for (int i = 0; i < count - 1; i++) {
        auto randX = dist(gen);
        auto randY = dist(gen);
        auto randZ = dist(gen);

#define FOUR_BLOCKS

#if defined(TWO_BLOCKS)
        if (i < count / 2) {  // block 1
            bodies.pos[i] = {randX - 1.3f, randY, randZ};
            // bodies.vel[i] = { 0.0f, 110.0f, 0.0f }; // Good looping
            bodies.vel[i] = {0.0f, 160.0f, 0.0f};  // Good mixing
            bodies.color[i] = {0.0f, 1.0f, 0.0f};
        } else if (i < count) {  // block 2
            bodies.pos[i] = {randX + 1.3f, randY, randZ};
            // bodies.vel[i] = { 0.0f, -110.0f, 0.0f };
            bodies.vel[i] = {0.0f, -160.0f, 0.0f};
            bodies.color[i] = {1.0f, 0.0f, 1.0f};
        }
#elif defined(FOUR_BLOCKS)
        if (i < (1.0f/4.0f) * count) { // block 1
            bodies.pos[i] = { randX - 1.3f, randY, randZ };
            bodies.vel[i] = { 0.0f, 110.0f, 0.0f }; // Good looping
            //bodies.vel[i] = { 0.0f, 160.0f, 0.0f }; // Good mixing
            bodies.color[i] = { 0.0f, 1.0f, 0.0f };
        }
        else if (i < (2.0f/3.0f) * count) { // block 2
            bodies.pos[i] = { randX + 1.3f, randY, randZ };
            bodies.vel[i] = { 0.0f, -110.0f, 0.0f };
            //	bodies.vel[i] = { 0.0f, -160.0f, 0.0f };
            bodies.color[i] = { 1.0f, 0.0f, 1.0f };
        }
        else if (i < (3.0f/4.0f) * count) { // block 3
            bodies.pos[i] = { randX, randY +1.3, randZ };
            bodies.vel[i] = { 0.0f, 0.0f, 110.0f };
            //	bodies.vel[i] = { 0.0f, -160.0f, 0.0f };
            bodies.color[i] = { 1.0f, 1.0f, 1.0f };
        }
        else { // block 4
            bodies.pos[i] = { randX, randY-1.3f, randZ };
            bodies.vel[i] = { 0.0f, 0.0f, -110.0f };
            //	bodies.vel[i] = { 0.0f, -160.0f, 0.0f };
            bodies.color[i] = { 1.0f, 0.0f, 0.0f };
        }
#endif
        bodies.mass[i] = static_cast<float>(fabs(dist(gen) * 9.5e9f));
        bodies.acc[i] = {0, 0, 0};
        // bodies.color[i] = { fabs(dist(gen)), fabs(dist(gen)), fabs(dist(gen)) };
    }
    bodies.pos[count - 1] = {0.0f, 0.0f, 0.0f};
    bodies.mass[count - 1] = 5e14f;
    bodies.color[count - 1] = {1.0f, 1.0f, 1.0f};
}
