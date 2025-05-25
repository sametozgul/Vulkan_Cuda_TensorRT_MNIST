#ifndef HELPERFUNCTIONS_H
#define HELPERFUNCTIONS_H

#include <random>
#include "Context.h"
#include "Initializers.h"
static VkCommandBuffer beginSingleTimeCommands(const VulkanData &vulkanData)
{
    // Memory transfer operations are executed using command buffers, just like drawing commands. Therefore we must first allocate a temporary command buffer
    // You may wish to create a separate command pool for these kinds of short-lived buffers, because the implementation may be able to apply memory allocation optimizations.
    // You should use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in that case.
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vulkanData.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vulkanData.device, &allocInfo, &commandBuffer);
    // Start recording the command buffer:
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // We're only going to use the command buffer once and wait with returning from the function until the copy operation has finished executing. It's good practice to tell the driver about our intent using VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}
static void endSingleTimeCommands(const VulkanData &vulkanData, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(vulkanData.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkanData.graphicQueue);

    vkFreeCommandBuffers(vulkanData.device, vulkanData.commandPool, 1, &commandBuffer);
}

static VkImageView createImageView(const VkDevice &device, VkImageViewCreateInfo info)
{

    VkImageView imageView;

    VK_CHECK(vkCreateImageView(device, &info, nullptr, &imageView));
    return imageView;
}
static uint32_t findMemoryType(const VkPhysicalDevice &physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    // VkPhysicalDeviceMemoryProperties has two arrays memoryTypes and memoryHeaps
    // MemoryHeaps are distinct memory resources like dedicated VRAM and swap space in RAM for when VRAM runs out.
    // The different types of memory exist within these heaps.
    // Right now we'll only concern ourselves with the type of memory and not the heap it comes from, but you can imagine that this can affect performance.
    VkPhysicalDeviceMemoryProperties memProperties;
    // Query available supported memory types
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    // The typeFilter parameter will be used specify the bit field of memory types are suitable. That means that we can find the index of a suitable memory
    // type by simply iterating over them and checking if the corresponding bit is set to 1.
    // However We're not just interested in a memory type that is suitable for the vertex buffer. We also need to be able to write our vertex data to that
    // memory. The memoryTypes array consist of VkMemoryType structs that specify the and properties of each type of memory.
    // The properties define special features of the memory, like being able to map it so we can write it from the CPU.
    // This property is indicated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, but we also need to use the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property.
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}
static void transitionImageLayout(const VulkanData &vulkanData, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
    // One of the most common ways to perform layout transitions is using an image memory barrier.
    // A pipeline barrier like that is generally used to synchronize access to resources, like ensuring that a write to a buffer completes before reading from it, but it can also be used to transition image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanData);
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.image = image;
    // imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        imageMemoryBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (imageMemoryBarrier.srcAccessMask == 0)
        {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }
    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    endSingleTimeCommands(vulkanData, commandBuffer);
}

static void generateMipmaps(const VulkanData &vulkanData, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(vulkanData.physicalDevice, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanData);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);

        VkImageBlit blit = {};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
                       image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);

        if (mipWidth > 1)
            mipWidth /= 2;
        if (mipHeight > 1)
            mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    endSingleTimeCommands(vulkanData, commandBuffer);
}
static VkImage createImage(const VulkanData &vulkanData, VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags properties, VkDeviceMemory &imageMemory, bool externalMemory = false)
{
    VkImage tempImage;

    VkExportMemoryAllocateInfoKHR vulkanExportMemoryAllocateInfoKHR = {};
    vulkanExportMemoryAllocateInfoKHR.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
    vulkanExportMemoryAllocateInfoKHR.pNext = NULL;
    vulkanExportMemoryAllocateInfoKHR.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    VkExternalMemoryImageCreateInfo vkExternalMemImageCreateInfo = {};
    vkExternalMemImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    vkExternalMemImageCreateInfo.pNext = NULL;
    vkExternalMemImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    if (externalMemory)
    {
        imageInfo.pNext = &vkExternalMemImageCreateInfo;
        allocInfo.pNext = &vulkanExportMemoryAllocateInfoKHR;
    }

    if (vkCreateImage(vulkanData.device, &imageInfo, nullptr, &tempImage) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vulkanData.device, tempImage, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(vulkanData.physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vulkanData.device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(vulkanData.device, tempImage, imageMemory, 0);

    return tempImage;
}

static void copyBufferToImage(const VulkanData &vulkanData, VkImage &image, VkBuffer buffer, std::vector<VkBufferImageCopy> bufferCopyRegions)
{
    // Just like with buffer copies, you need to specify which part of the buffer is going to be copied to which part of the image
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanData);
    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(bufferCopyRegions.size()),
        bufferCopyRegions.data());

    endSingleTimeCommands(vulkanData, commandBuffer);
}

static VkFormat findSupportedFormat(const VkPhysicalDevice &physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    // We're going to write a function findSupportedFormat that takes a list of candidate formats in order from most desirable to least desirable, and checks which is the first one that is supported
    // The VkFormatProperties struct contains three fields:
    // linearTilingFeatures: Use cases that are supported with linear tiling
    // optimalTilingFeatures: Use cases that are supported with optimal tiling
    // bufferFeatures: Use cases that are supported for buffers
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}
static VkBool32 formatIsFilterable(VkPhysicalDevice physicalDevice, VkFormat format, VkImageTiling tiling)
{
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);

    if (tiling == VK_IMAGE_TILING_OPTIMAL)
        return formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    if (tiling == VK_IMAGE_TILING_LINEAR)
        return formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    return false;
}
static void copyBufferToImage(const VulkanData &vulkanData, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanData);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(vulkanData, commandBuffer);
}
static VkSampler createSampler(const VulkanData &vulkanData, uint32_t mipLevel, VkFilter filterType, SamplerModes samplerModes, VkBorderColor borderColor, VkCompareOp compareOp)
{

    VkSampler sampler;
    // It is possible for shaders to read texels directly from images, but that is not very common when they are used as textures.
    // Textures are usually accessed through samplers, which will apply filtering and transformations to compute the final color that is retrieved.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // The magFilter and minFilter fields specify how to interpolate texels that are magnified or minified. Magnification concerns the oversampling problem describes above, and minification concerns undersampling.
    samplerInfo.magFilter = filterType;
    samplerInfo.minFilter = filterType;

    samplerInfo.addressModeU = samplerModes.modeU;
    samplerInfo.addressModeV = samplerModes.modeV;
    samplerInfo.addressModeW = samplerModes.modeW;
    // These two fields specify if anisotropic filtering should be used. There is no reason not to use this unless performance is a concern.
    // The maxAnisotropy field limits the amount of texel samples that can be used to calculate the final color. A lower value results in better performance, but lower quality results.
    samplerInfo.anisotropyEnable = VK_TRUE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vulkanData.physicalDevice, &properties);

    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    // You can either query the properties at the beginning of your program and pass them around to the functions that need them
    // The borderColor field specifies which color is returned when sampling beyond the image with clamp to border addressing mode
    samplerInfo.borderColor = borderColor;
    // The unnormalizedCoordinates field specifies which coordinate system you want to use to address texels in an image.
    // If this field is VK_TRUE, then you can simply use coordinates within the [0, texWidth) and [0, texHeight) range. If it is VK_FALSE, then the texels are addressed using the [0, 1) range on all axes.
    // Real-world applications almost always use normalized coordinates, because then it's possible to use textures of varying resolutions with the exact same coordinates.
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    // If a comparison function is enabled, then texels will first be compared to a value, and the result of that comparison is used in filtering operations. This is mainly used for percentage-closer filtering on shadow maps.
    samplerInfo.compareEnable = VK_FALSE;
    // samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.compareOp = compareOp;

    // All of these fields apply to mipmapping.
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevel);
    VK_CHECK(vkCreateSampler(vulkanData.device, &samplerInfo, nullptr, &sampler));

    return sampler;
}
static VkFormat findDepthFormat(const VkPhysicalDevice &physicalDevice)
{
    // All of these candidate formats contain a depth component, but the latter two also contain a stencil component.
    // We won't be using that yet, but we do need to take that into account when performing layout transitions on images with these formats.
    return findSupportedFormat(
        physicalDevice,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static float generateRandomFloat(float first, float second)
{
    static std::random_device rd;                                     // Seed for random number generator
    static std::mt19937 gen(rd());                                    // Mersenne Twister generator
    static std::uniform_real_distribution<float> dist(first, second); // Range [-1, 1]

    return dist(gen);
};
static glm::vec3 generateRandomPosition(float first, float second)
{

    float x = generateRandomFloat(first, second);
    float y = 0.0f;
    float z = generateRandomFloat(first, second);
    return {x, y, z};
}
static glm::vec3 generateDirection(float first, float second)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = generateRandomFloat(first, second);
    return {x, y, z};
}
static uint32_t getMemoryType(VulkanData &vulkanData, uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(vulkanData.physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                if (memTypeFound)
                {
                    *memTypeFound = true;
                }
                return i;
            }
        }
        typeBits >>= 1;
    }

    if (memTypeFound)
    {
        *memTypeFound = false;
        return 0;
    }
    else
    {
        throw std::runtime_error("Could not find a matching memory type");
    }
}
static VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat)
{
    // Since all depth formats may be optional, we need to find a suitable depth format to use
    // Start with the highest precision packed format
    std::vector<VkFormat> formatList = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM};

    for (auto &format : formatList)
    {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            *depthFormat = format;
            return true;
        }
    }

    return false;
}
static VkBool32 getSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat *depthStencilFormat)
{
    std::vector<VkFormat> formatList = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };

    for (auto &format : formatList)
    {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            *depthStencilFormat = format;
            return true;
        }
    }

    return false;
}

static QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice &device, const VkSurfaceKHR &surface)
{
    // QueueFamilyIndices represents queue indices
    QueueFamilyIndices indices{};

    uint32_t queueFamilyCount{0};
    // Get queue family count from physical device.
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    // Get queue properties of queues.
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    // Find the queue family that has the capability of presenting to our window surface
    VkBool32 presentSupport = false;

    // Find the indice that is capable to do graphic things
    int i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            indices.graphicsAndComputeFamily = i;
        }
        // Check the physicaldevice that supports presentation queue
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }
        i++;
    }

    return indices;
}

static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    // Just checking if a swap chain is available is not sufficient, because it may nat actually be competible with our surface.
    // There are basically three kind of properties we need to check
    // - Basic surface capabilites (min/max numbre of images in swap chain)
    // - Surface format (pixel format, color space)
    // - Available presentation modes
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModesCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, nullptr);
    if (presentModesCount != 0)
    {
        details.presentModes.resize(presentModesCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, details.presentModes.data());
    }

    return details;
}

static void insertImageMemoryBarrier(
    VkCommandBuffer cmdbuffer,
    VkImage image,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkImageSubresourceRange subresourceRange)
{
    VkImageMemoryBarrier imageMemoryBarrier = initializers::imageMemoryBarrier();
    imageMemoryBarrier.srcAccessMask = srcAccessMask;
    imageMemoryBarrier.dstAccessMask = dstAccessMask;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

static void createExternalBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkExternalMemoryHandleTypeFlagsKHR handleType, VkBuffer &buffer, VkDeviceMemory &bufferMemory, VulkanData &vulkanData)
{
    // Create buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Specify that the buffer will be used as external memory
    VkExternalMemoryBufferCreateInfoKHR externalBufferCreateInfo = {};
    externalBufferCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR;
    externalBufferCreateInfo.handleTypes = handleType;
    bufferInfo.pNext = &externalBufferCreateInfo;

    VK_CHECK(vkCreateBuffer(vulkanData.device, &bufferInfo, nullptr, &buffer));

    // Allocate memory for the buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vulkanData.device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = initializers::memoryAllocateInfo();
    allocInfo.allocationSize = memRequirements.size;

    // Specify that the memory will be used as external memory
    VkExportMemoryAllocateInfoKHR exportMemoryAllocateInfo = {};
    exportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
    exportMemoryAllocateInfo.pNext = NULL;
    exportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    allocInfo.pNext = &exportMemoryAllocateInfo;

    allocInfo.memoryTypeIndex = getMemoryType(vulkanData, memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(vulkanData.device, &allocInfo, nullptr, &bufferMemory));

    vkBindBufferMemory(vulkanData.device, buffer, bufferMemory, 0);
}
static void *getMemHandle(VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagBits handleType, VulkanData &vulkanData)
{
    int fd = -1;

    VkMemoryGetFdInfoKHR vkMemoryGetFdInfoKHR = {};
    vkMemoryGetFdInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    vkMemoryGetFdInfoKHR.pNext = NULL;
    vkMemoryGetFdInfoKHR.memory = memory;
    vkMemoryGetFdInfoKHR.handleType = handleType;

    PFN_vkGetMemoryFdKHR fpGetMemoryFdKHR;
    fpGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(vulkanData.device, "vkGetMemoryFdKHR");
    std::cout << "fpGetMemoryFdKHR: " << fpGetMemoryFdKHR << std::endl;
    if (!fpGetMemoryFdKHR)
    {
        throw std::runtime_error("Failed to retrieve vkGetMemoryWin32HandleKHR!");
    }
    if (fpGetMemoryFdKHR(vulkanData.device, &vkMemoryGetFdInfoKHR, &fd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to retrieve handle for buffer!");
    }
    return (void *)(uintptr_t)fd;
}

static void createBufferV(VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkBuffer &buffer,
                          VkDeviceMemory &bufferMemory, VulkanData &vulkanData)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vulkanData.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vulkanData.device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(vulkanData.physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vulkanData.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(vulkanData.device, buffer, bufferMemory, 0);
}

static void copyBufferV(VkBuffer dst, VkBuffer src, VkDeviceSize size, VulkanData &vulkanData)

{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanData);

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    endSingleTimeCommands(vulkanData, commandBuffer);
}

static void createExternalSemaphore(VkSemaphore &semaphore, VkExternalSemaphoreHandleTypeFlagBits handleType, VulkanData &vulkanData)
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkExportSemaphoreCreateInfoKHR exportSemaphoreCreateInfo = {};
    exportSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR;

    exportSemaphoreCreateInfo.pNext = NULL;
    exportSemaphoreCreateInfo.handleTypes = handleType;
    semaphoreInfo.pNext = &exportSemaphoreCreateInfo;

    if (vkCreateSemaphore(vulkanData.device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create synchronization objects for a CUDA-Vulkan!");
    }
}
static void *getSemaphoreHandle(VkSemaphore semaphore, VkExternalSemaphoreHandleTypeFlagBits handleType, VulkanData &vulkanData)
{
    int fd;

    VkSemaphoreGetFdInfoKHR semaphoreGetFdInfoKHR = {};
    semaphoreGetFdInfoKHR.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    semaphoreGetFdInfoKHR.pNext = NULL;
    semaphoreGetFdInfoKHR.semaphore = semaphore;
    semaphoreGetFdInfoKHR.handleType = handleType;

    PFN_vkGetSemaphoreFdKHR fpGetSemaphoreFdKHR;
    fpGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(vulkanData.device, "vkGetSemaphoreFdKHR");
    std::cout << "fpGetSemaphoreFdKHR: " << fpGetSemaphoreFdKHR << std::endl;
    if (!fpGetSemaphoreFdKHR)
    {
        throw std::runtime_error("Failed to retrieve vkGetMemoryWin32HandleKHR!");
    }
    if (fpGetSemaphoreFdKHR(vulkanData.device, &semaphoreGetFdInfoKHR, &fd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to retrieve handle for buffer!");
    }

    return (void *)(uintptr_t)fd;
}
#endif // HELPERFUNCTIONS_H
