#include "DepthBuffer.h"
#include "HelperFunctions.h"
// Creating a depth image is fairly straightforward.
// It should have the same resolution as the color attachment, defined by the swap chain extent, an image usage appropriate for a depth attachment, optimal tiling and device local memory

// Unlike the texture image, we don't necessarily need a specific format, because we won't be directly accessing the texels from the program.
// It just needs to have a reasonable accuracy, at least 24 bits is common in real-world applications.

// There are several formats that fit this requirement:
// VK_FORMAT_D32_SFLOAT: 32-bit float for depth
// VK_FORMAT_D32_SFLOAT_S8_UINT: 32-bit signed float for depth and 8 bit stencil component
// VK_FORMAT_D24_UNORM_S8_UINT: 24-bit float for depth and 8 bit stencil component

void DepthBuffer::init(const VulkanData &vulkanData)
{
    vulkanData_ = vulkanData;
}

void DepthBuffer::cleanUp()
{
    vkDestroyImageView(vulkanData_.device, imageView, nullptr);
    vkDestroyImage(vulkanData_.device, image, nullptr);
    vkFreeMemory(vulkanData_.device, imageMemory, nullptr);
}

void DepthBuffer::createSources()
{

    VkFormat depthFormat;
    VkBool32 validDepthFormat = getSupportedDepthFormat(vulkanData_.physicalDevice, &depthFormat);
    assert(validDepthFormat);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = vulkanData_.swapChainExtent.width;
    imageInfo.extent.height = vulkanData_.swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples =  vulkanData_.msaaSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


    image = createImage(vulkanData_, imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageMemory);
    // That's it for creating the depth image.
    // We don't need to map it or copy another image to it, because we're going to clear it at the start of the render pass like the color attachment.
    // We don't need to explicitly transition the layout of the image to a depth attachment because we'll take care of this in the render pass.
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.image = image;

    if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    imageView = createImageView(vulkanData_.device, viewInfo);

}

bool DepthBuffer::hasStencilComponent(VkFormat format)
{
    // helper function that tells us if the chosen depth format contains a stencil component:
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}
