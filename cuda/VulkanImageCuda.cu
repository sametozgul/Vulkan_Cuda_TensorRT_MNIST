
#include <algorithm>

#include "VulkanImageCuda.h"

__global__ void convertTextureToMNIST(cudaTextureObject_t texObj, float *d_out, int width, int height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height)
        return;

    // CUDA textures use normalized coordinates for linear interpolation
    float u = (x + 0.5f) / width;
    float v = (y + 0.5f) / height;

    // Assuming texture contains uchar4 or float4 data in [0, 255] or [0.0, 1.0]
    float4 texColor = tex2D<float4>(texObj, u, v);

    // Convert RGB to grayscale using standard weights
    float gray = 0.299f * texColor.x + 0.587f * texColor.y + 0.114f * texColor.z;

    // Store in row-major format
    int idx = y * width + x;
    d_out[idx] = gray; // Assume texColor channels are already [0.0, 1.0]
}

int VulkanImageCuda::initCuda(uint8_t *vkDeviceUUID, size_t UUID_SIZE)
{
    int current_device = 0;
    int device_count = 0;
    int devices_prohibited = 0;

    cudaDeviceProp deviceProp;
    checkCudaErrors(cudaGetDeviceCount(&device_count));

    if (device_count == 0)
    {
        fprintf(stderr, "CUDA error: no devices supporting CUDA.\n");
        exit(EXIT_FAILURE);
    }

    // Find the GPU which is selected by Vulkan
    while (current_device < device_count)
    {
        cudaGetDeviceProperties(&deviceProp, current_device);

        if ((deviceProp.computeMode != cudaComputeModeProhibited))
        {
            // Compare the cuda device UUID with vulkan UUID
            int ret = memcmp((void *)&deviceProp.uuid, vkDeviceUUID, UUID_SIZE);
            if (ret == 0)
            {
                checkCudaErrors(cudaSetDevice(current_device));
                checkCudaErrors(cudaGetDeviceProperties(&deviceProp, current_device));
                printf("GPU Device %d: \"%s\" with compute capability %d.%d\n\n",
                       current_device,
                       deviceProp.name,
                       deviceProp.major,
                       deviceProp.minor);

                return current_device;
            }
        }
        else
        {
            devices_prohibited++;
        }

        current_device++;
    }

    if (devices_prohibited == device_count)
    {
        fprintf(stderr,
                "CUDA error:"
                " No Vulkan-CUDA Interop capable GPU found.\n");
        exit(EXIT_FAILURE);
    }

    return -1;
}
void VulkanImageCuda::updateCuda(unsigned int imageWidth, unsigned int imageHeight,
                                 float *d_mnistInput, cudaTextureObject_t textureObjMipMapInput,
                                 cudaStream_t &stream)
{

    dim3 block(16, 16);
    dim3 grid((28 + block.x - 1) / block.x, (28 + block.y - 1) / block.y);

    convertTextureToMNIST<<<grid, block, 0, stream>>>(textureObjMipMapInput, d_mnistInput, 28, 28);

    cudaGetLastError(); 
}

VulkanImageCuda::~VulkanImageCuda() {}
