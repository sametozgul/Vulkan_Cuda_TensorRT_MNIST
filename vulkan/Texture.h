#ifndef TEXTURE_H
#define TEXTURE_H

// Adding a texture to our application will involve the following steps:
// - Create an image object backed by device memory
// - Fill it with pixels from an image file
// - Create an image sampler
// - Add a combined image sampler descriptor to sample colors from the texture

// Creating an image and filling it with data is similar to vertex buffer creation
// Although it is possible to create a staging image for this purpose, Vulkan also allows you to copy pixels from a VkBuffer to an image and the API for this is actually faster on some hardware.
// We'll first create this buffer and fill it with pixel values, and then we'll create an image to copy the pixels to. Creating an image is not very different from creating buffers.
// It involves querying the memory requirements, allocating device memory and binding it, just like we've seen before.

// One of the most common ways to transition the layout of an image is a pipeline barrier. Pipeline barriers are primarily used for synchronizing access to resources, like making sure that an image was written to before it is read, but they can also be used to transition layouts.

#include "Context.h"
#include "MemoryManager.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include <tiny_gltf.h>
#include <functional>
#include "cuda_runtime.h"
class Texture : public MemoryManager
{
public:
    Texture() = default;
    void cleanUp();
    void loadFromGltfImage(const VulkanData &vulkanData, tinygltf::Image &gltfimage, std::string path);
    void loadImageFromFile(const VulkanData &vulkanData, const std::string &imageName, VkFormat format, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout, bool forceLinear = false);
    void loadImageFromFileCubeMap(const VulkanData &vulkanData, const std::string &imageName, VkFormat format, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout);
    void loadImage(const VulkanData &vulkanData, void *buffer, VkDeviceSize bufferSize, int width, int height, uint32_t mipLevel);
    void emptyTexture(const VulkanData &vulkanData);
    void empty3DTexture(const VulkanData &vulkanData, unsigned char *buffer);
    void loadImageData(const std::string &filePath, VulkanData &vulkanData,  cudaTextureObject_t& textureObjMipMapInput, 
                       std::function<void(unsigned int, unsigned int, unsigned int, size_t, VkDeviceMemory &, cudaTextureObject_t&)> importImageMemFunc);
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    VkDescriptorImageInfo descriptor;
    void updateDescriptor();
    const VkDescriptorImageInfo& getImageDescriptor() const { return descriptor; }
    uint32_t width = 1, height = 1;
    uint32_t mipLevels = 1;
    uint32_t layerCount = 1;
    uint32_t index;
    uint32_t depth = 4;
    size_t totalImageMemSize;
    VkImageLayout imageLayout;
    unsigned int *image_data = NULL;
    std::string execution_path;

private:
    // void copyBufferToImage(VkBuffer buffer, std::vector<VkBufferImageCopy> bufferCopyRegions);
    ktxResult loadKTXFile(std::string filename, ktxTexture **target);
    void createTextureImage(unsigned int imageWidth, unsigned int imageHeight);
};

#endif // TEXTURE_H
