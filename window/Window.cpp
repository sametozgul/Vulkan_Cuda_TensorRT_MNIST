#include "Window.h"
#include <iostream>

void Window::init(int width, int height)
{
    if(!glfwInit()){
        std::cerr << "GLFW can not be initialized..." << std::endl;
    }
    // Tell to GLFW this is not OpenGL API
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Tell to GLFW the window is not resizable.
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, "glTF Loading", nullptr, nullptr);
    if(window == nullptr){
        glfwTerminate();
        std::cerr << "The window is not created..." << std::endl;
        return;
    }
    renderer = std::make_unique<Renderer>(*window);

    glfwSetInputMode(window,GLFW_CURSOR, GLFW_CURSOR);
    glfwSetWindowUserPointer(window, renderer.get());
    glfwSetWindowSizeCallback(window,[](GLFWwindow* win,int width,int height){
        auto renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(win));
        renderer->framebufferResizeCallback(win,width,height);
    });
    glfwSetKeyCallback(window, [](GLFWwindow *win, int key, int scancode, int action, int mods){
        auto renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(win));
        renderer->keyboardInputCallBack(win, key, scancode, action, mods);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow *win, int button, int action, int mods){
        auto renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(win));
        renderer->handleMousePressCallBack(win, button, action, mods);

    });
    glfwSetCursorPosCallback(window, [](GLFWwindow *win, double xpos, double ypos){
        auto renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(win));
        renderer->handleCursorPosCallBack(win, xpos, ypos);
    });
    glfwSetScrollCallback(window, [](GLFWwindow *win, double xpos, double ypos){
        auto renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(win));
        renderer->handleCursorCallBack(win, xpos, ypos);
    });
    std::cout << "The window is initialized..." << std::endl;
}

void Window::mainLoop()
{
    glfwSwapInterval(1);
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        renderer->render();
    }
}

void Window::cleanUp()
{
    renderer->cleanUp();
    glfwDestroyWindow(window);
    glfwTerminate();
}
