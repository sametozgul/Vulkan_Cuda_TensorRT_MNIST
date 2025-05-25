#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H


#include <assert.h>
#include <vector>

#include <vulkan/vulkan.h>

#include "Context.h"


class SwapChain
{
public:
    SwapChain() = default;

    VkSwapchainKHR swapChain{VK_NULL_HANDLE};
    std::vector<VkImage> images;
    VkFormat imageFormat;
    VkExtent2D extent;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;


public:
    void init(VulkanData& vulkanData);
    void create();
    void createImageViews();
    void recreate();
    void cleanup();
public:
    VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t& imageIndex);
    VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
private:

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
private:
    VulkanData vulkanData_;
};

#endif // SWAPCHAIN_H
