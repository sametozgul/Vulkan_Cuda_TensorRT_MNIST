#ifndef BUFFER_H
#define BUFFER_H

#endif // BUFFER_H
#include "Context.h"
#include "MemoryManager.h"
#include <cstring>
class Buffer :public MemoryManager
{
public:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorBufferInfo descriptor;
    VkDeviceSize size = 0;
    VkDeviceSize aligment = 0;
    void *mapped = nullptr;
    VkBufferUsageFlags usageFlags;

    template <typename T>
    void create(const VulkanData &vulkanData, T& t){
        vulkanData_ = vulkanData;
        VkDeviceSize bufferSize = sizeof(t[0]) * t.size();

        createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     buffer, memory);
        map();
        memcpy(mapped, t.data(), size);
        unmap();
        setupDescriptor();

    }
    void create(const VulkanData &vulkanData, VkBufferUsageFlags usage,  VkMemoryPropertyFlags memFlags, VkDeviceSize size, void*data = nullptr){
        vulkanData_ = vulkanData;
        unmap();
        cleanUp();
        createBuffer(size, usage, memFlags, buffer, memory);
        if (data != nullptr)
        {
            VK_CHECK(map());
            memcpy(mapped, data, size);
            if ((memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
                flush();

            unmap();
        }
        setupDescriptor();

    }
    void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0){
        descriptor.offset = offset;
        descriptor.buffer = buffer;
        descriptor.range = size;
    }
    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0){
        return vkMapMemory(vulkanData_.device, memory, offset, size, 0, &mapped);
    }
    void unmap()
    {
        if (mapped)
        {
            vkUnmapMemory(vulkanData_.device, memory);
            mapped = nullptr;
        }
    }
    void copyTo(void* data, VkDeviceSize size){
        assert(mapped);
        memcpy(mapped, data, size);
    }
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0){
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkFlushMappedMemoryRanges(vulkanData_.device, 1, &mappedRange);
    }
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0){
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkInvalidateMappedMemoryRanges(vulkanData_.device, 1, &mappedRange);
    }
    void cleanUp(){
        if (buffer)
        {
            vkDestroyBuffer(vulkanData_.device, buffer, nullptr);
        }
        if (memory)
        {
            vkFreeMemory(vulkanData_.device, memory, nullptr);
        }
    }


};
