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

set(SOURCE_FILES main.cc display.cc display.h pobject.cc pobject.h shader.cc shader.h simpleio.cc simpleio.h physics_gl.cc physics_gl.h args.h)
set(SOURCE_FILES_CL main_opencl.cc physics_cl.cc simpleio.cc simpleio.h physics_cl.h display.cc display.h physics_gl.cc physics_gl.h shader.cc shader.h pobject.cc pobject.h args.h)

set(SHARED_LIBS ${CMAKE_THREAD_LIB_INIT} ${SDL2_LIBRARIES} ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES})
set(SHARED_INCLUDES ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})

add_executable(gravity ${SOURCE_FILES})
add_executable(gravity_cl ${SOURCE_FILES_CL})

target_link_libraries(gravity ${SHARED_LIBS})
target_link_libraries(gravity_cl ${SHARED_LIBS} ${OpenCL_LIBRARIES})

target_include_directories(gravity PUBLIC ${SHARED_INCLUDES})
target_include_directories(gravity_cl PUBLIC ${SHARED_INCLUDES})
