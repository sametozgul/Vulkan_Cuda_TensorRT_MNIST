#ifndef WINDOW_H
#define WINDOW_H

#include <GLFW/glfw3.h>
#include <memory>
#include "Renderer.h"

class Window
{
public:
    Window() = default;
    void init(int width, int height);
    void mainLoop();
    void cleanUp();
private:
    GLFWwindow* window;
    std::unique_ptr<Renderer> renderer = nullptr;

};

#endif // WINDOW_H
