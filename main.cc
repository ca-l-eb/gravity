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

#include "args.h"
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

struct program_args {
    int count;
    float dt;
    float camera_step;
};

static program_args parse_args(int argc, char *argv[])
{
    arg_parser parser{"gravity"};
    parser.add_arg({"-n", "number of objects", 1});
    parser.add_arg({"-dt", "time step", 1});
    parser.add_arg({"-rot", "camera rotation speed", 1});
    parser.add_arg({"-h", "help", 0});

    parser.parse(argc, argv);

    bool help = parser.find("-h").get(false);
    if (help) {
        parser.show_help();
        exit(0);
    }

    program_args args;
    args.count = parser.find("-n").get(1 << 12);
    args.dt = parser.find("-dt").get(0.00005f);
    args.camera_step = parser.find("-rot").get(0.0f);

    return args;
}

int main(int argc, char *argv[])
{
    GLDisplay disp(1600, 900, "Gravity");
    std::cout << "OpenGL version:" << glGetString(GL_VERSION) << "\n";

    auto args = parse_args(argc, argv);

    physics_gl pgl{args.count, args.dt};
    pgl.use_shader();
    pgl.bind();

    glPointSize(2);

    auto cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view;

    pgl.set_perspective(disp.aspect_ratio(), 0.1f, 100.0f);

    auto b = pgl.get_bodies();
    auto updatedPosition = false;
    auto running = true;
    std::thread physics_thread{&do_physics, b, args.dt, &updatedPosition, &running};
    auto counter = 0.0f;
    auto frames = 1;

    while (!disp.is_closed()) {
        auto start = std::chrono::high_resolution_clock::now();

        disp.clear(0.0f, 0.0f, 0.0f, 1.0f);
        if (disp.resized()) {
            pgl.set_perspective(disp.aspect_ratio(), 0.1f, 100.f);
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
        glDrawArraysInstanced(GL_POINTS, 0, 3 * sizeof(glm::vec3), args.count);

        disp.update();
        frames++;
        counter += args.camera_step;

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::this_thread::sleep_for(std::chrono::microseconds(16667 - elapsed_us));
    }

    running = false;
    physics_thread.join();

    return 0;
}
