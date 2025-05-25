#include "SwapChain.h"
#include "HelperFunctions.h"
#include <algorithm>

void SwapChain::init(VulkanData &vulkanData)
{
    vulkanData_ = vulkanData;
}

VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    // If the swapChainadequate conditions were met then support is definitely sufficient, but may still be many diffferent modes.
    // There are three types of setting to determine:
    // - Surface format (color depth)
    // - Presentation mode (conditions for "swapping" images to screen
    // - Swap extent (resolution of images in swap chain)

    // VkSurfaceFormatKHR entry contains a format and colorSpace member.
    // - The format member specifies the color channels and types. For example VK_FORMAT_B8G8R8A8_SRGB that we store B G R and alpga channels.
    // - colorSpcae member specifies if the SRGB color space is supported or not using VK_COLOR_SPACE_SRGB_NONLINEAR_KHR flag.

    // For the color space we'll use SRGB if it is available, because it result in more accurate perceived colors.
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    // There are four possible present modes available in Vulkan.
    // VK_PRESENT_MODE_IMMEDIATE_KHR -> images submitted by applicaton immediataly to the screen right away so there might be tearing.
    // VK_PRESENT_MODE_FIFO_KHR -> The swap chain is a queue where display takes an image from the front of queue when the display is refreshed.
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR -> This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank.
    // VK_PRESENT_MODE_MAILBOX_KHR -> This is another variation of the second mode.
    // Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    // Swap extent is resoulution of the swap chain images and it's almost exactly equal to resolution of window.
    // The range of the possible resolutions are defined in the VkSurfaceCapabilitiesKHR structure.

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        vulkanData_.screenProperties.width = capabilities.currentExtent.width;
        vulkanData_.screenProperties.height = capabilities.currentExtent.width;
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(vulkanData_.window, &width, &height);
        vulkanData_.screenProperties.width = width;
        vulkanData_.screenProperties.height = height;

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void SwapChain::create()
{

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanData_.physicalDevice, vulkanData_.surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D tempExtent = chooseSwapExtent(swapChainSupport.capabilities);
    // Decide how many images we would like to have in the swap chain.
    // Implementation specifies the minimum number that it requires to function.
    // However, simply sticking to this minimum means that we may sometimes have to wait on the driver to complete internal operations before we can acquire another image to render to.
    // Therefore it is recommended to request at least one more image than the minimum
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    // Create swap chain createInfo
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vulkanData_.surface;
    createInfo.minImageCount = imageCount;

    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = tempExtent;
    // ImageArrayLayers specifies the amount of layers each image consist of.
    createInfo.imageArrayLayers = 1;
    // ImageUsage specifies what kind of operations we'll use the image in swap chain for. We're going to render directly to them which means they are used as a color attachment
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    // Specify how to handle swap chain images that will be used across multiple queue families
    // That will be the case in our application if the graphics queue family is different from the presentation queue.
    // We'll be drawing on the images in the swap chain from graphics queue and then submitting them on the presentation queue.

    // There are two ways to handle images that are accessed from multiple queue.
    // VK_SHARING_MODE_EXCLUSIVE -> An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family.
    // This option offers the best performance
    // VK_SHARING_MODE_CONCURRENT -> mages can be used across multiple queue families without explicit ownership transfers.

    // If the queue families differ, then we'll be using the concurrent mode
    // Concurrent mode requires you to specify in advance between which queue families ownership will be shared using the queueFamilyIndexCount and pQueueFamilyIndices parameters
    // If the graphics queue family and presentation queue family are the same, which will be the case on most hardware, then we should stick to exclusive mode
    // Because concurrent mode requires you to specify at least two distinct queue families.
    QueueFamilyIndices indices = findQueueFamilies(vulkanData_.physicalDevice, vulkanData_.surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsAndComputeFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    // We can specify that a certain transform should be applied to images in the swap chain if it is supported (supportedTransforms in capabilities),
    // Like a 90 degree clockwise rotation or horizontal flip
    // To specify that you do not want any transformation, simply specify the current transformation
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    // The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system
    // Almost always want to simply ignore the alpha channel
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // The presentMode member speaks for itself.
    createInfo.presentMode = presentMode;
    // If the clipped is VK_TRUE  that means we don't care about color of pixels that are obscured.
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(vulkanData_.device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    std::cout << "Swap chain is initialized..." << std::endl;

    vkGetSwapchainImagesKHR(vulkanData_.device, swapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(vulkanData_.device, swapChain, &imageCount, images.data());

    imageFormat = surfaceFormat.format;
    extent = tempExtent;


}

void SwapChain::createImageViews()
{
    // To use any VkImage, including those in the swapchain, in ther render pipeline we have to create a VkImageView object.
    // An image view is quite literally a view into an image. It describes how to access the image and which part of the image to access,
    // Create image views for each image in swap chain.
    imageViews.resize(images.size());
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    for (size_t i = 0; i < images.size(); i++) {
        viewInfo.image = images[i];
        imageViews[i] = createImageView(vulkanData_.device, viewInfo);
    }
    std::cout << "Image views are initialized..." << std::endl;
}

void SwapChain::recreate()
{
    // To make sure that the old versions of these objects are cleaned up before recreating them,
    cleanup();
    create();
    createImageViews();

}

void SwapChain::cleanup()
{

    for (size_t i = 0; i < framebuffers.size(); i++) {
        vkDestroyFramebuffer(vulkanData_.device, framebuffers[i], nullptr);
    }

    for (size_t i = 0; i < imageViews.size(); i++) {
        vkDestroyImageView(vulkanData_.device, imageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(vulkanData_.device, swapChain , nullptr);
}

VkResult SwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t &imageIndex)
{
    return vkAcquireNextImageKHR(vulkanData_.device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, &imageIndex);
}

VkResult SwapChain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &imageIndex;
    // Check if a wait semaphore has been specified to wait for before presenting the image
    if (waitSemaphore != VK_NULL_HANDLE)
    {
        presentInfo.pWaitSemaphores = &waitSemaphore;
        presentInfo.waitSemaphoreCount = 1;
    }
    return vkQueuePresentKHR(queue, &presentInfo);
}
