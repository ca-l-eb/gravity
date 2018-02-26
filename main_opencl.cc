#include <glm/gtc/matrix_transform.hpp>

#include <math.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "physics_cl.h"
#include "physics_gl.h"
#include "simpleio.h"

struct arg_opt {
    std::string_view name;
    std::string_view description;
    int num_params;
};

bool operator<(const arg_opt &l, const arg_opt &r)
{
    return l.name < r.name;
}

struct parsed_opt {
    std::string value;
    bool empty;

    template<typename T>
    T get(T default_value)
    {
        if (empty)
            return default_value;
        return (T) value;
    }
};

template<>
int parsed_opt::get(int default_value)
{
    if (empty)
        return default_value;
    return std::stoi(value);
}

template<>
float parsed_opt::get(float default_value)
{
    if (empty)
        return default_value;
    return std::stof(value);
}

template<>
double parsed_opt::get(double default_value)
{
    if (empty)
        return default_value;
    return std::stod(value);
}

template<>
std::string parsed_opt::get(std::string default_value)
{
    if (empty)
        return default_value;
    return value;
}

template<>
bool parsed_opt::get(bool)
{
    return !empty;
}

class arg_parser
{
private:
    std::set<arg_opt> arg_set;
    std::map<arg_opt, parsed_opt> parsed_options;
    size_t max_name_length;

public:
    void add_arg(arg_opt option)
    {
        arg_set.insert(option);
        max_name_length = std::max(max_name_length, option.name.size());
    }

    void parse(int argc, char *argv[])
    {
        for (int i = 1; i < argc; i++) {
            std::string_view view{argv[i]};
            auto it = arg_set.find({view, 0});
            if (it != arg_set.end()) {
                int n = it->num_params;
                parsed_opt parsed;
                parsed.empty = false;
                if (n-- && i + 1 < argc) {
                    parsed.value = argv[++i];
                }
                parsed_options[*it] = parsed;
            }
        }
    }

    parsed_opt find(std::string_view param)
    {
        auto it = parsed_options.find({param, "", 0});
        if (it != parsed_options.end()) {
            return it->second;
        }
        return {"", true};
    }

    void show_help()
    {
        for (auto &option : arg_set) {
            std::cout << "  " << std::left << std::setw(max_name_length + 2) << option.name
                      << option.description << "\n";
        }
        std::cout << std::internal;
    }
};

struct program_args {
    int count;
    float dt;
    float camera_step;
    std::string prefered_platform;
};

static program_args parse_args(int argc, char *argv[])
{
    arg_parser parser;
    parser.add_arg({"-n", "number of objects", 1});
    parser.add_arg({"-p", "preferred OpenCL platform", 1});
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
    args.prefered_platform = parser.find("-p").get<std::string>("");

    return args;
}

int main(int argc, char *argv[])
{
    try {
        auto args = parse_args(argc, argv);
        std::cout << "n=" << args.count << " dt=" << args.dt << "\n";

        auto display = GLDisplay{1600, 900, "Gravity OpenCL"};
        if (display.wasError()) {
            return display.wasError();
        }
        std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";

        physics_gl pgl{args.count, args.dt};

        auto c = physics_cl{pgl, args.prefered_platform};
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
        glPointSize(2);

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
                // Else context is not OpenGL shared buffer, we need to read the data back, then
                // write it back to OpenGL to display the updated positions of the particles
                c.apply_gravity();
                c.update_positions();
                c.write_position_data();

                pgl.update_positions();
            }
            c.finish();

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
