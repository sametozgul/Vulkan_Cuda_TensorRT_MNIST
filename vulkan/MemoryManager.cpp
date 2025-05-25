#include "MemoryManager.h"
void MemoryManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    // Indicates for which purpose the data in the buffer is going to be used.
    // It is possible define multiple usage using bitwise operator.
    bufferInfo.usage = usage;
    // The buffer only will be used by graphic queue no need to be shared.
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vulkanData_.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }


    vkGetBufferMemoryRequirements(vulkanData_.device, buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT is selected because we need a memory that is visible from host (CPU)
    allocInfo.memoryTypeIndex = findMemoryType(vulkanData_.physicalDevice ,memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vulkanData_.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
    // After we create a memory, we must not associate this memory and with the buffer.
    // The fourth parameter is the offset within the region of memory. Since this memory is allocated specifically for this the vertex buffer, the offset is simply 0.
    // If the offset is non-zero, then it is required to be divisible by memRequirements.alignment.
    vkBindBufferMemory(vulkanData_.device, buffer, bufferMemory, 0);
}



void MemoryManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanData_);
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    endSingleTimeCommands(vulkanData_ ,commandBuffer);
}
