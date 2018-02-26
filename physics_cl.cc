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
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "physics_cl.h"
#include "physics_gl.h"
#include "simpleio.h"

static bool check_error(cl_int err, const char *message)
{
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: " << message << " (" << err << ")" << std::endl;
        return true;
    }
    return false;
}

#define throw_error_info(err, message)                                                            \
    do {                                                                                          \
        if ((err) != CL_SUCCESS) {                                                                \
            std::stringstream ss;                                                                 \
            ss << "error at " << __FILE__ << ":" << __LINE__ << " " << (message) << " (" << (err) \
               << ')';                                                                            \
            throw std::runtime_error{ss.str()};                                                   \
        }                                                                                         \
    } while (0);

static std::string get_device_name(cl_device_id id)
{
    auto size = 0UL;
    clGetDeviceInfo(id, CL_DEVICE_NAME, 0, nullptr, &size);
    auto s = std::string(size + 1, '\0');
    clGetDeviceInfo(id, CL_DEVICE_NAME, size, const_cast<char *>(s.data()), nullptr);
    return s;
}

static cl_device_id get_best_device(cl_platform_id platform, const std::string &preferred_device)
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
        // If we get a matching device name, use that instead of max compute units
        auto device_name = get_device_name(device);
        if (!preferred_device.empty() && device_name.find(preferred_device) != std::string::npos)
            return device;
    }
    return best_device;
}

static std::string get_platform_name(cl_platform_id id)
{
    auto size = 0UL;
    clGetPlatformInfo(id, CL_PLATFORM_NAME, 0, nullptr, &size);
    auto s = std::string(size + 1, '\0');
    clGetPlatformInfo(id, CL_PLATFORM_NAME, size, const_cast<char *>(s.data()), nullptr);
    return s;
}

static std::string get_device_extensions(cl_device_id device)
{
    char buffer[1 << 13];
    auto ext_size = 0UL;
    auto err = clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof(buffer), buffer, &ext_size);
    if (err || ext_size == 0) {
        check_error(err, "clGetDeviceInfo extensions");
        return "";
    }
    buffer[ext_size] = '\0';
    return std::string{buffer, buffer + ext_size};
}

static bool is_extension_supported(const char *extension_name, cl_device_id id)
{
    auto extensions = get_device_extensions(id);
    if (extensions.find(extension_name, 0) != std::string::npos) {
        return true;
    }
    return false;
}

static int cl_gl_compatibility(cl_device_id id)
{
#ifdef __APPLE__
    static const char *CL_GL_SHARING_EXT = "cl_APPLE_gl_sharing";
#else
    static const char *CL_GL_SHARING_EXT = "cl_khr_gl_sharing";
#endif
    return is_extension_supported(CL_GL_SHARING_EXT, id);
}

static void check_build_errors(cl_int error, cl_program program, cl_device_id device)
{
    if (error) {
        auto len = 0UL;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &len);
        auto log = std::string(len, '\0');
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, len,
                              const_cast<char *>(log.c_str()), nullptr);
        std::cerr << "build error(" << error << "): " << log << "\n";
    }
}

static cl_program make_program(const char *kernel_source, cl_context context, cl_device_id device)
{
    auto error = 0;
    auto program = clCreateProgramWithSource(context, 1, &kernel_source, nullptr, nullptr);

    error = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);
    check_build_errors(error, program, device);

    return program;
}

static std::vector<cl_platform_id> get_platforms()
{
    auto platformIdCount = 0U;
    clGetPlatformIDs(0, nullptr, &platformIdCount);

    auto platforms = std::vector<cl_platform_id>(platformIdCount);
    clGetPlatformIDs(platformIdCount, platforms.data(), nullptr);

    return platforms;
}

static cl_command_queue get_command_queue(cl_context context, cl_device_id device)
{
    auto error = 0;
#ifdef __APPLE__
    auto ret = clCreateCommandQueue(context, device, 0, &error);
#else
    auto ret = clCreateCommandQueueWithProperties(context, device, nullptr, &error);
#endif
    check_error(error, "clCreateCommandQueueWithProperties");
    return ret;
}

// Help from: http://sa10.idav.ucdavis.edu/docs/sa10-dg-opencl-gl-interop.pdf
// Create CL context properties, add handle & share-group enum
static cl_context_properties *get_shared_gl_properties(cl_platform_id platform)
{
#ifdef __APPLE__
    static cl_context_properties properties[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties) CGLGetShareGroup(CGLGetCurrentContext()), 0};
#elif _WIN32
    static cl_context_properties properties[] = {CL_GL_CONTEXT_KHR,
                                                 (cl_context_properties) wglGetCurrentContext(),
                                                 CL_WGL_HDC_KHR,
                                                 (cl_context_properties) wglGetCurrentDC(),
                                                 CL_CONTEXT_PLATFORM,
                                                 (cl_context_properties) platform,
                                                 0};
#else
    static cl_context_properties properties[] = {CL_GL_CONTEXT_KHR,
                                                 (cl_context_properties) glXGetCurrentContext(),
                                                 CL_GLX_DISPLAY_KHR,
                                                 (cl_context_properties) glXGetCurrentDisplay(),
                                                 CL_CONTEXT_PLATFORM,
                                                 (cl_context_properties) platform,
                                                 0};
#endif
    return properties;
}

static cl_context get_shared_gl_context(cl_platform_id platform, cl_device_id *device,
                                        cl_int *error)
{
    auto properties = get_shared_gl_properties(platform);
    return clCreateContext(properties, 1, device, nullptr, nullptr, error);
}

static cl_context get_context(cl_device_id *device, cl_int *error)
{
    return clCreateContext(nullptr, 1, device, nullptr, nullptr, error);
}

physics_cl::physics_cl(physics_gl &p, const std::string &prefered_platform,
                       const std::string &preferred_device)
    : pgl{p}
{
    auto platforms = get_platforms();
    if (platforms.empty())
        throw std::runtime_error{"No OpenCL platforms found"};

    for (auto &platform : platforms) {
        auto platform_name = get_platform_name(platform);
        if (platform_name.find(prefered_platform) != std::string::npos) {
            this->platform = platform;
            break;
        }
    }

    // Otherwise just pick the first platform if we couldn't find preferred one
    if (!platform)
        platform = platforms[0];

    device = get_best_device(platform, preferred_device);

    std::cout << "using " << get_device_name(device) << '\n';

    auto error = 0;
    if (cl_gl_compatibility(device)) {
        context = get_shared_gl_context(platform, &device, &error);
        gl_context = !check_error(error, "failed to use shared OpenGL buffer");
    }

    if (!gl_context) {
        context = get_context(&device, &error);
        throw_error_info(error, "OpenCL context creation failed");
    }

    queue = get_command_queue(context, device);

    auto kernel_source = read_file("res/physics.cl");
    program = make_program(kernel_source.c_str(), context, device);

    apply_gravity_kernel = clCreateKernel(program, "apply_gravity", &error);
    throw_error_info(error, "apply_gravity kernel creation");
    update_kernel = clCreateKernel(program, "update_positions", &error);
    throw_error_info(error, "update_positions kernel creation");

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
        throw_error_info(error, "failed to get OpenGL shared memory object");
        std::cout << "using shared OpenGL buffer" << std::endl;
    } else {
        // Need to update these positions each frame if its not shared by OpenGL
        input_pos = clCreateBuffer(context, CL_MEM_READ_ONLY, vec_size, nullptr, &error);
        throw_error_info(error, "gpu memory allocation failed");
        clEnqueueWriteBuffer(queue, input_pos, CL_FALSE, 0, vec_size, pgl.bodies.pos.data(), 0,
                             nullptr, nullptr);
    }

    input_vel = clCreateBuffer(context, CL_MEM_READ_ONLY, vec_size, nullptr, &error);
    throw_error_info(error, "gpu memory allocation failed");
    input_acc = clCreateBuffer(context, CL_MEM_READ_ONLY, vec_size, nullptr, &error);
    throw_error_info(error, "gpu memory allocation failed");
    input_mass = clCreateBuffer(context, CL_MEM_READ_ONLY, size * sizeof(float), nullptr, &error);
    throw_error_info(error, "gpu memory allocation failed");
    input_dt = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float), nullptr, &error);
    throw_error_info(error, "gpu memory allocation failed");

    error = clEnqueueWriteBuffer(queue, input_vel, CL_FALSE, 0, vec_size, pgl.bodies.vel.data(), 0,
                                 nullptr, nullptr);
    throw_error_info(error, "failed to write to gpu memory");
    error = clEnqueueWriteBuffer(queue, input_acc, CL_FALSE, 0, vec_size, pgl.bodies.acc.data(), 0,
                                 nullptr, nullptr);
    throw_error_info(error, "failed to write to gpu memory");
    error = clEnqueueWriteBuffer(queue, input_mass, CL_FALSE, 0, size * sizeof(float),
                                 pgl.bodies.mass.data(), 0, nullptr, nullptr);
    throw_error_info(error, "failed to write to gpu memory");
    error = clEnqueueWriteBuffer(queue, input_dt, CL_FALSE, 0, sizeof(float), &pgl.step_dt, 0,
                                 nullptr, nullptr);
    throw_error_info(error, "failed to write to gpu memory");
    clFinish(queue);
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

        std::cout << "[" << i << "] " << get_platform_name(platform) << "\n";

        for (auto &device : devices) {
            auto j = i;
            clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf("  Device: %s\n", buffer);

            clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf("   %d Hardware version: %s\n", 1, buffer);

            clGetDeviceInfo(device, CL_DRIVER_VERSION, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf("   %d Software version: %s\n", 2, buffer);

            clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, sizeof(buffer), buffer, &size);
            buffer[size] = '\0';
            printf("   %d OpenCL C version: %s\n", 3, buffer);

            auto compute_units = 0;
            clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units),
                            &compute_units, nullptr);
            printf("   %d Parallel compute units: %d\n", 4, compute_units);
            j++;
        }
        i++;
    }
}

void physics_cl::acquire_gl_object()
{
    glFlush();
    auto err = clEnqueueAcquireGLObjects(queue, 1, &input_pos, 0, nullptr, nullptr);
    throw_error_info(err, "clEnqueueAcquireGLObjects");
}

void physics_cl::release_gl_object()
{
    auto err = clEnqueueReleaseGLObjects(queue, 1, &input_pos, 0, nullptr, nullptr);
    throw_error_info(err, "releasing GL objects");
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
