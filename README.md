# About
A simple N-body simulation originally written in C++ using OpenGL for visualization and OpenMP for parallelization.
OpenCL was later added to support GPUs and better CPU multithreading on supported devices.

## Options
`gravity_cl` has the ability to select OpenCL platform (e.g. NVIDIA, Intel, AMD) and OpenCL device using `-p` and `-d` flags respectively.
If no device flag is present, the device with the most compute units is selected.
Similarly, if no platform is specified, the first platform retured by OpenCL is used.

For full set of options, use `-h`

# Building
```
mkdir build
cd build
cmake ..
cmake --build .
ln -s ../res
```

This builds 2 executables, `gravity` and `gravity_cl`.

The `res` folder must be in the same directory as the executables so the OpenGL shaders and OpenCL kernel are visible.

# Dependencies
- [SDL2](https://www.libsdl.org/download-2.0.php)
- [GLEW](https://github.com/nigels-com/glew)
- [GLM](https://glm.g-truc.net/0.9.8/index.html)
- OpenGL
- OpenCL
