#include <GL/glew.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#include <OpenCL/cl_ext.h>
#include <OpenCL/cl_gl.h>
#include <OpenCL/cl_gl_ext.h>
#include <OpenCL/opencl.h>
#include <OpenGL/CGLContext.h>
#include <OpenGL/OpenGL.h>
#else
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <CL/cl_gl_ext.h>
#ifdef _WIN32
#include <GL/wglew.h>
#else
#include <GL/glext.h>
#include <GL/glx.h>
#endif
#endif

#include <glm/gtc/matrix_transform.hpp>

#include <math.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#include "physics_cl.h"
#include "physics_gl.h"
#include "simpleio.h"

static int check_error(cl_int err, const char *message)
{
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: " << message << " (" << err << ")" << std::endl;
        return 1;
    }
    return 0;
}

physics_cl::physics_cl(physics_gl &p) : pgl{p}
{
    platform = get_platform();
    device = get_best_device();

    std::cout << "Best device: ";
    print_device_name(device);

    context = get_context();
    queue = get_command_queue();

    auto kernel_source = read_file("res/physics.cl");
    program = make_program(kernel_source.c_str());

    auto error = 0;
    apply_gravity_kernel = clCreateKernel(program, "apply_gravity", &error);
    check_error(error, "apply_gravity kernel creation");
    update_kernel = clCreateKernel(program, "update_positions", &error);
    check_error(error, "update_positions kernel creation");

    make_buffers();

    global_dimensions[0] = p.num_particles;
    global_dimensions[1] = 0;
    global_dimensions[2] = 0;
}

physics_cl::~physics_cl()
{
    clReleaseMemObject(input_pos);
    clReleaseMemObject(input_vel);
    clReleaseMemObject(input_acc);
    clReleaseMemObject(input_mass);
    clReleaseProgram(program);
    clReleaseKernel(apply_gravity_kernel);
    clReleaseKernel(update_kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
};

void physics_cl::make_buffers()
{
    auto size = pgl.num_particles;
    auto error = 0;
    auto vec_size = sizeof(glm::vec3) * size;
    // Map the OpenGL VBO memory to this OpenCL context if it is a GL context
    if (gl_context) {
        input_pos = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, pgl.positions_vbo, &error);
        std::cout << "Using OpenGL buffer" << std::endl;
        check_error(error, "clCreateFromGLBuffer");
    } else {
        // Need to update these positions each frame if its not shared by OpenGL
        input_pos = clCreateBuffer(context, CL_MEM_READ_ONLY, vec_size, nullptr, &error);
        check_error(error, "clCreateBuffer");
        clEnqueueWriteBuffer(queue, input_pos, CL_FALSE, 0, vec_size, pgl.bodies.pos.data(), 0,
                             nullptr, nullptr);
    }

    input_vel = clCreateBuffer(context, CL_MEM_READ_ONLY, vec_size, nullptr, &error);
    check_error(error, "clCreateBuffers input_vel");
    input_acc = clCreateBuffer(context, CL_MEM_READ_ONLY, vec_size, nullptr, &error);
    check_error(error, "clCreateBuffers input_acc");
    input_mass = clCreateBuffer(context, CL_MEM_READ_ONLY, size * sizeof(float), nullptr, &error);
    check_error(error, "clCreateBuffers input_mass");
    input_dt = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float), nullptr, &error);
    check_error(error, "clCreateBuffers input_dt");

    error = clEnqueueWriteBuffer(queue, input_vel, CL_FALSE, 0, vec_size, pgl.bodies.vel.data(), 0,
                                 nullptr, nullptr);
    check_error(error, "clEnqueueWriteBuffer input_vel");
    error = clEnqueueWriteBuffer(queue, input_acc, CL_FALSE, 0, vec_size, pgl.bodies.acc.data(), 0,
                                 nullptr, nullptr);
    check_error(error, "clEnqueueWriteBuffer input_acc");
    error = clEnqueueWriteBuffer(queue, input_mass, CL_FALSE, 0, size * sizeof(float),
                                 pgl.bodies.mass.data(), 0, nullptr, nullptr);
    check_error(error, "clEnqueueWriteBuffer input_mass");
    error = clEnqueueWriteBuffer(queue, input_dt, CL_FALSE, 0, sizeof(float), &pgl.step_dt, 0,
                                 nullptr, nullptr);
    check_error(error, "clEnqueueWriteBuffer input_dt");
    clFinish(queue);
}

cl_platform_id physics_cl::get_platform()
{
    auto platformIdCount = 0U;
    clGetPlatformIDs(0, nullptr, &platformIdCount);

    auto platformIds = std::vector<cl_platform_id>(platformIdCount);
    clGetPlatformIDs(platformIdCount, platformIds.data(), nullptr);

    std::cout << "Found " << platformIdCount << " platforms: " << std::endl;
    for (auto i = 0U; i < platformIdCount; i++) {
        print_platform_name(platformIds[i]);
    }

    return platformIds[0];
}

// Help from: http://sa10.idav.ucdavis.edu/docs/sa10-dg-opencl-gl-interop.pdf
// Create CL context properties, add handle & share-group enum
static cl_context_properties *get_shared_gl_properties(cl_platform_id platform)
{
#ifdef __APPLE__
    // Get current CGL Context and CGL Share group
    auto kCGLContext = CGLGetCurrentContext();
    auto kCGLShareGroup = CGLGetShareGroup(kCGLContext);
    static cl_context_properties properties[] = {CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
                                                 (cl_context_properties) kCGLShareGroup, 0};
#elif _WIN32
    static cl_context_properties properties[] = {
        CL_GL_CONTEXT_KHR,
        (cl_context_properties) wglGetCurrentContext(),  // WGL Context
        CL_WGL_HDC_KHR,
        (cl_context_properties) wglGetCurrentDC(),  // WGL HDC
        CL_CONTEXT_PLATFORM,
        (cl_context_properties) platform,
        0};
#else
    static cl_context_properties properties[] = {
        CL_GL_CONTEXT_KHR,
        (cl_context_properties) glXGetCurrentContext(),  // GLX Context
        CL_GLX_DISPLAY_KHR,
        (cl_context_properties) glXGetCurrentDisplay(),  // GLX GLDisplay
        CL_CONTEXT_PLATFORM,
        (cl_context_properties) platform,  // OpenCL platform
        0};
#endif
    return properties;
}

cl_context physics_cl::get_context()
{
    auto properties = get_shared_gl_properties(platform);
    auto error = 0;
    auto context = clCreateContext(properties, 1, &device, nullptr, nullptr, &error);
    if (!check_error(error, "clCreateContext OpenGL")) {
        gl_context = true;
        return context;
    } else {
        context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &error);
        check_error(error, "clCreateContext");
        return context;
    }
}

cl_command_queue physics_cl::get_command_queue()
{
    auto error = 0;
    auto ret = clCreateCommandQueueWithProperties(context, device, nullptr, &error);
    if (error)
        check_error(error, "clCreateCommandQueueWithProperties");
    return ret;
}

cl_program physics_cl::make_program(const char *kernel_source)
{
    auto error = 0;
    auto program = clCreateProgramWithSource(context, 1, &kernel_source, nullptr, nullptr);

    error = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);
    check_build_errors(error, program, device);

    return program;
}

cl_device_id physics_cl::get_best_device()
{
    auto max_compute_units = 0L;
    cl_device_id best_device;

    auto device_count = 0U;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &device_count);

    std::vector<cl_device_id> devices(device_count);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, device_count, devices.data(), nullptr);

    // Choose best device, i.e. most compute units
    for (auto &device : devices) {
        auto check_compute_units = 0U;
        clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(check_compute_units),
                        &check_compute_units, nullptr);
        if (check_compute_units > max_compute_units) {
            max_compute_units = check_compute_units;
            best_device = device;
        }
    }
    return best_device;
}

std::string physics_cl::get_device_name(cl_device_id id)
{
    auto size = 0UL;
    clGetDeviceInfo(id, CL_DEVICE_NAME, 0, nullptr, &size);
    auto s = std::string(size + 1, '\0');
    clGetDeviceInfo(id, CL_DEVICE_NAME, size, const_cast<char *>(s.data()), nullptr);
    return s;
}

std::string physics_cl::get_platform_name(cl_platform_id id)
{
    auto size = 0UL;
    clGetPlatformInfo(id, CL_PLATFORM_NAME, 0, nullptr, &size);
    auto s = std::string(size + 1, '\0');
    clGetPlatformInfo(id, CL_PLATFORM_NAME, size, const_cast<char *>(s.data()), nullptr);
    return s;
}

std::string physics_cl::get_device_extensions(cl_device_id id)
{
    auto ext_size = 1024UL;
    auto str = std::string(ext_size, '\0');
    auto err = clGetDeviceInfo(id, CL_DEVICE_EXTENSIONS, ext_size, &str, &ext_size);
    if (err || ext_size == 0) {
        check_error(err, "clGetDeviceInfo extensions");
        return "";
    }
    return str;
}

bool physics_cl::is_extension_supported(const char *extension_name, cl_device_id id)
{
    auto extensions = get_device_extensions(id);
    if (extensions.find(extension_name, 0) != std::string::npos) {
        return true;
    }
    return false;
}

int physics_cl::cl_gl_compatibility(cl_device_id id)
{
    if (is_extension_supported(CL_GL_SHARING_EXT, id)) {
        std::cout << get_device_name(id) << " supports OpenGL interoperability" << std::endl;
        return 1;
    }
    return 0;
}

void physics_cl::print_device_name(cl_device_id id)
{
    std::cout << get_device_name(id) << std::endl;
}

void physics_cl::print_platform_name(cl_platform_id id)
{
    std::cout << get_platform_name(id) << std::endl;
}

void physics_cl::check_build_errors(cl_int error, cl_program program, cl_device_id deviceID)
{
    if (error) {
        printf("Error = %d\n", error);
        auto len = 0UL;
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, 0, nullptr, &len);
        auto log = std::string(len, '\0');
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, len,
                              const_cast<char *>(log.c_str()), nullptr);
        std::cerr << log << '\n';
    }
}

void physics_cl::apply_gravity()
{
    clSetKernelArg(apply_gravity_kernel, 0, sizeof(input_pos), &input_pos);
    clSetKernelArg(apply_gravity_kernel, 1, sizeof(input_vel), &input_vel);
    clSetKernelArg(apply_gravity_kernel, 2, sizeof(input_acc), &input_acc);
    clSetKernelArg(apply_gravity_kernel, 3, sizeof(input_mass), &input_mass);

    // Enqueue our problem to actually be executed by the device
    clEnqueueNDRangeKernel(queue, apply_gravity_kernel, 1, nullptr, global_dimensions, nullptr, 0,
                           nullptr, nullptr);
    clFinish(queue);
}

void physics_cl::update_positions()
{
    clSetKernelArg(update_kernel, 0, sizeof(float *), &input_pos);
    clSetKernelArg(update_kernel, 1, sizeof(float *), &input_vel);
    clSetKernelArg(update_kernel, 2, sizeof(float *), &input_acc);
    clSetKernelArg(update_kernel, 3, sizeof(float *), &input_dt);

    // Enqueue our problem to actually be executed by the device
    clEnqueueNDRangeKernel(queue, update_kernel, 1, nullptr, global_dimensions, nullptr, 0, nullptr,
                           nullptr);
    clFinish(queue);
}

// http://dhruba.name/2012/08/14/opencl-cookbook-listing-all-devices-and-their-critical-attributes/
void physics_cl::print_platform_info()
{
    char buffer[2048];
    auto size = 0UL;

    auto platformCount = 0U;
    clGetPlatformIDs(0, nullptr, &platformCount);
    auto platformIds = std::vector<cl_platform_id>(platformCount);
    clGetPlatformIDs(platformCount, platformIds.data(), nullptr);

    auto i = 1;
    for (auto &platform : platformIds) {
        auto device_count = 0U;
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &device_count);

        auto devices = std::vector<cl_device_id>(device_count);
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, device_count, devices.data(), nullptr);

        for (auto &device : devices) {
            auto j = i;
            clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf("%d. Device: %s\n", j, buffer);

            clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf(" %d.%d Hardware version: %s\n", j, 1, buffer);

            clGetDeviceInfo(device, CL_DRIVER_VERSION, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf(" %d.%d Software version: %s\n", j, 2, buffer);

            clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf(" %d.%d OpenCL C version: %s\n", j, 3, buffer);

            auto compute_units = 0;
            clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units),
                            &compute_units, nullptr);
            printf(" %d.%d Parallel compute units: %d\n", j, 4, compute_units);
            j++;
        }
        i++;
    }
}

void physics_cl::acquire_gl_object()
{
    glFlush();
    auto err = clEnqueueAcquireGLObjects(queue, 1, &input_pos, 0, nullptr, nullptr);
    check_error(err, "clEnqueueAcquireGLObjects");
}

void physics_cl::release_gl_object()
{
    auto err = clEnqueueReleaseGLObjects(queue, 1, &input_pos, 0, nullptr, nullptr);
    check_error(err, "releasing GL objects");
}

void physics_cl::finish()
{
    clFinish(queue);
}

void physics_cl::write_position_data()
{
    auto bytes = pgl.num_particles * sizeof(glm::vec3);
    auto data = pgl.bodies.pos.data();
    clEnqueueReadBuffer(queue, input_pos, CL_TRUE, 0, bytes, data, 0, nullptr, nullptr);
}

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
    auto step = float(M_PI / 300.0f) / 15.0f;
    handle_args(argc, argv, count, dt, step);

    physics_gl pgl{count, dt};

    auto c = physics_cl{pgl};
    c.print_platform_info();

    // Bind shader and use VAO so OpenGL draws correctly
    pgl.use_shader();
    pgl.bind();

    auto aspect_ratio = (float) display.getWidth() / display.getHeight();
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
            aspect_ratio = (float) display.getWidth() / display.getHeight();
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
