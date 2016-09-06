cmake_minimum_required(VERSION 3.5)
project(gravity)

#set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11  -O3 -g")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(WIN32)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} C:/dev/SDL2-2.0.4/x86_64-w64-mingw32/lib/cmake/SDL2)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} C:/dev/glm/cmake)
    set(SDL2_DIR C:/dev/SDL2-2.0.4/x86_64-w64-mingw32/lib/cmake/SDL2)
    set(SDL2_INCLUDE_DIRS C:/dev/SDL2-2.0.4/x86_64-w64-mingw32/include)
    set(GLEW_INCLUDE_DIR C:/dev/glew-1.13.0/include)
    set(GLEW_LIBRARY C:/dev/glew-1.13.0/lib/Release/x64)
    set(GLM_INCLUDE_DIRS C:/dev/glm)
endif(WIN32)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} -mavx")
    message(STATUS "Using OpenMP to parallelize physics")
endif()

set(CMAKE_MODULE_PATH /usr/share/cmake-3.5/Modules)
find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(Threads REQUIRED)

if(UNIX AND NOT APPLE)
    set(OpenCL_LIBRARY /usr/lib/x86_64-linux-gnu/libOpenCL.so.1.0.0)
#    find_package(MESA REQUIRED)
endif()
find_package(OpenCL REQUIRED)


#include_directories(${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})

set(RESOURCES_DIRECTORY res)
set(SOURCE_FILES main.cc display.cc display.h pobject.cc pobject.h shader.cc shader.h simpleio.cc simpleio.h)
set(SOURCE_FILES_CL opencl.cc simpleio.cc simpleio.h opencl.h display.cc display.h physics_gl.cc physics_gl.h shader.cc shader.h pobject.cc pobject.h)
file(COPY ${RESOURCES_DIRECTORY} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_executable(gravity ${SOURCE_FILES})
add_executable(gravity_cl ${SOURCE_FILES_CL})

# Link the libraries to the executable
target_link_libraries(gravity ${SDL2_LIBRARIES} ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${CMAKE_THREAD_LIB_INIT})
target_link_libraries(gravity_cl ${OpenCL_LIBRARIES} ${SDL2_LIBRARIES} ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES})