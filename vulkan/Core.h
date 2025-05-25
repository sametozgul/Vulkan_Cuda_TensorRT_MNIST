#ifndef CORE_H
#define CORE_H

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#include <GLFW/glfw3.h>

#include <vector>

#include "Context.h"
#include "Validation.h"
#include "Shader.h"
#include "SwapChain.h"
#include "DepthBuffer.h"

#include "HelperFunctions.h"
#include "Initializers.h"
#include "imgui.h"
#include "UserInterface.h"
#include "Camera.h"

static const size_t MAX_FRAMES_IN_FLIGHT = 5;

class Core
{
public:
    Core();
    virtual ~Core();
    void init();
    void cleanUp();

protected:
    // Core elements of Vulkan
public:
    GLFWwindow *window_;
    VkInstance instance_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice logicalDevice_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkRenderPass renderPass_;
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers;
    SwapChain swapChain{};
    VkFormat depthFormat;
    DepthBuffer depthBufferObject{};
    UserInterface mUserInterface{};

    // Window
public:
    void windowResize();
    void updateCamera(const int &key);
    void mousePressCallBack(int button, int action, int mods);
    void mouseCursorPosCallBack(const double &x, const double &y);
    void mouseCursorCallBack(const double &x, const double &y);
    bool isMouseFirst = true;
    struct Mouse
    {
        struct
        {
            bool left = false;
            bool right = false;
            bool middle = false;
        } buttons;
        glm::vec2 position;
    } mouse;
    bool viewUpdated = false;

    // Instance
private:
    void createInstance();

    // Extensions and validation layers.
private:
    bool checkRequestedLayerSupported();
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    std::vector<const char *> getRequiredExtensions();
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif
    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
                                                        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
                                                        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
                                                        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME};

private:
    Validation validation{};

    // Physical and logical device functions
private:
    void pickPhysicalDevice();
    void createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    // QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    VkSampleCountFlagBits getMaxUsableSampleCount();

protected:
    QueueFamilyIndices indices{};
    // Queuesco
protected:
    VkQueue graphicQueue_;
    VkQueue presentQueue_;
    // Surface
private:
    void createSurface();
    // Swap Chain
public:
    void createPipelineCache();
    VkPipelineCache pipelineCache{VK_NULL_HANDLE};
    // Render passes
private:
    void createRenderPass();
    // FrameBuffers
private:
    void createFramebuffers();
    // Commands
private:
    void createCommandPool();
    void createCommandBuffer();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    // Draw
public:
    void nextFrame();
    void prepareFrame(uint32_t &imageIndex, size_t &currentFrameIdx);
    void submitFrame(uint32_t &imageIndex, size_t &currentFrameIdx);
    // void drawFrame();

    uint32_t currentFrame = 0;
    // Synchronization
protected:
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    bool framebufferResized = false;
    void createSyncObjects();
    // MipMap
protected:
    uint32_t mipLevels = 1;

private:
    VkImage textureImage;

protected:
    glm::mat4 mModelMatrix;
    glm::mat4 mViewMatrix;
    glm::mat4 mProjectionMatrix;
    Camera camera{};

private:
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;
    void createColorResources();

protected:
    VkSubmitInfo submitInfo{};

    void clearSwapChain();

protected:
    virtual void render() = 0;
    virtual void prepare();
    virtual void renderFrame();
    virtual void buildCommandBuffers();
    virtual void OnUpdateUIOverlay(UserInterface *userInterface);
    bool prepared = false;
    bool requiresStencil{false};

protected:
    VulkanData context;
    UIData uiData;

protected:
    Shader vertexShaders;
    Shader fragmentShaders;

protected:
    void drawUI(const VkCommandBuffer commandBuffer);

    uint8_t vkDeviceUUID_[VK_UUID_SIZE];
};

#endif // CORE_H