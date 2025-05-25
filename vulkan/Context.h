#ifndef CONTEXT_H
#define CONTEXT_H

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <cassert>
#include <optional>
#include <vector>
#include <array>
#include <iostream>

const int MAX_INSTANCES = 2;
const uint32_t shadowMapize{ 1024 };
#define VK_FLAGS_NONE 0
#define VK_CHECK(func){                                  \
const VkResult result = func;                        \
    if(result != VK_SUCCESS){                            \
        std::cerr << "Error handling function " << #func \
        << " at " << __FILE__ << ":"                     \
        << __LINE__ << ". Result is "                    \
        << string_VkResult(result)                       \
        << std::endl;                                    \
        assert(false);                                   \
}                                                    \
}                                                        \
 \


    struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
        return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Screen{
    int width = 0;
    int height = 0;
};

struct VulkanData{
    GLFWwindow *window = nullptr;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkCommandPool commandPool;
    VkQueue graphicQueue;
    VkExtent2D swapChainExtent;
    VkRenderPass renderpass;
    std::vector<VkBuffer> uniformBuffers;
    VkCommandBuffer commandBuffer;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkSurfaceKHR surface;
    Screen screenProperties;
    uint8_t *vkDeviceUUID = nullptr;

};

struct SamplerModes{
    VkSamplerAddressMode modeU;
    VkSamplerAddressMode modeV;
    VkSamplerAddressMode modeW;
};

struct UIData{
    float frameTime = 0.0f;
    float lightPosition[3] = {0.0f, 0.0f, 0.0f};
    float tickDiff = 0.0f;
    std::string physicalDeviceName;
    float animationSpeed = 1.0;
    int currenItem = 0;
    bool isSkyBox = true;
    bool isBloom = true;
    float bias = 0.1f;

    bool requiredUpdate = false;
};


struct  InstanceData{
    glm::mat4 instances[MAX_INSTANCES]; // 128  -> 64 * MAX_INSTANCES bytes
};




#endif // CONTEXT_H
