//
// Created by Caleb Dechant on 9/5/16.
//
#include <glm/glm.hpp>
#include <random>

#include "physics_gl.h"


// Call other constructor if given no parameters
physics_gl::physics_gl() : physics_gl(640, 480, DEFAULT_BODIES_COUNT, DEFAULT_STEP_DT) { }

physics_gl::physics_gl(int width, int height, int num_bodies, float dt) :
    disp(width, height, "Gravity Physics GL"),
    shader("res/simple_mesh.vs", "res/simple_mesh.fs"),
    bodies(num_bodies){

    num_particles = num_bodies;
    init_bodies();

    vertices_attrib = shader.getAttribLocation("vertices");
    positions_attrib = shader.getAttribLocation("position");
    colors_attrib = shader.getAttribLocation("inColor");
    scale_attrib = shader.getAttribLocation("scale");
    view_uniform = shader.getUniformLocation("view");
    project_uniform = shader.getUniformLocation("projection");

    make_gl_buffers();
    step_dt = dt;
    step_camera = DEFAULT_STEP_CAMERA; // Just something for now


    // Wire-frame rendering, makes particles easier to see
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_DEPTH_TEST);

}

physics_gl::~physics_gl() {
    // TODO: Delete OpenGL data
}

void physics_gl::make_gl_buffers() {

    glGenVertexArrays(1, &vao); // Generate vao, stores info about layout
    glGenBuffers(1, &vertices_vbo);
    glGenBuffers(1, &positions_vbo);
    glGenBuffers(1, &colors_vbo);
    glGenBuffers(1, &scale_vbo);

    glBindVertexArray(vao); // Make sure we remember how data is formatted, etc

    // Particle used in our system
    glm::vec3 particle[] = {{-0.5f, -0.5f, 0.0f},
                            {0.0f, 0.5f, 0.0f},
                            {0.5f, -0.5f, 0.0f} };

    // Single triangle as particle instead of circle as above
    glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(particle), particle, GL_STATIC_DRAW);
    glVertexAttribPointer(vertices_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(vertices_attrib);

    // Set up scale of circles
    glBindBuffer(GL_ARRAY_BUFFER, scale_vbo);
    glBufferData(GL_ARRAY_BUFFER, bodies.size() * sizeof(GLfloat), bodies.radii, GL_STATIC_DRAW);
    glVertexAttribPointer(scale_attrib, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(scale_attrib);

    // Set up color of circles
    glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
    glBufferData(GL_ARRAY_BUFFER, bodies.size() * sizeof(glm::vec3), bodies.color, GL_STATIC_DRAW);
    glVertexAttribPointer(colors_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(colors_attrib);

    // Set up offsets (positions of circles), needs to be updated every iteration
    glBindBuffer(GL_ARRAY_BUFFER, positions_vbo);
    glBufferData(GL_ARRAY_BUFFER, bodies.size() * sizeof(glm::vec3), bodies.pos, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(positions_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(positions_attrib);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Tell OpenGL about the instanced data (color, scale, and position)
    glVertexAttribDivisor(colors_attrib, 1);
    glVertexAttribDivisor(scale_attrib, 1);
    glVertexAttribDivisor(positions_attrib, 1);
}

void physics_gl::init_bodies() {
    int count = bodies.size();
    float range = 0.2f;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(-range, range);
    for (int i = 0; i < count-1; i++) {
        float randX = static_cast<float>(dist(gen));
        float randY = static_cast<float>(dist(gen));
        float randZ = static_cast<float>(dist(gen));
        if (i < count/2) { // block 1
            bodies.pos[i] = { randX - 1.3f, randY, randZ };
            bodies.vel[i] = { 0.0f, 110.0f, 0.0f }; // Good looping
//			bodies.vel[i] = { 0.0f, 160.0f, 0.0f }; // Good mixing
            bodies.color[i] = { 0.0f, 1.0f, 0.0f };
        }
        else if (i < count) { // block 2
            bodies.pos[i] = { randX + 1.3f, randY, randZ };
            bodies.vel[i] = { 0.0f, -110.0f, 0.0f };
//			bodies.vel[i] = { 0.0f, -160.0f, 0.0f };
            bodies.color[i] = { 1.0f, 0.0f, 1.0f };
        }
        float mass = static_cast<float>(fabs(dist(gen) * 9.5e8f));
        bodies.mass[i] = mass;
        bodies.acc[i] = { 0, 0, 0 };
        bodies.radii[i] = static_cast<float>(pow(mass, 0.333) * 6.9e-6f);
        //bodies.color[i] = { fabs(dist(gen)), fabs(dist(gen)), fabs(dist(gen)) };
    }
    bodies.pos[count-1] = {0.0f, 0.0f, 0.0f};
    bodies.mass[count-1] = 5e14f;
    bodies.color[count-1] = {1.0f, 1.0f, 1.0f};
    bodies.radii[count-1] = static_cast<float>(pow(1e14f, .333) * 2.9e-6f);
}