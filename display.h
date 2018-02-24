#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>

class GLDisplay
{
public:
    GLDisplay(int width, int height, const char *title);
    ~GLDisplay();
    inline int getWidth()
    {
        return width;
    }
    inline int getHeight()
    {
        return height;
    }
    inline int wasError()
    {
        return error;
    }
    inline bool isClosed()
    {
        return closed;
    }
    bool wasResized();
    void clear(float r, float g, float b, float a);
    void update();

private:
    SDL_Window *window;
    SDL_GLContext glContext;
    int error;
    int width, height;
    bool closed, clearEnabled, fullscreen, resized;
};

#endif
