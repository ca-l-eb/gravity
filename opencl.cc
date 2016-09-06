#include <utility>
#include <GL/glew.h>
#ifdef __APPLE__
    #include <OpenCL/cl.h>
    #include <OpenCL/cl_gl.h>
    #include <OpenCL/cl_gl_ext.h>
    #include <OpenCL/cl_ext.h>
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
		#include <GL/glx.h>
		#include <GL/glext.h>
    #endif
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <math.h>
#include <string.h>
#include <ctime>

#include "simpleio.h"
#include "opencl.h"
#include "physics_gl.h"


CL_Helper::CL_Helper(physics_gl* p) {
    pgl = p;
    platform = get_platform();
	gpu_device = get_best_device();

    std::cout << "Best device: ";
    print_device_name(gpu_device);

    context = get_context();
    queue = get_command_queue();

    char *kernel_source = read_file("./res/physics.cl");
    program = make_program(kernel_source);

    cl_int error = 0;
    apply_gravity_kernel = clCreateKernel(program, "apply_gravity", &error);
    check_error(error, "apply_gravity kernel creation");
    update_kernel = clCreateKernel(program, "update_positions", &error);
    check_error(error, "update_positions kernel creation");

    make_buffers();

    global_dimensions[0] = p->num_particles;
    global_dimensions[1] = 1;
    global_dimensions[2] = 1;

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


void CL_Helper::make_buffers() {
    int size = pgl->num_particles;
    cl_int error = 0;
    // Map the OpenGL VBO memory to this OpenCL context if it is a GL context
    if (gl_context) {
        input_pos = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, pgl->positions_vbo, &error);
        std::cout << "Using OpenGL buffer" << std::endl;
        check_error(error, "clCreateBuffer opengl");
    }
    else {
        // Need to update these positions each frame if its not shared by OpenGL
        input_pos = clCreateBuffer(context, CL_MEM_READ_ONLY, size * sizeof(glm::vec3), NULL, &error);
        clEnqueueWriteBuffer(queue, input_pos, CL_FALSE, 0, size * sizeof(glm::vec3), pgl->bodies.pos, 0, NULL, NULL);
        check_error(error, "clCreateBuffer normal buffer");
    }


    input_vel = clCreateBuffer(context, CL_MEM_READ_ONLY, size * sizeof(glm::vec3), NULL, &error);
    check_error(error, "clCreateBuffers input_vel");
    input_acc = clCreateBuffer(context, CL_MEM_READ_ONLY, size * sizeof(glm::vec3), NULL, &error);
    check_error(error, "clCreateBuffers input_acc");
    input_mass = clCreateBuffer(context, CL_MEM_READ_ONLY, size * sizeof(float), NULL, &error);
    check_error(error, "clCreateBuffers input_mass");
    input_dt = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float), NULL, &error);
    check_error(error, "clCreateBuffers input_dt");
    input_size = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL, &error);
    check_error(error, "clCreateBuffers input_size");

    error = clEnqueueWriteBuffer(queue, input_vel, CL_FALSE, 0, size * sizeof(glm::vec3), pgl->bodies.vel, 0, NULL, NULL);
    check_error(error, "clEnqueueWriteBuffer input_vel");
    error = clEnqueueWriteBuffer(queue, input_acc, CL_FALSE, 0, size * sizeof(glm::vec3), pgl->bodies.acc, 0, NULL, NULL);
    check_error(error, "clEnqueueWriteBuffer input_acc");
    error = clEnqueueWriteBuffer(queue, input_mass, CL_FALSE, 0, size * sizeof(float), pgl->bodies.mass, 0, NULL, NULL);
    check_error(error, "clEnqueueWriteBuffer input_mass");
    error = clEnqueueWriteBuffer(queue, input_dt, CL_FALSE, 0, sizeof(float), &pgl->step_dt, 0, NULL, NULL);
    check_error(error, "clEnqueueWriteBuffer input_dt");
    error = clEnqueueWriteBuffer(queue, input_size, CL_FALSE, 0, sizeof(int), &pgl->num_particles, 0, NULL, NULL);
    check_error(error, "clEnqueueWriteBuffer input_size");
    clFinish(queue);
}

static int setup_OpenCL() {

    std::clock_t start;
    start = std::clock();

    const int num_elements = 1 << 25;
    float *buffer = new float[num_elements];
    const int buffer_size = sizeof(float) * num_elements;
    for (int i = 0; i < num_elements; i++) {
//        buffer[i] = 1.0f / std::sqrtf(i + 1);
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
#elif _WIN32
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
        gl_context = true;
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

//    clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 0, NULL, &bytes);
//    size_t devNum = bytes/sizeof(cl_device_id);
//    std::vector<cl_device_id> devs (devNum);
//    clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, bytes, &devs[0], NULL);
//    for (int i = 0; i < devNum; i++) {
//        print_device_name(devs[i]);
//    }

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

cl_device_id CL_Helper::get_best_device() {
    // Get CPU devices
    cl_uint check = 0;
	cl_int error = 0;

	cl_device_id* devices;
	cl_device_id best_device;
	cl_uint device_count;
	cl_uint max_compute_units = 0;

	// Get all devices for the platform
	clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, NULL, &device_count);
	devices = new cl_device_id[device_count];
	clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, device_count, devices, NULL);
    
	// Choose best device, i.e. most compute units
	for (int i = 0; i < device_count; i++) {
		cl_uint check_compute_units = 0;
		clGetDeviceInfo(devices[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(check_compute_units), &check_compute_units, NULL);
		if (check_compute_units > max_compute_units) {
			max_compute_units = check_compute_units;
			best_device = devices[i];
		}
	}
	delete[] devices;

	return best_device;
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

void CL_Helper::apply_gravity() {
    clSetKernelArg(apply_gravity_kernel, 0, sizeof(input_pos), &input_pos);
    clSetKernelArg(apply_gravity_kernel, 1, sizeof(input_vel), &input_vel);
    clSetKernelArg(apply_gravity_kernel, 2, sizeof(input_acc), &input_acc);
    clSetKernelArg(apply_gravity_kernel, 3, sizeof(input_mass), &input_mass);
    clSetKernelArg(apply_gravity_kernel, 4, sizeof(input_size), &input_size);

    // Enqueue our problem to actually be executed by the device
    clEnqueueNDRangeKernel(queue, apply_gravity_kernel, 1, NULL, global_dimensions, NULL, 0, NULL, NULL);
    // Wait for the queue to finish
    clFinish(queue);
}

void CL_Helper::update_positions() {
    clSetKernelArg(update_kernel, 0, sizeof(float*), &input_pos);
    clSetKernelArg(update_kernel, 1, sizeof(float*), &input_vel);
    clSetKernelArg(update_kernel, 2, sizeof(float*), &input_acc);
    clSetKernelArg(update_kernel, 3, sizeof(float*), &input_dt);

    // Enqueue our problem to actually be executed by the device
    clEnqueueNDRangeKernel(queue, update_kernel, 1, NULL, global_dimensions, NULL, 0, NULL, NULL);
    // Wait for the queue to finish
    clFinish(queue);
}

// http://dhruba.name/2012/08/14/opencl-cookbook-listing-all-devices-and-their-critical-attributes/
void CL_Helper::print_platform_info() {
	int i, j;
	char* value;
	size_t valueSize;
	cl_uint platformCount;
	cl_platform_id* platforms;
	cl_uint deviceCount;
	cl_device_id* devices;
	cl_uint maxComputeUnits;

	// get all platforms
	clGetPlatformIDs(0, NULL, &platformCount);
	platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * platformCount);
	clGetPlatformIDs(platformCount, platforms, NULL);
	for (i = 0; i < platformCount; i++) {
		std::cout << "Platform " << i << ":" << std::endl;
		// get all devices
		clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &deviceCount);
		devices = (cl_device_id*)malloc(sizeof(cl_device_id) * deviceCount);
		clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, deviceCount, devices, NULL);

		// for each device print critical attributes
		for (j = 0; j < deviceCount; j++) {

			// print device name
			clGetDeviceInfo(devices[j], CL_DEVICE_NAME, 0, NULL, &valueSize);
			value = (char*)malloc(valueSize);
			clGetDeviceInfo(devices[j], CL_DEVICE_NAME, valueSize, value, NULL);
			printf("%d. Device: %s\n", j + 1, value);
			free(value);

			// print hardware device version
			clGetDeviceInfo(devices[j], CL_DEVICE_VERSION, 0, NULL, &valueSize);
			value = (char*)malloc(valueSize);
			clGetDeviceInfo(devices[j], CL_DEVICE_VERSION, valueSize, value, NULL);
			printf(" %d.%d Hardware version: %s\n", j + 1, 1, value);
			free(value);

			// print software driver version
			clGetDeviceInfo(devices[j], CL_DRIVER_VERSION, 0, NULL, &valueSize);
			value = (char*)malloc(valueSize);
			clGetDeviceInfo(devices[j], CL_DRIVER_VERSION, valueSize, value, NULL);
			printf(" %d.%d Software version: %s\n", j + 1, 2, value);
			free(value);

			// print c version supported by compiler for device
			clGetDeviceInfo(devices[j], CL_DEVICE_OPENCL_C_VERSION, 0, NULL, &valueSize);
			value = (char*)malloc(valueSize);
			clGetDeviceInfo(devices[j], CL_DEVICE_OPENCL_C_VERSION, valueSize, value, NULL);
			printf(" %d.%d OpenCL C version: %s\n", j + 1, 3, value);
			free(value);

			// print parallel compute units
			clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS,
				sizeof(maxComputeUnits), &maxComputeUnits, NULL);
			printf(" %d.%d Parallel compute units: %d\n", j + 1, 4, maxComputeUnits);

		}
		free(devices);
	}
	free(platforms);
}

int main(int argc, char *argv[]) {
	int count = 1 << 11;
	float dt = 0.00005f;
	float step = float(M_PI / 300.0f) / 5.0f;
	physics_gl p(1600, 900, count, dt);
	if (p.disp.wasError()) {
		return p.disp.wasError();
	}

	printf("OpenGL version: %s\n", glGetString(GL_VERSION));

	CL_Helper c(&p);
	c.print_platform_info();

	int size_particle = 3 * sizeof(glm::vec3);
	p.shader.use();
	glBindVertexArray(p.vao);

	p.perspective_matrix = glm::perspective(80.0f, (float)p.disp.getWidth() / (float)p.disp.getHeight(), 0.1f, 100.0f);
	glUniformMatrix4fv(p.project_uniform, 1, GL_FALSE, glm::value_ptr(p.perspective_matrix));


	glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 view;

	float counter = 0;
    int frames = 0;
	// Update transformation camera
	view = glm::lookAt(glm::vec3(2 * sin(counter), 1.1f*sin(1.3*counter)*cos(.33f*counter), 2 * cos(counter)), cameraTarget, up);
	//view = glm::lookAt(glm::vec3(2.5 * sin(counter), 0.5f , 2.5 * cos(counter)), cameraTarget, up);
	glUniformMatrix4fv(p.view_uniform, 1, GL_FALSE, glm::value_ptr(view));


//	std::cout << "Last body outside of loop: ";
//	p.bodies.printBody(0);
//	std::cout << std::endl;
	//    for (int i = 0; i < p.num_particles; i++) {
	//        p.bodies.pos[i] = {0.0f, 0.0f, 0.0f};
	//    }

	while (!p.disp.isClosed()) {
		p.disp.clear(0.0f, 0.0f, 0.0f, 1.0f);
		if (p.disp.wasResized()) {
			p.perspective_matrix = glm::perspective(80.0f, (float)p.disp.getWidth() / (float)p.disp.getHeight(), 0.1f, 100.0f);
			glUniformMatrix4fv(p.project_uniform, 1, GL_FALSE, glm::value_ptr(p.perspective_matrix));
			glViewport(0, 0, p.disp.getWidth(), p.disp.getHeight());
		}

		// Update particle positions using OpenCL
		// Flush GL queue
		glFlush();

		if (c.is_gl_context()) {
			// Acquire shared objects
			cl_int err = clEnqueueAcquireGLObjects(c.queue, 1, &c.input_pos, 0, NULL, NULL);
			c.check_error(err, "clEnqueueAcquireGLObjects");
			// Enqueue OpenCL commands to operate on objects (kernels, read/write commands, etc)
			c.apply_gravity();
			c.update_positions();

            // For testing if data is actually being moved...
//            if (frames % 4000 == 0) {
//                clEnqueueReadBuffer(c.queue, c.input_pos, CL_TRUE, 0, p.num_particles * sizeof(glm::vec3), p.bodies.pos, 0, NULL, NULL);
//                clEnqueueReadBuffer(c.queue, c.input_vel, CL_TRUE, 0, p.num_particles * sizeof(glm::vec3), p.bodies.vel, 0, NULL, NULL);
//                clFinish(c.queue);
//                p.bodies.printBody(0);
//                std::cout << std::endl;
//            }
			// Release shared objects
			err = clEnqueueReleaseGLObjects(c.queue, 1, &c.input_pos, 0, NULL, NULL);
			c.check_error(err, "releasing GL objects");
			clFinish(c.queue);
		}
		else {
			c.apply_gravity();
			c.update_positions();
			clEnqueueReadBuffer(c.queue, c.input_pos, CL_TRUE, 0, p.num_particles * sizeof(glm::vec3), p.bodies.pos, 0, NULL, NULL);
			clFinish(c.queue);
			glBindBuffer(GL_ARRAY_BUFFER, p.positions_vbo);
			glBufferSubData(GL_ARRAY_BUFFER, 0, p.num_particles * sizeof(glm::vec3), (GLvoid *)p.bodies.pos);
		}

		//counter += step;

		view = glm::lookAt(glm::vec3(2 * sin(counter), 1.1f*sin(1.3*counter)*cos(.33f*counter), 2 * cos(counter)), cameraTarget, up);
		//view = glm::lookAt(glm::vec3(2.5 * sin(counter), 0.5f , 2.5 * cos(counter)), cameraTarget, up);
		glUniformMatrix4fv(p.view_uniform, 1, GL_FALSE, glm::value_ptr(view));


		// Draw the particles to the screen, and update
		glDrawArraysInstanced(GL_TRIANGLES, 0, size_particle, count);
		p.disp.update();
	}

	return 0;
}