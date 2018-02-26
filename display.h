#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>

class GLDisplay
{
public:
    GLDisplay(int width, int height, const char *title);
    ~GLDisplay();

    inline int width()
    {
        return _width;
    }

    inline int height()
    {
        return _height;
    }

    inline bool is_closed()
    {
        return closed;
    }

    bool resized();
    void clear(float r, float g, float b, float a);
    void update();

private:
    SDL_Window *window;
    SDL_GLContext glContext;
    int _width, _height;
    bool closed, clear_enabled, fullscreen, _resized;
};

#endif
