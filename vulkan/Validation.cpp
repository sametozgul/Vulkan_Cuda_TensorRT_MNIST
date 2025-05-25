#include "Validation.h"
#include <iostream>

void Validation::setupDebugMessenger(const VkInstance &instance)
{

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger)) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
    std::cout << "Debug manager is created...(Validaition Layer is activated.)" << std::endl;

}

void Validation::DestroyDebugUtilsMessengerEXT(const VkInstance& instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VkBool32 Validation::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult Validation::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    // PFN_vkCreateDebugUtilsMessengerEXT -> Application-defined debug messenger callback function
    // vkCreateDebugUtilsMessengerEXT -> A messenger object which handles passing along debug messages to a provided debug callback
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void Validation::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
{
    // Create VkDebugUtilsMessengerCreateInfoEXT instace to debug vulkan API.
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT -> Indicates all messages from the Vulkan loader, layers, and drivers.
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT -> Specifies use of Vulkan that may expose an application bug.
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT -> Specifies that the application has violated a valid usage condition of the specification
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT -> Specifies that some general event has occurred.
    // VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT -> Specifies that something has occurred during validation against the Vulkan specification that may indicate invalid behavior
    // VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT -> Specifies a potentially non-optimal use of Vulkan, e.g. using vkCmdClearColorImage when setting VkAttachmentDescription::loadOp to VK_ATTACHMENT_LOAD_OP_CLEAR would have worked.
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void Validation::cleanUp(const VkInstance& instance)
{
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
}

