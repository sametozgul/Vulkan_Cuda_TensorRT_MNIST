
#include "Renderer.h"
#include "VulkanImageCuda.h"
#include "helper_cuda.h"
#include <chrono>
#include "Context.h"
#include <functional>
#include "Texture.h"
#include <functional>
#include <string>
#include "TensorRTManager.h"
class CudaManager
{
    VulkanData vulkanData;
    VulkanImageCuda vulkanImageCuda;

    // It is used only creating cuda images...

    cudaTextureObject_t textureObjMipMapInput_ = 0;
    std::vector<cudaTextureObject_t> textureObjMipMaps;

    cudaExternalSemaphore_t cudaExtCudaUpdateVkSemaphore;
    cudaExternalSemaphore_t cudaExtVkUpdateCudaSemaphore;

    VkSemaphore cudaUpdateVkSemaphore, vkUpdateCudaSemaphore;

private:
    std::unique_ptr<TensorRTManager> tensorRTManager = nullptr;

public:
    // Texture texture{};
    cudaStream_t stream;
    std::vector<Texture> textures;

    int getDetection() const { return tensorRTManager->getPrediction(); }
    CudaManager(VulkanData vulkandata, uint32_t imageCount)
        : vulkanData(vulkandata),
          stream(0),
          vulkanImageCuda(1)
    {
        int cuda_device = -1;
        // This tell cuda which Vulkan device to use. Both Vulkan and CUDA should use the same device.
        cuda_device = vulkanImageCuda.initCuda(vulkanData.vkDeviceUUID, VK_UUID_SIZE);
        if (cuda_device == -1)
        {
            printf("Error: No CUDA-Vulkan interop capable device found\n");
            exit(EXIT_FAILURE);
        }
        // A Cuda stream is a sequence of operations that execute in order on the device.
        // The default stream is a special stream that is created by default and is used for all operations that do not specify a stream.
        // The default stream is also known as the "null stream" or "stream 0".
        // The default stream is not a real stream, but rather a special case that allows for easier programming.
        checkCudaErrors(cudaStreamCreate(&stream));

        textures.resize(imageCount);
        textureObjMipMaps.resize(imageCount);
        for (int i = 0; i < imageCount; ++i)
        {
            textures[i].loadImageData("textures/digit_rgba" +std::to_string(i) + ".ppm", vulkanData, textureObjMipMaps[i],
                                      [this](unsigned int mipLevels, unsigned int imageWidth, unsigned int imageHeight, size_t totalImageMemSize, VkDeviceMemory &textureImageMemory, cudaTextureObject_t &textureObjMipMapInput)
                                      {
                                          this->cudaVkImportImageMem(mipLevels, imageWidth, imageHeight, totalImageMemSize, textureImageMemory, textureObjMipMapInput);
                                      });
        }

        createSyncObjectsExt();
        cudaVkImportSemaphore();

        tensorRTManager = std::make_unique<TensorRTManager>(stream);
    }

    ~CudaManager()
    {

        vkDestroySemaphore(vulkanData.device, cudaUpdateVkSemaphore, nullptr);
        vkDestroySemaphore(vulkanData.device, vkUpdateCudaSemaphore, nullptr);
        for(auto& texture: textures) texture.cleanUp();
        std::cout << "CudaManager destroyed" << std::endl;
    }
    void init()
    {
    }
    void cudaVkImportImageMem(unsigned int mipLevels, unsigned int imageWidth, unsigned int imageHeight, size_t totalImageMemSize, VkDeviceMemory &textureImageMemory, cudaTextureObject_t &textureObjMipMapInput)
    {
        cudaExternalMemory_t cudaExtMemImageBuffer;
        cudaMipmappedArray_t cudaMipmappedImageArray, cudaMipmappedImageArrayTemp, cudaMipmappedImageArrayOrig;
        // Describes a memory resource that cuda will import
        cudaExternalMemoryHandleDesc cudaExtMemHandleDesc;

        memset(&cudaExtMemHandleDesc, 0, sizeof(cudaExtMemHandleDesc));

        cudaExtMemHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueFd;
        // Gets a Linux file descriptor(fd) from the vulkan image memory. This is done using Vulkan's external memory API.
        // OpaqueFd is a Linux specific handle type that allows for sharing memory between processes.
        cudaExtMemHandleDesc.handle.fd = (int)(uintptr_t)getMemHandle(textureImageMemory, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR, vulkanData);

        cudaExtMemHandleDesc.size = totalImageMemSize;
        // This registers the Vulkan memory with Cuda.
        // Cuda now has access to the Vulkan memory and can use it for CUDA operations.
        checkCudaErrors(cudaImportExternalMemory(&cudaExtMemImageBuffer, &cudaExtMemHandleDesc));

        // This creates a mipmapped array from the imported Vulkan memory.
        cudaExternalMemoryMipmappedArrayDesc externalMemoryMipmappedArrayDesc;

        memset(&externalMemoryMipmappedArrayDesc, 0, sizeof(externalMemoryMipmappedArrayDesc));

        cudaExtent extent = make_cudaExtent(imageWidth, imageHeight, 0);
        cudaChannelFormatDesc formatDesc;
        formatDesc.x = 8;
        formatDesc.y = 8;
        formatDesc.z = 8;
        formatDesc.w = 8;
        formatDesc.f = cudaChannelFormatKindUnsigned;

        externalMemoryMipmappedArrayDesc.offset = 0;
        externalMemoryMipmappedArrayDesc.formatDesc = formatDesc;
        externalMemoryMipmappedArrayDesc.extent = extent;
        externalMemoryMipmappedArrayDesc.flags = 0;
        externalMemoryMipmappedArrayDesc.numLevels = mipLevels;
        // cudaMipmappedImageArray: A handle for this structure in CUDA.
        // This maps the Vulkan image memory into a CUDA mipmapped array.
        checkCudaErrors(cudaExternalMemoryGetMappedMipmappedArray(
            &cudaMipmappedImageArray, cudaExtMemImageBuffer, &externalMemoryMipmappedArrayDesc));
        // Allocates two mipmapped arrays for the CUDA kernel to use.

        checkCudaErrors(cudaMallocMipmappedArray(&cudaMipmappedImageArrayTemp, &formatDesc, extent, mipLevels));
        checkCudaErrors(cudaMallocMipmappedArray(&cudaMipmappedImageArrayOrig, &formatDesc, extent, mipLevels));

        for (int mipLevelIdx = 0; mipLevelIdx < mipLevels; mipLevelIdx++)
        {
            cudaArray_t cudaMipLevelArray, cudaMipLevelArrayTemp, cudaMipLevelArrayOrig;

            checkCudaErrors(cudaGetMipmappedArrayLevel(&cudaMipLevelArray, cudaMipmappedImageArray, mipLevelIdx));
            checkCudaErrors(
                cudaGetMipmappedArrayLevel(&cudaMipLevelArrayTemp, cudaMipmappedImageArrayTemp, mipLevelIdx));
            checkCudaErrors(
                cudaGetMipmappedArrayLevel(&cudaMipLevelArrayOrig, cudaMipmappedImageArrayOrig, mipLevelIdx));

            uint32_t width = (imageWidth >> mipLevelIdx) ? (imageWidth >> mipLevelIdx) : 1;
            uint32_t height = (imageHeight >> mipLevelIdx) ? (imageHeight >> mipLevelIdx) : 1;
            checkCudaErrors(cudaMemcpy2DArrayToArray(cudaMipLevelArrayOrig,
                                                     0,
                                                     0,
                                                     cudaMipLevelArray,
                                                     0,
                                                     0,
                                                     width * sizeof(uchar4),
                                                     height,
                                                     cudaMemcpyDeviceToDevice));
        }

        cudaResourceDesc resDescr;
        memset(&resDescr, 0, sizeof(cudaResourceDesc));

        resDescr.resType = cudaResourceTypeMipmappedArray;
        resDescr.res.mipmap.mipmap = cudaMipmappedImageArrayOrig;

        cudaTextureDesc texDescr;
        memset(&texDescr, 0, sizeof(cudaTextureDesc));

        texDescr.normalizedCoords = true;
        texDescr.filterMode = cudaFilterModeLinear;
        texDescr.mipmapFilterMode = cudaFilterModeLinear;

        texDescr.addressMode[0] = cudaAddressModeWrap;
        texDescr.addressMode[1] = cudaAddressModeWrap;

        texDescr.maxMipmapLevelClamp = float(mipLevels - 1);

        texDescr.readMode = cudaReadModeNormalizedFloat;
        // Allows CUDA kernel to read from a texture with interpolation
        checkCudaErrors(cudaCreateTextureObject(&textureObjMipMapInput, &resDescr, &texDescr, NULL));

        printf("CUDA Kernel Vulkan image buffer\n");
    }
    void cudaUpdateVkImage(uint32_t imageIndex)
    {
        cudaVkSemaphoreWait(cudaExtVkUpdateCudaSemaphore);
        float *d_mnistInput;
        cudaMalloc(&d_mnistInput, 28 * 28 * sizeof(float));

        vulkanImageCuda.updateCuda(textures[imageIndex].width, textures[imageIndex].height, d_mnistInput, textureObjMipMaps[imageIndex], stream);

        tensorRTManager->infer(d_mnistInput, stream);

        cudaVkSemaphoreSignal(cudaExtCudaUpdateVkSemaphore);
    }
    void cudaVkSemaphoreSignal(cudaExternalSemaphore_t &extSemaphore)
    {
        cudaExternalSemaphoreSignalParams extSemaphoreSignalParams;
        memset(&extSemaphoreSignalParams, 0, sizeof(extSemaphoreSignalParams));

        extSemaphoreSignalParams.params.fence.value = 0;
        extSemaphoreSignalParams.flags = 0;
        checkCudaErrors(cudaSignalExternalSemaphoresAsync(&extSemaphore, &extSemaphoreSignalParams, 1, stream));
    }

    void cudaVkSemaphoreWait(cudaExternalSemaphore_t &extSemaphore)
    {
        cudaExternalSemaphoreWaitParams extSemaphoreWaitParams;
        memset(&extSemaphoreWaitParams, 0, sizeof(extSemaphoreWaitParams));
        extSemaphoreWaitParams.params.fence.value = 0;
        extSemaphoreWaitParams.flags = 0;
        checkCudaErrors(cudaWaitExternalSemaphoresAsync(&extSemaphore, &extSemaphoreWaitParams, 1, stream));
    }
    // This function imports two Vulkan semaphores into CUDA as external semaphores.
    // This enables CUDA to synchronize with Vulkan using these semaphores.
    void cudaVkImportSemaphore()
    {
        // 1. Declare a CUDA external semaphore handle descriptor.
        cudaExternalSemaphoreHandleDesc externalSemaphoreHandleDesc;

        // 2. Clear the descriptor to ensure all fields are zeroed.
        memset(&externalSemaphoreHandleDesc, 0, sizeof(externalSemaphoreHandleDesc));

        // 3. Set the type to indicate the handle is an opaque file descriptor (Linux-specific).
        externalSemaphoreHandleDesc.type = cudaExternalSemaphoreHandleTypeOpaqueFd;

        // 4. Obtain a file descriptor for the Vulkan semaphore (cudaUpdateVkSemaphore) using Vulkan's external semaphore API.
        externalSemaphoreHandleDesc.handle.fd =
            (int)(uintptr_t)getSemaphoreHandle(cudaUpdateVkSemaphore, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, vulkanData);

        // 5. No special flags are needed for this import.
        externalSemaphoreHandleDesc.flags = 0;

        // 6. Import the Vulkan semaphore into CUDA as a CUDA external semaphore.
        checkCudaErrors(cudaImportExternalSemaphore(&cudaExtCudaUpdateVkSemaphore, &externalSemaphoreHandleDesc));

        // 7. Repeat the process for the second semaphore (vkUpdateCudaSemaphore).
        memset(&externalSemaphoreHandleDesc, 0, sizeof(externalSemaphoreHandleDesc));
        externalSemaphoreHandleDesc.type = cudaExternalSemaphoreHandleTypeOpaqueFd;
        externalSemaphoreHandleDesc.handle.fd =
            (int)(uintptr_t)getSemaphoreHandle(vkUpdateCudaSemaphore, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, vulkanData);
        externalSemaphoreHandleDesc.flags = 0;
        checkCudaErrors(cudaImportExternalSemaphore(&cudaExtVkUpdateCudaSemaphore, &externalSemaphoreHandleDesc));

        // 8. Print a message indicating success.
        printf("CUDA Imported Vulkan semaphore\n");
    }
    void getWaitFrameSemaphores(std::vector<VkSemaphore> &wait, std::vector<VkPipelineStageFlags> &waitStages) const
    {
        wait.push_back(cudaUpdateVkSemaphore);
        waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    void getSignalFrameSemaphores(std::vector<VkSemaphore> &signal) const
    {
        signal.push_back(vkUpdateCudaSemaphore);
    }
    // This function creates two Vulkan semaphores that are exportable to CUDA using an opaque file descriptor (FD).
    // These semaphores are used for synchronization between Vulkan and CUDA operations.
    void createSyncObjectsExt()
    {
        // 1. Create and initialize a VkSemaphoreCreateInfo structure.
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // 2. Clear the semaphoreInfo structure to zero, then set the sType again (redundant, but ensures clean state).
        memset(&semaphoreInfo, 0, sizeof(semaphoreInfo));
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // 3. Create and initialize a VkExportSemaphoreCreateInfoKHR structure.
        //    This structure tells Vulkan that the semaphore can be exported as an opaque FD for interop with CUDA.
        VkExportSemaphoreCreateInfoKHR vulkanExportSemaphoreCreateInfo = {};
        vulkanExportSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR;

        // 4. Set pNext to NULL (no further extension structures).
        vulkanExportSemaphoreCreateInfo.pNext = NULL;

        // 5. Specify that the semaphore can be exported as an opaque file descriptor.
        vulkanExportSemaphoreCreateInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

        // 6. Link the export info structure to the semaphore creation info via the pNext chain.
        semaphoreInfo.pNext = &vulkanExportSemaphoreCreateInfo;

        // 7. Create two Vulkan semaphores with export capability.
        //    These will be used for CUDA-Vulkan synchronization.
        if (vkCreateSemaphore(vulkanData.device, &semaphoreInfo, nullptr, &cudaUpdateVkSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(vulkanData.device, &semaphoreInfo, nullptr, &vkUpdateCudaSemaphore) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a CUDA-Vulkan!");
        }
    }
};