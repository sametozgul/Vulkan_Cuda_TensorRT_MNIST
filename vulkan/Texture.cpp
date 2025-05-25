#include "Texture.h"
#include "Controllers.h"
#include "Initializers.h"
#include <cstring>
#include "helper_string.h"
#include "helper_image.h"


void Texture::cleanUp()
{
    vkDestroySampler(vulkanData_.device, sampler, nullptr);
    vkDestroyImageView(vulkanData_.device, view, nullptr);
    vkDestroyImage(vulkanData_.device, image, nullptr);
    vkFreeMemory(vulkanData_.device, memory, nullptr);
}

void Texture::loadFromGltfImage(const VulkanData &vulkanData, tinygltf::Image &gltfimage, std::string path)
{
    vulkanData_ = vulkanData;
    bool isKtx = false;
    // Image points to an external ktx file
    if (gltfimage.uri.find_last_of(".") != std::string::npos)
    {
        if (gltfimage.uri.substr(gltfimage.uri.find_last_of(".") + 1) == "ktx")
        {
            isKtx = true;
        }
    }

    VkFormat format;

    if (!isKtx)
    {
        unsigned char *buffer = nullptr;
        VkDeviceSize bufferSize = 0;
        bool deleteBuffer = false;
        if (gltfimage.component == 3)
        {
            // Most devices don't support RGB only on Vulkan so convert if necessary
            // TODO: Check actual format support and transform only if required
            bufferSize = gltfimage.width * gltfimage.height * 4;
            buffer = new unsigned char[bufferSize];
            unsigned char *rgba = buffer;
            unsigned char *rgb = &gltfimage.image[0];
            for (size_t i = 0; i < gltfimage.width * gltfimage.height; ++i)
            {
                for (int32_t j = 0; j < 3; ++j)
                {
                    rgba[j] = rgb[j];
                }
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else
        {
            buffer = &gltfimage.image[0];
            bufferSize = gltfimage.image.size();
        }

        VkFormatProperties formatProperties;
        format = VK_FORMAT_R8G8B8A8_UNORM;
        width = gltfimage.width;
        height = gltfimage.height;
        mipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1.0);
        vkGetPhysicalDeviceFormatProperties(vulkanData_.physicalDevice, format, &formatProperties);
        assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT);

        assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);
        uint8_t *data;
        vkMapMemory(vulkanData_.device, stagingMemory, 0, memRequirements.size, 0, (void **)&data);
        memcpy(data, buffer, static_cast<size_t>(bufferSize));
        vkUnmapMemory(vulkanData_.device, stagingMemory);

        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = {width, height, 1};
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        image = createImage(vulkanData_, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory);

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanData);
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        VkImageMemoryBarrier imageMemoryBarrier{};

        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = width;
        bufferCopyRegion.imageExtent.height = height;
        bufferCopyRegion.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        endSingleTimeCommands(vulkanData_, commandBuffer);

        vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
        vkFreeMemory(vulkanData_.device, stagingMemory, nullptr);

        VkCommandBuffer blitCmd = beginSingleTimeCommands(vulkanData_);
        for (uint32_t i = 1; i < mipLevels; i++)
        {
            VkImageBlit imageBlit{};

            imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.srcSubresource.layerCount = 1;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = int32_t(width >> (i - 1));
            imageBlit.srcOffsets[1].y = int32_t(height >> (i - 1));
            imageBlit.srcOffsets[1].z = 1;

            imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.dstSubresource.layerCount = 1;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = int32_t(width >> i);
            imageBlit.dstOffsets[1].y = int32_t(height >> i);
            imageBlit.dstOffsets[1].z = 1;

            VkImageSubresourceRange mipSubRange = {};
            mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipSubRange.baseMipLevel = i;
            mipSubRange.levelCount = 1;
            mipSubRange.layerCount = 1;

            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = 0;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.image = image;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }

            vkCmdBlitImage(blitCmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageMemoryBarrier.image = image;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }
        }

        subresourceRange.levelCount = mipLevels;
        imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        if (deleteBuffer)
        {
            delete[] buffer;
        }
        endSingleTimeCommands(vulkanData_, blitCmd);
    }
    else
    {
        // Texture is stored in an external ktx file
        std::string filename = path + "/" + gltfimage.uri;

        ktxTexture *ktxTexture;

        ktxResult result = KTX_SUCCESS;

        if (!Controller::isFileExists(filename))
        {
            Controller::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
        }
        result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

        assert(result == KTX_SUCCESS);

        width = ktxTexture->baseWidth;
        height = ktxTexture->baseHeight;
        mipLevels = ktxTexture->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);
        format = ktxTexture_GetVkFormat(ktxTexture);

        // Get device properties for the requested texture format
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(vulkanData_.physicalDevice, format, &formatProperties);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        createBuffer(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

        uint8_t *data;
        VK_CHECK(vkMapMemory(vulkanData_.device, stagingMemory, 0, memRequirements.size, 0, (void **)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(vulkanData_.device, stagingMemory);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t i = 0; i < mipLevels; i++)
        {
            ktx_size_t offset;
            KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
            assert(result == KTX_SUCCESS);
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
            bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = {width, height, 1};
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        image = createImage(vulkanData_, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.layerCount = 1;

        transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        copyBufferToImage(vulkanData_, image, stagingBuffer, bufferCopyRegions);

        vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
        vkFreeMemory(vulkanData_.device, stagingMemory, nullptr);

        ktxTexture_Destroy(ktxTexture);
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxLod = (float)mipLevels;
    samplerInfo.maxAnisotropy = 8.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;

    SamplerModes samplerModes{};
    samplerModes.modeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerModes.modeV = samplerModes.modeU;
    samplerModes.modeW = samplerModes.modeU;
    sampler = createSampler(vulkanData_, mipLevels, VK_FILTER_LINEAR, samplerModes, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_COMPARE_OP_NEVER);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.levelCount = mipLevels;

    view = createImageView(vulkanData_.device, viewInfo);
    imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = imageLayout;
}

void Texture::loadImageFromFile(const VulkanData &vulkanData, const std::string &imageName, VkFormat format, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout, bool forceLinear)
{
    vulkanData_ = vulkanData;
    ktxTexture *ktxTexture;
    ktxResult result = loadKTXFile(imageName, &ktxTexture);
    assert(result == KTX_SUCCESS);

    width = ktxTexture->baseWidth;
    height = ktxTexture->baseHeight;
    mipLevels = ktxTexture->numLevels;

    ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
    ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(vulkanData_.physicalDevice, format, &formatProperties);

    VkBool32 useStaging = !forceLinear;

    if (useStaging)
    {
        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        createBuffer(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(vulkanData_.device, stagingMemory, 0, memRequirements.size, 0, (void **)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(vulkanData_.device, stagingMemory);

        // Setup buffer copy regions for each mip level
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t i = 0; i < mipLevels; i++)
        {
            ktx_size_t offset;
            KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
            assert(result == KTX_SUCCESS);

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
            bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = {width, height, 1};
        imageCreateInfo.usage = imageUsageFlags;

        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VkExternalMemoryImageCreateInfo vkExternalMemImageCreateInfo = {};
        vkExternalMemImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        vkExternalMemImageCreateInfo.pNext = NULL;
        vkExternalMemImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        imageCreateInfo.pNext = &vkExternalMemImageCreateInfo;

        image = createImage(vulkanData_, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory, true);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.layerCount = 1;

        transitionImageLayout(vulkanData_,
                               image,  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

        copyBufferToImage(vulkanData_, image, stagingBuffer, bufferCopyRegions);
        // Copy mip levels from staging buffer

        generateMipmaps(vulkanData_, image, VK_FORMAT_R8G8B8A8_UNORM, width, height, mipLevels);

        // Clean up staging resources
        vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
        vkFreeMemory(vulkanData_.device, stagingMemory, nullptr);
    }
    else
    {
        // Prefer using optimal tiling, as linear tiling
        // may support only a small set of features
        // depending on implementation (e.g. no mip maps, only one layer, etc.)

        // Check if this support is supported for linear tiling
        assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

        VkImage mappableImage;
        VkDeviceMemory mappableMemory;

        VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent = {width, height, 1};
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imageCreateInfo.usage = imageUsageFlags;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        mappableImage = createImage(vulkanData_, imageCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mappableMemory);

        // Get sub resource layout
        // Mip map count, array layer, etc.
        VkImageSubresource subRes = {};
        subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subRes.mipLevel = 0;

        VkSubresourceLayout subResLayout;
        void *data;

        // Get sub resources layout
        // Includes row pitch, size offsets, etc.
        vkGetImageSubresourceLayout(vulkanData_.device, mappableImage, &subRes, &subResLayout);

        // Map image memory
        VK_CHECK(vkMapMemory(vulkanData_.device, mappableMemory, 0, memRequirements.size, 0, &data));

        // Copy image data into memory
        memcpy(data, ktxTextureData, memRequirements.size);

        vkUnmapMemory(vulkanData_.device, mappableMemory);

        // Linear tiled images don't need to be staged
        // and can be directly used as textures
        image = mappableImage;
        memory = mappableMemory;
        this->imageLayout = imageLayout;

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.layerCount = 1;

        transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout);
    }

    ktxTexture_Destroy(ktxTexture);

    // Create a default sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = (useStaging) ? (float)mipLevels : 0.0f;
    // Only enable anisotropic filtering if enabled on the device
    samplerCreateInfo.maxAnisotropy = 1.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(vulkanData_.device, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    // Textures are not directly accessed by the shaders and
    // are abstracted by image views containing additional
    // information and sub resource ranges
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    // Linear tiling usually won't support mip maps
    // Only set mip map count if optimal tiling is used
    viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mipLevels : 1;
    viewCreateInfo.image = image;
    VK_CHECK(vkCreateImageView(vulkanData_.device, &viewCreateInfo, nullptr, &view));

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void Texture::loadImageFromFileCubeMap(const VulkanData &vulkanData, const std::string &imageName, VkFormat format, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    vulkanData_ = vulkanData;
    ktxTexture *ktxTexture;
    ktxResult result = loadKTXFile(imageName, &ktxTexture);
    assert(result == KTX_SUCCESS);

    uint32_t width, height, mipLevels;

    width = ktxTexture->baseWidth;
    height = ktxTexture->baseHeight;
    mipLevels = ktxTexture->numLevels;

    ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
    ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);
    // Copy texture data into staging buffer
    uint8_t *data;
    VK_CHECK(vkMapMemory(vulkanData_.device, stagingMemory, 0, memRequirements.size, 0, (void **)&data));
    memcpy(data, ktxTextureData, ktxTextureSize);
    vkUnmapMemory(vulkanData_.device, stagingMemory);

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = {width, height, 1};
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // Cube faces count as array layers in Vulkan
    imageCreateInfo.arrayLayers = 6;
    // This flag is required for cube map images
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    image = createImage(vulkanData_, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory);

    // Setup buffer copy regions for each face including all of its miplevels
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    for (uint32_t face = 0; face < 6; face++)
    {
        for (uint32_t level = 0; level < mipLevels; level++)
        {
            // Calculate offset into staging buffer for the current mip level and face
            ktx_size_t offset;
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
            assert(ret == KTX_SUCCESS);
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = face;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
            bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }
    }

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 6;

    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(vulkanData_, image, stagingBuffer, bufferCopyRegions);
    imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout);

    // Create sampler
    VkSamplerCreateInfo samplerInfo = initializers::samplerCreateInfo();
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VK_CHECK(vkCreateSampler(vulkanData_.device, &samplerInfo, nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewInfo = initializers::imageViewCreateInfo();
    // Cube map view type
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    // 6 array layers (faces)
    viewInfo.subresourceRange.layerCount = 6;
    // Set number of mip levels
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.image = image;
    VK_CHECK(vkCreateImageView(vulkanData_.device, &viewInfo, nullptr, &view));

    // Clean up staging resources
    vkFreeMemory(vulkanData_.device, stagingMemory, nullptr);
    vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
    ktxTexture_Destroy(ktxTexture);
    updateDescriptor();
}

void Texture::loadImage(const VulkanData &vulkanData, void *buffer, VkDeviceSize bufferSize, int width, int height, uint32_t mipLevel)
{
    vulkanData_ = vulkanData;
    mipLevels = mipLevel;
    // Create a host visible memory so that we can use vkMapMemeory and copy the pixels to it
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    // The buffer should be in host visible memory so that we can map it and it should be usable as a transfer source
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void *data;
    vkMapMemory(vulkanData_.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, buffer, static_cast<size_t>(bufferSize));
    vkUnmapMemory(vulkanData_.device, stagingBufferMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    image = createImage(vulkanData_, imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory);
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    std::vector<VkBufferImageCopy> bufferCopyRegions;

    for (uint32_t i = 0; i < mipLevels; i++)
    {
        VkBufferImageCopy region{};
        // The bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory.
        // For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates that the pixels are simply tightly packed like they are in our case. The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels.
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1};
        bufferCopyRegions.push_back(region);
    }

    copyBufferToImage(vulkanData_, image, stagingBuffer, bufferCopyRegions);
    // transitionImageLayout(vulkanData_, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevel_);

    vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
    vkFreeMemory(vulkanData_.device, stagingBufferMemory, nullptr);

    generateMipmaps(vulkanData_, image, VK_FORMAT_R8G8B8A8_SRGB, width, height, mipLevel);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevel;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.image = image;

    view = createImageView(vulkanData_.device, viewInfo);
    SamplerModes samplerModes{};
    samplerModes.modeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerModes.modeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerModes.modeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    sampler = createSampler(vulkanData_, mipLevels, VK_FILTER_LINEAR, samplerModes, VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_COMPARE_OP_ALWAYS);

    updateDescriptor();
}

void Texture::emptyTexture(const VulkanData &vulkanData)
{
    vulkanData_ = vulkanData;

    width = 1;
    height = 1;
    layerCount = 1;
    mipLevels = 1;

    size_t bufferSize = width * height * 4;
    unsigned char *buffer = new unsigned char[bufferSize];
    memset(buffer, 0, bufferSize);

    // Create a host visible memory so that we can use vkMapMemeory and copy the pixels to it
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    // The buffer should be in host visible memory so that we can map it and it should be usable as a transfer source
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void *data;
    vkMapMemory(vulkanData_.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, buffer, static_cast<size_t>(bufferSize));
    vkUnmapMemory(vulkanData_.device, stagingBufferMemory);

    VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = {width, height, 1};
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    image = createImage(vulkanData_, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory);
    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;

    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    std::vector<VkBufferImageCopy> bufferCopyRegions;

    for (uint32_t i = 0; i < mipLevels; i++)
    {
        VkBufferImageCopy region{};
        // The bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory.
        // For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates that the pixels are simply tightly packed like they are in our case. The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels.
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1};
        bufferCopyRegions.push_back(region);
    }

    copyBufferToImage(vulkanData_, image, stagingBuffer, bufferCopyRegions);
    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
    vkFreeMemory(vulkanData_.device, stagingBufferMemory, nullptr);
    imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // generateMipmaps(vulkanData_, image, VK_FORMAT_R8G8B8A8_SRGB, width, height, mipLevels);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.image = image;

    view = createImageView(vulkanData_.device, viewInfo);
    VkSamplerCreateInfo samplerCreateInfo = initializers::samplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    VK_CHECK(vkCreateSampler(vulkanData_.device, &samplerCreateInfo, nullptr, &sampler));

    updateDescriptor();
}

void Texture::empty3DTexture(const VulkanData &vulkanData, unsigned char *buffer)
{
    const size_t bufferSize = width * height * depth;
    vulkanData_ = vulkanData;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

    void *data;
    vkMapMemory(vulkanData_.device, stagingMemory, 0, bufferSize, 0, &data);
    memcpy(data, buffer, static_cast<size_t>(bufferSize));
    vkUnmapMemory(vulkanData_.device, stagingMemory);

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;

    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    std::vector<VkBufferImageCopy> bufferCopyRegions;

    for (uint32_t i = 0; i < mipLevels; i++)
    {
        VkBufferImageCopy region{};
        // The bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory.
        // For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates that the pixels are simply tightly packed like they are in our case. The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels.
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            static_cast<uint32_t>(depth)};
        bufferCopyRegions.push_back(region);
    }

    copyBufferToImage(vulkanData_, image, stagingBuffer, bufferCopyRegions);

    imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout);

    delete[] buffer;
    vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
    vkFreeMemory(vulkanData_.device, stagingMemory, nullptr);
}

void Texture::loadImageData(const std::string &filePath, VulkanData &vulkanData,  cudaTextureObject_t& textureObjMipMapInput, std::function<void(unsigned int, unsigned int, unsigned int, size_t, VkDeviceMemory &, cudaTextureObject_t&)> importImageMemFunc)
{
    // load image (needed so we can get the width and height before we create
    // the window
    vulkanData_ = vulkanData;

    unsigned int imageWidth, imageHeight;

    char *image_path = sdkFindFilePath(filePath.c_str(), execution_path.c_str());

    if (image_path == 0)
    {
        printf("Error finding image file '%s'\n", filePath.c_str());
        exit(EXIT_FAILURE);
    }

    sdkLoadPPM4(image_path, (unsigned char **)&image_data, &imageWidth, &imageHeight);

    if (!image_data)
    {
        printf("Error opening file '%s'\n", image_path);
        exit(EXIT_FAILURE);
    }

    printf("Loaded '%s', %d x %d pixels\n", image_path, imageWidth, imageHeight);

    width = imageWidth;
    height = imageHeight;
    createTextureImage(imageWidth, imageHeight);
    importImageMemFunc(mipLevels, imageWidth, imageHeight, totalImageMemSize, memory, textureObjMipMapInput);
}

void Texture::createTextureImage(unsigned int imageWidth, unsigned int imageHeight)
{
    VkDeviceSize imageSize = imageWidth * imageHeight * 4;
    mipLevels = 1;

    if (!image_data)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingBufferMemory);

    void *data;
    vkMapMemory(vulkanData_.device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, image_data, static_cast<size_t>(imageSize));
    vkUnmapMemory(vulkanData_.device, stagingBufferMemory);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = imageWidth;
    imageInfo.extent.height = imageHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = {imageWidth, imageHeight, 1};
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    image = createImage(vulkanData_, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory, true);

    VkMemoryRequirements vkMemoryRequirements = {};
    vkGetImageMemoryRequirements(vulkanData_.device, image, &vkMemoryRequirements);
    totalImageMemSize = vkMemoryRequirements.size;

    transitionImageLayout(vulkanData_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    copyBufferToImage(vulkanData_,
                       stagingBuffer, image, static_cast<uint32_t>(imageWidth), static_cast<uint32_t>(imageWidth));

    

    vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
    vkFreeMemory(vulkanData_.device, stagingBufferMemory, nullptr);

    generateMipmaps(vulkanData_, image, VK_FORMAT_R8G8B8A8_UNORM, imageWidth, imageHeight, mipLevels);

    // Create a default sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod = 0.0f;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = static_cast<float>(mipLevels);
    // Only enable anisotropic filtering if enabled on the device
    samplerCreateInfo.maxAnisotropy = 16.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VK_CHECK(vkCreateSampler(vulkanData_.device, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    // Textures are not directly accessed by the shaders and
    // are abstracted by image views containing additional
    // information and sub resource ranges
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.image = image;
    VK_CHECK(vkCreateImageView(vulkanData_.device, &viewCreateInfo, nullptr, &view));

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void Texture::updateDescriptor()
{
    descriptor.sampler = sampler;

    descriptor.imageView = view;
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

ktxResult Texture::loadKTXFile(std::string filename, ktxTexture **target)
{
    ktxResult result = KTX_SUCCESS;
    if (!Controller::isFileExists(filename))
    {
        Controller::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
    }
    result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
    return result;
}
