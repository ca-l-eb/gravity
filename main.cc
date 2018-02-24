#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "display.h"
#include "pobject.h"
#include "shader.h"

static void handle_args(int argc, char *argv[], int &count, float &dt, float &step);
static void init_PBodies(PBodies &b);
static std::mutex mu;

static void doPhysics(PBodies *b, float dt, bool *updated, bool *running)
{
    while (true) {
        b->applyGravity(dt);
        std::lock_guard<std::mutex> guard(mu);
        *updated = true;  // Instance data needs updating... (in main thread)
        if (!*running) {
            break;
        }
    }
    std::cout << "physics thread finished" << std::endl;
}

int main(int argc, char *argv[])
{
    int width = 1600;
    int height = 900;
    GLDisplay disp(width, height, "Gravity");
    if (disp.wasError()) {
        return disp.wasError();
    }

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    float dt = 0.00005f;                // Time step in seconds
    int count = 1 << 11;                // Number of particles
    float step = float(M_PI / 300.0f);  // Camera rotation in radians
    handle_args(argc, argv, count, dt, step);

    PBodies b(count);
    init_PBodies(b);  // Generate random locations for the particles

    GLShader simpleShader("res/simple_mesh.vs", "res/simple_mesh.fs");
    simpleShader.use();

    // Time to link vertex data with the corresponding vertex attribute in the vertex shader
    GLint colorAttrib = simpleShader.getAttribLocation("inColor");
    GLint positionAttrib = simpleShader.getAttribLocation("position");

    GLint viewUniform = simpleShader.getUniformLocation("view");
    GLint projectionUniform = simpleShader.getUniformLocation("projection");

    GLuint vboPositions, vboColors, vao;
    glGenVertexArrays(1, &vao);  // Generate vao
    glGenBuffers(1, &vboPositions);
    glGenBuffers(1, &vboColors);
    glBindVertexArray(vao);

    // Set up color of circles
    glBindBuffer(GL_ARRAY_BUFFER, vboColors);
    glBufferData(GL_ARRAY_BUFFER, b.size() * sizeof(glm::vec3), b.color.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(colorAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(colorAttrib);

    // Set up offsets (positions of circles), needs to be updated every iteration
    glBindBuffer(GL_ARRAY_BUFFER, vboPositions);
    glBufferData(GL_ARRAY_BUFFER, b.size() * sizeof(glm::vec3), b.pos.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(positionAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(positionAttrib);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Tell OpenGL about the instanced data (color and position)
    glVertexAttribDivisor(colorAttrib, 1);
    glVertexAttribDivisor(positionAttrib, 1);

    glPointSize(2);

    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view;

    glm::mat4 perspectiveMatrix =
        glm::perspective(80.0f, (float) width / (float) height, 0.1f, 100.0f);
    glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(perspectiveMatrix));

    bool updatedPosition = false;
    bool running = true;

    std::thread physicsThread(doPhysics, &b, dt, &updatedPosition, &running);

    float counter = 0;

    int frames = 1;

    simpleShader.use();  // Bind the simple shader
    glBindVertexArray(vao);
    while (!disp.isClosed()) {
        auto start = std::chrono::high_resolution_clock::now();

        disp.clear(0.0f, 0.0f, 0.0f, 1.0f);
        if (disp.wasResized()) {
            perspectiveMatrix = glm::perspective(
                80.0f, (float) disp.getWidth() / (float) disp.getHeight(), 0.1f, 100.0f);
            glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(perspectiveMatrix));
            glViewport(0, 0, disp.getWidth(), disp.getHeight());
        }
        // Update transformation camera
        view =
            glm::lookAt(glm::vec3(2 * sin(counter), 1.1f * sin(1.3 * counter) * cos(.33f * counter),
                                  2 * cos(counter)),
                        cameraTarget, up);
        glUniformMatrix4fv(viewUniform, 1, GL_FALSE, glm::value_ptr(view));

        {
            // We don't want the other thread messing with data when we're moving it to the GPU
            std::lock_guard<std::mutex> guard(mu);
            if (updatedPosition) {
                glBindBuffer(GL_ARRAY_BUFFER, vboPositions);
                glBufferSubData(GL_ARRAY_BUFFER, 0, b.size() * sizeof(glm::vec3),
                                (GLvoid *) b.pos.data());
                updatedPosition = false;
            }
        }

        // Draw the instanced particle data
        glDrawArraysInstanced(GL_POINTS, 0, 3 * sizeof(glm::vec3), count);

        disp.update();
        frames++;
        counter += step;

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::this_thread::sleep_for(std::chrono::microseconds(16667 - elapsed_us));
    }

    running = false;
    physicsThread.join();

    return 0;
}

static void handle_args(int argc, char *argv[], int &count, float &dt, float &step)
{
    if (argc == 2) {
        std::string fs(argv[1]);
        step = std::stof(fs);
    }
    if (argc == 3) {
        std::string is(argv[1]);
        count = std::stoi(is);
        std::string fs(argv[2]);
        dt = std::stof(fs);
    }
    if (argc == 4) {
        std::string is(argv[1]);
        count = std::stoi(is);
        std::string fs(argv[2]);
        dt = std::stof(fs);
        fs = std::string(argv[3]);
        step = std::stof(fs) / 360.0f;
    }
}

static void init_PBodies(PBodies &b)
{
    int count = b.size();
    float range = 0.2f;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(-range, range);
    for (int i = 0; i < count - 1; i++) {
        float randX = dist(gen);
        float randY = dist(gen);
        float randZ = dist(gen);
        if (i < count / 2) {  // block 1
            b.pos[i] = {randX - 1.3f, randY, randZ};
            b.vel[i] = {0.0f, 110.0f, 0.0f};  // Good looping
            // b.vel[i] = { 0.0f, 160.0f, 0.0f }; // Good mixing
            b.color[i] = {0.0f, 1.0f, 0.0f};
        } else if (i < count) {  // block 2
            b.pos[i] = {randX + 1.3f, randY, randZ};
            b.vel[i] = {0.0f, -110.0f, 0.0f};
            // b.vel[i] = { 0.0f, -160.0f, 0.0f };
            b.color[i] = {1.0f, 0.0f, 1.0f};
        }
        b.mass[i] = fabs(dist(gen) * 9.5e8f);
        b.acc[i] = {0, 0, 0};
    }
    b.pos[count - 1] = {0.0f, 0.0f, 0.0f};
    b.mass[count - 1] = 5e14f;
    b.color[count - 1] = {1.0f, 1.0f, 1.0f};
}
