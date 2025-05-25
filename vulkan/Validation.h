#ifndef VALIDATION_H
#define VALIDATION_H

#include <vulkan/vulkan.h>

class Validation
{
public:
    Validation() = default;
    void setupDebugMessenger(const VkInstance &instance);
    VkDebugUtilsMessengerEXT debugMessenger;
    void DestroyDebugUtilsMessengerEXT(const VkInstance &instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void* pUserData);
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void cleanUp(const VkInstance &instance);
};

#endif // VALIDATION_H
