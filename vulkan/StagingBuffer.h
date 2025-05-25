#ifndef STAGINGBUFFER_H
#define STAGINGBUFFER_H

#include "MemoryManager.h"

class StagingBuffer:public MemoryManager
{
public:
    template <typename T>
    void create(const VulkanData &vulkanData, const T &t, VkBufferUsageFlags usage, VkDeviceSize bufferSize){

        vulkanData_ = vulkanData;
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vulkanData_.device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, t.data(), (size_t) bufferSize);
        vkUnmapMemory(vulkanData_.device, stagingBufferMemory);

        createBuffer(bufferSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);

        copyBuffer(stagingBuffer, buffer, bufferSize);

        vkDestroyBuffer(vulkanData_.device, stagingBuffer, nullptr);
        vkFreeMemory(vulkanData_.device, stagingBufferMemory, nullptr);
    }

    void cleanUp(){
        vkDestroyBuffer(vulkanData_.device, buffer, nullptr);
        vkFreeMemory(vulkanData_.device, bufferMemory, nullptr);
    }

    void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0){
        descriptor.offset = offset;
        descriptor.buffer = buffer;
        descriptor.range = size;
    }
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;
    VkDescriptorBufferInfo descriptor;
};

#endif // STAGINGBUFFER_H
