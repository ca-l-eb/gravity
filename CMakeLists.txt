cmake_minimum_required(VERSION 3.5)
project(gravity)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17  -O3 -g -march=native")

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    message(STATUS "Using OpenMP to parallelize physics")
endif()

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenCL REQUIRED)

set(SOURCE_FILES main.cc display.cc display.h pobject.cc pobject.h shader.cc shader.h simpleio.cc simpleio.h physics_gl.cc physics_gl.h)
set(SOURCE_FILES_CL main_opencl.cc physics_cl.cc simpleio.cc simpleio.h physics_cl.h display.cc display.h physics_gl.cc physics_gl.h shader.cc shader.h pobject.cc pobject.h)

add_executable(gravity ${SOURCE_FILES})
add_executable(gravity_cl ${SOURCE_FILES_CL})

set(LIBS ${CMAKE_THREAD_LIB_INIT} ${SDL2_LIBRARIES} ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES})

# Link the libraries to the executable
target_link_libraries(gravity ${LIBS})
target_link_libraries(gravity_cl ${LIBS} ${OpenCL_LIBRARIES})

target_include_directories(gravity PUBLIC ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
target_include_directories(gravity_cl PUBLIC ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
