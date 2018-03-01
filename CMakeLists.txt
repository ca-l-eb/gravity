cmake_minimum_required(VERSION 3.5)
project(gravity)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    message(STATUS "Using OpenMP to parallelize physics")
endif()

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenCL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)

set(SHARED_SOURCE_FILES
    src/args.h
    src/display.cc
    src/display.h
    src/physics_gl.cc
    src/physics_gl.h
    src/pobject.cc
    src/pobject.h
    src/shader.cc
    src/shader.h
    src/simpleio.cc
    src/simpleio.h
)

set(CL_SOURCE_FILES
    src/physics_cl.cc
    src/physics_cl.h
)

set(SHARED_LIBS ${CMAKE_THREAD_LIB_INIT} ${SDL2_LIBRARIES} ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES})
set(SHARED_INCLUDES ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})

add_executable(gravity src/main.cc ${SHARED_SOURCE_FILES})
add_executable(gravity_cl src/main_opencl.cc ${SHARED_SOURCE_FILES} ${CL_SOURCE_FILES})

target_link_libraries(gravity ${SHARED_LIBS})
target_link_libraries(gravity_cl ${SHARED_LIBS} ${OpenCL_LIBRARIES})

target_include_directories(gravity PUBLIC ${SHARED_INCLUDES})
target_include_directories(gravity_cl PUBLIC ${SHARED_INCLUDES} ${OpenCL_INCLUDE_DIRS})
