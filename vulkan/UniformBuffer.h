#ifndef UNIFORMBUFFER_H
#define UNIFORMBUFFER_H

// In the next chapter we'll specify the buffer that contains the UBO data for the shader, but we need to create this buffer first.
// We're going to copy new data to the uniform buffer every frame, so it doesn't really make any sense to have a staging buffer.

// We should have multiple buffers, because multiple frames may be in flight at the same time and we don't want to update the buffer in preparation of the next frame while a previous one is still reading from it!
// Thus, we need to have as many uniform buffers as we have frames in flight, and write to a uniform buffer that is not currently being read by the GPU
#include "Context.h"
#include "MemoryManager.h"

template<typename T>
class UniformBuffer: public MemoryManager
{
public:
    UniformBuffer() = default;
    void init(const VulkanData &vulkanData);
    void updateData(const T& data);
    void cleanUp();
    VkBuffer buffers;
    VkDeviceMemory memory;
    void* mapped;
    VkDescriptorBufferInfo descriptor{} ;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
private:
    VkDeviceSize bufferSize = sizeof(T);
    void setupDescriptor(VkDeviceSize offset){
        descriptor.buffer = buffers;
        descriptor.offset = offset;
        descriptor.range = bufferSize;
    }

};

template <typename T>
void UniformBuffer<T>::init(const VulkanData &vulkanData)
{
    vulkanData_ = vulkanData;

    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffers, memory);

    vkMapMemory(vulkanData_.device, memory, 0, bufferSize, 0, &mapped);

    setupDescriptor(0);

}

template <typename T>
void UniformBuffer<T>::updateData(const T& data)
{

    memcpy(mapped, &data, sizeof(data));

    // setupDescriptor(sizeof(data),0);
}

template <typename T>
void UniformBuffer<T>::cleanUp()
{
    vkDeviceWaitIdle(vulkanData_.device);  // Ensure no operations are pending
    vkDestroyBuffer(vulkanData_.device, buffers, nullptr);
    vkFreeMemory(vulkanData_.device, memory, nullptr);
}





#endif // UNIFORMBUFFER_H
