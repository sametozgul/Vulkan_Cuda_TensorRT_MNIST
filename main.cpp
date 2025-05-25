

#define GLFW_INCLUDE_VULKAN


#include "Window.h"
#include <memory>

using namespace std;
int main()
{
    std::unique_ptr<Window> window = std::make_unique<Window>();

    window->init(640,640);
    window->mainLoop();
    window->cleanUp();
    return 0;
}
