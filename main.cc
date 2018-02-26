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
#include "physics_gl.h"
#include "pobject.h"
#include "shader.h"

static std::mutex mu;

static void do_physics(PBodies *b, float dt, bool *updated, bool *running)
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

int main(int argc, char *argv[])
{
    GLDisplay disp(1600, 900, "Gravity");
    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    auto dt = 0.00005f;
    auto count = 1 << 11;
    auto step = static_cast<float>(M_PI) / 300.0f;
    handle_args(argc, argv, count, dt, step);

    physics_gl pgl{count, dt};
    pgl.use_shader();
    pgl.bind();

    glPointSize(2);

    auto cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view;

    auto aspect_ratio = static_cast<float>(disp.width()) / disp.height();
    pgl.set_perspective(aspect_ratio, 0.1f, 100.0f);

    auto b = pgl.get_bodies();
    auto updatedPosition = false;
    auto running = true;
    std::thread physics_thread{&do_physics, b, dt, &updatedPosition, &running};
    auto counter = 0.0f;
    auto frames = 1;

    while (!disp.is_closed()) {
        auto start = std::chrono::high_resolution_clock::now();

        disp.clear(0.0f, 0.0f, 0.0f, 1.0f);
        if (disp.resized()) {
            aspect_ratio = static_cast<float>(disp.width()) / disp.height();
            pgl.set_perspective(aspect_ratio, 0.1f, 100.f);
            glViewport(0, 0, disp.width(), disp.height());
        }
        // Update transformation camera
        view =
            glm::lookAt(glm::vec3(2 * sin(counter), 1.1f * sin(1.3 * counter) * cos(.33f * counter),
                                  2 * cos(counter)),
                        cameraTarget, up);
        pgl.set_view(view);

        {
            // We don't want the other thread messing with data when we're moving it to the GPU
            std::lock_guard<std::mutex> guard(mu);
            if (updatedPosition) {
                pgl.update_positions();
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
    physics_thread.join();

    return 0;
}
