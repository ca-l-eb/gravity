//
// Created by dechant on 9/4/16.
//

#ifndef GRAVITY_OPENCL_H
#define GRAVITY_OPENCL_H

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <string>

#include "physics_gl.h"

class physics_cl
{
public:
    physics_cl(physics_gl &p);
    ~physics_cl();

    inline bool is_gl_context()
    {
        return gl_context;
    }

    void apply_gravity();
    void update_positions();
    void write_position_data();
    void finish();
    void acquire_gl_object();
    void release_gl_object();
    void print_platform_info();

    friend class physics_gl;

private:
    cl_platform_id platform;
    cl_context context;
    cl_command_queue queue;
    cl_device_id device;
    cl_program program;
    cl_mem input_pos, input_vel, input_acc, input_mass, input_dt;
    cl_kernel apply_gravity_kernel, update_kernel;
    size_t global_dimensions[3];
    bool gl_context;
    physics_gl &pgl;

    void print_device_name(cl_device_id id);
    void print_platform_name(cl_platform_id id);
    void check_build_errors(cl_int error, cl_program program, cl_device_id deviceID);
    void make_buffers();

    cl_device_id get_best_device();
    cl_platform_id get_platform();
    cl_context get_context();
    cl_command_queue get_command_queue();
    cl_program make_program(const char *kernel_source);
    std::string get_device_name(cl_device_id id);
    std::string get_platform_name(cl_platform_id id);
    std::string get_device_extensions(cl_device_id id);
    bool is_extension_supported(const char *support_str, cl_device_id id);
    int cl_gl_compatibility(cl_device_id id);

#ifdef __APPLE__
    const char *CL_GL_SHARING_EXT = "cl_APPLE_gl_sharing";
#else
    const char *CL_GL_SHARING_EXT = "cl_khr_gl_sharing";
#endif
};

#endif  // GRAVITY_OPENCL_H
