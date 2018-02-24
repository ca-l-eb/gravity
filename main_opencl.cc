#include <glm/gtc/matrix_transform.hpp>

#include <math.h>
#include <string.h>
#include <iostream>
#include <utility>
#include <vector>

#include "physics_cl.h"
#include "physics_gl.h"
#include "simpleio.h"

static void handle_args(int argc, char *argv[], int &count, float &dt, float &step)
{
    if (argc > 1) {
        std::string fs = std::string(argv[1]);
        count = std::stof(fs);
    }
    if (argc > 2) {
        std::string fs = std::string(argv[2]);
        dt = std::stof(fs);
    }
    if (argc > 3) {
        std::string fs = std::string(argv[3]);
        step = std::stof(fs) / 300.0f;
    }
}

int main(int argc, char *argv[])
{
    auto display = GLDisplay{1600, 900, "Gravity OpenCL"};
    if (display.wasError()) {
        return display.wasError();
    }
    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    auto count = 1 << 12;
    auto dt = 0.0005f;
    auto step = static_cast<float>(M_PI) / (360.0f * 15.0f);
    handle_args(argc, argv, count, dt, step);

    physics_gl pgl{count, dt};

    auto c = physics_cl{pgl};
    c.print_platform_info();

    // Bind shader and use VAO so OpenGL draws correctly
    pgl.use_shader();
    pgl.bind();

    auto aspect_ratio = static_cast<float>(display.getWidth()) / display.getHeight();
    pgl.set_perspective(aspect_ratio, 0.1f, 100.0f);

    auto camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
    auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    auto counter = 0.0f;

    // Set the camera transformation, and send it to OpenGL
    auto view =
        glm::lookAt(glm::vec3(2 * sin(counter), 1.1f * sin(1.3 * counter) * cos(.33f * counter),
                              2 * cos(counter)),
                    camera_target, up);
    pgl.set_view(view);
    pgl.set_perspective(aspect_ratio, 0.1f, 100.0f);

    glEnable(GL_DEPTH_TEST);
    glPointSize(1);

    while (!display.isClosed()) {
        display.clear(0.0f, 0.0f, 0.0f, 1.0f);
        if (display.wasResized()) {
            aspect_ratio = static_cast<float>(display.getWidth()) / display.getHeight();
            pgl.set_perspective(aspect_ratio, 0.1f, 100.0f);
            glViewport(0, 0, display.getWidth(), display.getHeight());
        }
        if (c.is_gl_context()) {
            c.acquire_gl_object();

            // Update the positions while OpenCL has acquired the OpenGL buffers
            c.apply_gravity();
            c.update_positions();

            c.release_gl_object();
        } else {
            // Else context is not OpenGL shared buffer, we need to read the data back, then write
            // it back to OpenGL to display the updated positions of the particles
            c.apply_gravity();
            c.update_positions();
            c.write_position_data();

            pgl.update_positions();
        }
        c.finish();

        // Update the camera
        counter += step;
        view =
            glm::lookAt(glm::vec3(2 * sin(counter), 1.1f * sin(1.3 * counter) * cos(.33f * counter),
                                  2 * cos(counter)),
                        camera_target, up);
        pgl.set_view(view);

        // Finally, draw the particles to the screen, and update
        glDrawArraysInstanced(GL_POINTS, 0, 3 * sizeof(glm::vec3), count);
        display.update();
    }
    return 0;
}
