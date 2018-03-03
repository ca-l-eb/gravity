#include <glm/gtc/matrix_transform.hpp>

#include <math.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "args.h"
#include "physics_cl.h"
#include "physics_gl.h"
#include "simpleio.h"

struct program_args {
    int count;
    float dt;
    float camera_step;
    std::string preferred_platform;
    std::string preferred_device;
    int point_size;
};

static program_args parse_args(int argc, char *argv[])
{
    arg_parser parser{"gravity_cl"};
    parser.add_arg({"-n", "number of objects", 1});
    parser.add_arg({"-p", "preferred OpenCL platform", 1});
    parser.add_arg({"-d", "preferred OpenCL device", 1});
    parser.add_arg({"-dt", "time step", 1});
    parser.add_arg({"-rot", "camera rotation speed", 1});
    parser.add_arg({"-h", "help", 0});
    parser.add_arg({"-ps", "particle point size", 1});

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
    args.preferred_platform = parser.find("-p").get<std::string>("");
    args.preferred_device = parser.find("-d").get<std::string>("");
    args.point_size = parser.find("-ps").get(1);

    return args;
}

int main(int argc, char *argv[])
{
    try {
        auto args = parse_args(argc, argv);
        std::cout << "n=" << args.count << " dt=" << args.dt << "\n";

        auto display = GLDisplay{1600, 900, "Gravity OpenCL"};
        std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";

        auto pgl = physics_gl{args.count, args.dt};
        auto pcl = physics_cl{pgl, args.preferred_platform, args.preferred_device};
        pcl.print_platform_info();

        // Bind shader and use VAO so OpenGL draws correctly
        pgl.use_shader();
        pgl.bind();

        auto camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
        auto up = glm::vec3(0.0f, 1.0f, 0.0f);
        auto counter = 0.0f;

        // Set the camera transformation, and send it to OpenGL
        auto view =
            glm::lookAt(glm::vec3(2 * sin(counter), 1.1f * sin(1.3 * counter) * cos(.33f * counter),
                                  2 * cos(counter)),
                        camera_target, up);
        pgl.set_view(view);
        pgl.set_perspective(display.aspect_ratio(), 0.1f, 100.0f);

        glEnable(GL_DEPTH_TEST);
        glPointSize(args.point_size);

        while (!display.is_closed()) {
            display.clear(0.0f, 0.0f, 0.0f, 1.0f);
            if (display.resized()) {
                pgl.set_perspective(display.aspect_ratio(), 0.1f, 100.0f);
                glViewport(0, 0, display.width(), display.height());
            }
            if (pcl.is_gl_context()) {
                pcl.acquire_gl_object();

                // Update the positions while OpenCL has acquired the OpenGL buffers
                pcl.apply_gravity();
                pcl.update_positions();

                pcl.release_gl_object();
            } else {
                // Else context is not OpenGL shared buffer, we need to read the data back, then
                // write it back to OpenGL to display the updated positions of the particles
                pcl.apply_gravity();
                pcl.update_positions();
                pcl.write_position_data();

                pgl.update_positions();
            }
            pcl.finish();

            // Update the camera
            counter += args.camera_step;
            view = glm::lookAt(
                glm::vec3(2 * sin(counter), 1.1f * sin(1.3 * counter) * cos(.33f * counter),
                          2 * cos(counter)),
                camera_target, up);
            pgl.set_view(view);

            // Finally, draw the particles to the screen, and update
            glDrawArraysInstanced(GL_POINTS, 0, 3 * sizeof(glm::vec3), args.count);
            display.update();
        }
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
    }
    return 0;
}
