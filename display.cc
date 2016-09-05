#include <GL/glew.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>

#ifdef _WIN32
#include <GL/GL.h>
#elif __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <iostream>
#include <string>

#include "display.h"


GLDisplay::GLDisplay(int width, int height, const char* title) {
    // Initialize SDL video
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Set attributes for RGBA sizes
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    // Set double buffered
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Create the window, centered
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        std::cerr << "Could not create a window!" << std::endl;
        error = 1;
    }
	this->width = width;
	this->height = height;

    // Get an OpenGL context to work with
    glContext = SDL_GL_CreateContext(window);
    if (glContext == NULL) {
        std::cerr << "Could not create OpenGL context!" << std::endl;
        error = 2;
    }

    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if (status != GLEW_OK) {
        std::cerr << "GLEW failed to initialize!" << std::endl;
        error = 3;
    }

    error = 0;
    closed = false;
    clearEnabled = true;
    fullscreen = false;
    resized = false;
}

GLDisplay::~GLDisplay() {
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// Clear the display to the specified color
void GLDisplay::clear(float r, float g, float b, float a) {
    if (clearEnabled) {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

bool GLDisplay::wasResized() {
    if (resized) {
        resized = false;
        return true;
    }
    return false;
}

void GLDisplay::update() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            closed = true;
        }
        else if (e.type == SDL_KEYDOWN) {
            auto keyPressed = e.key.keysym.sym;
            if (keyPressed == SDLK_UP) {
                clearEnabled = !clearEnabled;
            }
            if (keyPressed == SDLK_F11) {
				// Todo: change resolution to fit native monitor if not already
                fullscreen = !fullscreen;
                if (fullscreen) {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                }
                else {
                    SDL_SetWindowFullscreen(window, 0);
                }
            }
        }
        else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                width = e.window.data1;
                height = e.window.data2;
                resized = true;
            }
        }
    }
    SDL_GL_SwapWindow(window);
}
