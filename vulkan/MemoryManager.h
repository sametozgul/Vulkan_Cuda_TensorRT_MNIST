#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include "HelperFunctions.h"

class MemoryManager
{
protected:
    MemoryManager() = default;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    VulkanData vulkanData_;

public:
    VkMemoryRequirements memRequirements;
};

#endif // MEMORYMANAGER_H
