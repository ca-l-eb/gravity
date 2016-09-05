#include <utility>

#ifdef __APPLE__
    #include <OpenCL/cl.h>
    #include <OpenCL/cl_gl.h>
    #include <OpenCL/cl_gl_ext.h>
    #include <OpenCL/cl_ext.h>
#include <OpenCL/opencl.h>

    #include <OpenGL/CGLContext.h>
    #include <OpenGL/OpenGL.h>
    #include <OpenGL/CGLDevice.h>
#else
    #include <CL/cl.h>
    #ifdef __WIN32__
    #else
        #include <GL/glx.h>
        #include <CL/cl_gl.h>
    #include <CL/cl_gl_ext.h>
    #include <CL/opencl.h>
    #include <GL/glext.h>
    #endif
#endif

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>
#include <functional>
#include <ctime>
#include <cmath>
#include <string.h>

#include "simpleio.h"
#include "opencl.h"
#include "display.h"



CL_Helper::CL_Helper() {
    platform = get_platform();
    gpu_device = get_devices().at(1);
    context = get_context();
    queue = get_command_queue();

    char *kernel_source = read_file("./res/physics.cl");
    program = make_program(kernel_source);

    cl_int error = 0;
    apply_gravity_kernel = clCreateKernel(program, "apply_gravity", &error);
    check_error(error, "apply_gravity kernel creation");
    update_kernel = clCreateKernel(program, "update_positions", &error);
    check_error(error, "update_positions kernel creation");

    // Map the OpenGL VBO memory to this OpenCL context if it is a GL context
    if (is_gl_context) {
//        input_pos = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, pos_vbo, &error);
//        input_vel = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, vel_vbo, &error);
//        input_acc = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, acc_vbo, &error);
//        input_mass = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, mass_vbo, &error);
    }
}

CL_Helper::~CL_Helper() {
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

static int setup_OpenCL() {

    std::clock_t start;
    start = std::clock();

    const int num_elements = 1 << 25;
    float *buffer = new float[num_elements];
    const int buffer_size = sizeof(float) * num_elements;
    for (int i = 0; i < num_elements; i++) {
        buffer[i] = 1.0f / std::sqrt(i + 1);
    }

    double duration = (std::clock() - start) / (double) CLOCKS_PER_SEC;
    std::cout << "Took " << duration << " seconds to run on CPU" << std::endl;

    for (int i = 0; i < num_elements; i++) {
        buffer[i] = float(i);
    }
    cl_int error = 0;
    cl_uint num_devices = 0, check = 0;
    cl_device_id devices[2]; // 0 - CPU, 1 - GPU



    int which_device = 1; // Which device do we want to use, GPU

    // Create the context for that device
    cl_context context = clCreateContext(0, 1, &devices[which_device], NULL, NULL, &error);

    if (error != CL_SUCCESS) {
        printf("Error creating OpenCL context: %d\n", error);
        return error;
    }

    // Create a command queue for our device
    cl_command_queue queue = clCreateCommandQueue(context, devices[which_device], (cl_command_queue_properties) 0,
                                                  NULL);

    // Read kernel source, load it, and build kernel
    char *source = read_file("./res/double.cl");
    cl_program program = clCreateProgramWithSource(context, 1, (const char **) &source, NULL, NULL);

    // Compile the kernel with all our devices
    clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

    // Check errors building the kernel
//    check_build_errors(error, program, devices[which_device]);

    cl_kernel double_kernel = clCreateKernel(program, "vec_double", &error);

    start = std::clock();

    cl_mem input_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, buffer_size, NULL, NULL);
    cl_mem output_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, buffer_size, NULL, NULL);

    // Write our data to the input buffer for the device
    clEnqueueWriteBuffer(queue, input_buffer, CL_TRUE, 0, buffer_size, buffer, 0, NULL, NULL);

    clSetKernelArg(double_kernel, 0, sizeof(float *), &input_buffer);
    clSetKernelArg(double_kernel, 1, sizeof(float *), &output_buffer);

    size_t global_dimensions[] = {num_elements, 1, 1};

    // Enqueue our problem to actually be executed by the device
    clEnqueueNDRangeKernel(queue, double_kernel, 1, NULL, global_dimensions, NULL, 0, NULL, NULL);
//    check_error(error, "clEnqueueNDRangeKernel");

    // Wait for the queue to finish
    clFinish(queue);

    duration = (std::clock() - start) / (double) CLOCKS_PER_SEC;
    std::cout << "Took " << duration << " seconds to run on GPU" << std::endl;

    // Now read data back to the original buffer
    clEnqueueReadBuffer(queue, output_buffer, CL_TRUE, 0, buffer_size, buffer, 0, NULL, NULL);

    duration = ((std::clock() - start) / (double) CLOCKS_PER_SEC) - duration;
    std::cout << "Took " << duration << " seconds to move data back to CPU" << std::endl;


//    for (int i = 0; i < num_elements; i++) {
//        printf("data[%d] = %.16f\n", i, buffer[i]);
//    }

    delete[] buffer;


    return 0;
}

cl_platform_id CL_Helper::get_platform() {
    cl_uint platformIdCount = 0;
    clGetPlatformIDs (0, nullptr, &platformIdCount);

    std::vector<cl_platform_id> platformIds (platformIdCount);
    clGetPlatformIDs (platformIdCount, platformIds.data (), nullptr);
    std::cout << "Found " << platformIdCount << " platforms: " << std::endl;
    for (int i = 0; i < platformIdCount; i++) {
        print_platform_name(platformIds[i]);
    }
    return platformIds[0];
}

cl_context CL_Helper::get_context() {
// Help from: http://sa10.idav.ucdavis.edu/docs/sa10-dg-opencl-gl-interop.pdf
// Create CL context properties, add handle & share-group enum
#ifdef __APPLE__
    // Get current CGL Context and CGL Share group
    CGLContextObj kCGLContext = CGLGetCurrentContext();
    CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
    cl_context_properties properties[] = { CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
                                           (cl_context_properties) kCGLShareGroup, 0 };
#elif __WIN32__
    cl_context_properties properties[] = {
                                           CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(), // WGL Context
                                           CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(), // WGL HDC
                                           CL_CONTEXT_PLATFORM, (cl_context_properties) platform, 0 };
#else
    cl_context_properties properties[] = {
            CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(), // GLX Context
            CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(), // GLX GLDisplay
            CL_CONTEXT_PLATFORM, (cl_context_properties) platform, // OpenCL platform
            0
    };
#endif
    cl_context context = 0;
    size_t size = 0;
    cl_int error = 0;
#ifdef __APPLE__
    context = clCreateContext(properties, 1, &gpu_device, NULL, NULL, &error);
    if (!error) {
        is_gl_context = true;
        return context;
    }
    else {
        context = clCreateContext(0, 1, &gpu_device, NULL, NULL, &error);
        check_error(error, "clCreateContext normal context");
        return context;
    }
#else
//    error = clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR,
//                                         sizeof(cl_device_id), &gpu_device, &size);
    size_t bytes = 0;

    clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 0, NULL, &bytes);
    size_t devNum = bytes/sizeof(cl_device_id);
    std::vector<cl_device_id> devs (devNum);
    clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, bytes, &devs[0], NULL);
    for (int i = 0; i < devNum; i++) {
        print_device_name(devs[i]);
    }

#endif
    if (!error) {
        // Create an OpenGL/OpenCL context for this GPU device
        context = clCreateContext(properties, 1, &gpu_device, NULL, NULL, &error);
        if (check_error(error, "clCreateContext OpenGL")) {
            std::cerr << "Could not create OpenGL context for OpenCL" << std::endl;
        }
        else {
            return context;
        }
    }
    context = clCreateContext(0, 1, &gpu_device, NULL, NULL, &error);
    check_error(error, "clCreateContext normal context");
    return context;
}

cl_command_queue CL_Helper::get_command_queue() {
    // Create a command queue for our device
    return clCreateCommandQueue(context, gpu_device, (cl_command_queue_properties) 0, NULL);
}

cl_program CL_Helper::make_program(char* kernel_source) {
    cl_int error = 0;
    cl_program program = clCreateProgramWithSource(context, 1, (const char **) &kernel_source, NULL, NULL);

    // Compile the kernel with all our devices
    error = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

    // Check errors building the kernel
    check_build_errors(error, program, gpu_device);
    return program;
}




std::vector<cl_device_id> CL_Helper::get_devices() {
    // Get CPU devices
    cl_uint check = 0;
    cl_device_id devices[2];
    cl_int error = clGetDeviceIDs(NULL, CL_DEVICE_TYPE_CPU, 1, &devices[0], &check);
    check_error(error, "clGetDevices CPU");
    if (check && !error) {
        num_devices++;
        print_device_name(devices[0]);
    }

    // Get GPU devices
    error = clGetDeviceIDs(NULL, CL_DEVICE_TYPE_GPU, 1, &devices[1], &check);
    check_error(error, "clGetDevices GPU");
    if (check && !error) {
        num_devices++;
        print_device_name(devices[1]);
    }

    std::vector<cl_device_id> vector_devices;
    vector_devices.push_back(devices[0]);
    vector_devices.push_back(devices[1]);
    return vector_devices;
}


cl_int CL_Helper::check_error(cl_int err, const char *message) {
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: " << message << " (" << err << ")" << std::endl;
    }
    return err;
}

std::string CL_Helper::get_device_name(cl_device_id id) {
    size_t size = 0;
    clGetDeviceInfo(id, CL_DEVICE_NAME, 0, nullptr, &size);
    std::string result(size + 1, '\0');
    clGetDeviceInfo(id, CL_DEVICE_NAME, size, const_cast<char *> (result.data()), nullptr);
    return result;
}

std::string CL_Helper::get_platform_name(cl_platform_id id) {
    size_t size = 0;
    clGetPlatformInfo(id, CL_PLATFORM_NAME, 0, nullptr, &size);
    std::string result(size + 1, '\0');
    clGetPlatformInfo(id, CL_PLATFORM_NAME, size, const_cast<char *> (result.data()), nullptr);
    return result;
}

std::string CL_Helper::get_device_extensions(cl_device_id id) {
    size_t ext_size = 1024;
    std::string str (1024, '\0');
    // Get string containing supported device extensions
    cl_int err = clGetDeviceInfo(id, CL_DEVICE_EXTENSIONS, ext_size, &str, &ext_size);
    if (err || ext_size == 0) {
        check_error(err, "clGetDeviceInfo extensions");
        return "";
    }
    return str;
}

int CL_Helper::is_extension_supported(const char* extension_name, cl_device_id id) {
    std::string extensions = get_device_extensions(id);
    if (extensions.find(extension_name, 0) != std::string::npos) {
        return 1;
    }
    return 0;
}

int CL_Helper::cl_gl_compatibility(cl_device_id id) {
    if (is_extension_supported(CL_GL_SHARING_EXT, id)) {
        std::cout << get_device_name(id) << " supports OpenGL interoperability" << std::endl;
        return 1;
    }
    return 0;
}

void CL_Helper::print_device_name(cl_device_id id) {
    std::string name = get_device_name(id);
    std::cout << name << std::endl;
}

void CL_Helper::print_platform_name(cl_platform_id id) {
    std::string name = get_platform_name(id);
    std::cout << name << std::endl;
}

void CL_Helper::check_build_errors(cl_int error, cl_program program, cl_device_id deviceID) {
    if (error) {
        printf("Error = %d\n", error);
        size_t len = 0;
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
        char *log = new char[len];
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, len, log, NULL);
        std::cerr << log << std::endl;
        delete[] log;
    }
}

int main(int argc, char *argv[]) {
    GLDisplay d(640, 480, "Gravity-CL");
    CL_Helper c;
//    setup_OpenCL();
    return 0;
}
