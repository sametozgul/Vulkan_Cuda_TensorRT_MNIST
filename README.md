# Vulkan-CUDA-TensorRT MNIST Pipeline

## Overview

This project demonstrates a modern GPU-accelerated pipeline that integrates **Vulkan**, **CUDA**, and **NVIDIA TensorRT** for real-time digit recognition using the MNIST dataset. The workflow showcases advanced interoperability between graphics and compute APIs, leveraging the strengths of each technology for efficient deep learning inference on the GPU.

---

## Pipeline Description

### 1. Vulkan Texture Creation

The pipeline begins by creating a texture image using the Vulkan API. Vulkan is a low-overhead, cross-platform 3D graphics and compute API that provides high-efficiency, cross-platform access to modern GPUs. In this project, Vulkan is responsible for rendering or loading an image into a GPU-resident texture.


### 2. Vulkan-CUDA Interoperability

The Vulkan texture is then shared with CUDA using external memory and semaphore extensions. This enables zero-copy, high-performance data sharing between the graphics and compute domains on the GPU.


### 3. CUDA Image Processing

Once the image is accessible in CUDA, a custom CUDA kernel processes the texture to convert it into the MNIST format (28x28 grayscale, normalized). CUDA is NVIDIA’s parallel computing platform and programming model, enabling direct control over GPU computation.


### 4. TensorRT Inference

The processed image is then passed to NVIDIA TensorRT, a high-performance deep learning inference optimizer and runtime. TensorRT runs a pre-trained MNIST model to recognize the digit in the image, leveraging GPU acceleration for real-time inference.


---

## Key Features

- **Zero-copy GPU memory sharing** between Vulkan and CUDA
- **Real-time image preprocessing** using custom CUDA kernels
- **Efficient deep learning inference** with TensorRT
- **End-to-end GPU pipeline**: from texture creation to neural network output

---

## References

- [Khronos Vulkan Specification](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html)
- [NVIDIA CUDA Toolkit Documentation](https://docs.nvidia.com/cuda/)
- [NVIDIA TensorRT Documentation](https://docs.nvidia.com/deeplearning/tensorrt/)
- [NVIDIA Developer Blog: Vulkan and CUDA Interoperability](https://developer.nvidia.com/blog/vulkan-cuda-interoperability/)
- [LeCun, Y., et al. "Gradient-based learning applied to document recognition." Proceedings of the IEEE 86.11 (1998): 2278-2324.](http://yann.lecun.com/exdb/publis/pdf/lecun-98.pdf) *(MNIST original paper)*

---

## Usage

1. **Build the project** using CMake and your preferred compiler.
2. **Run the application**: it will display a Vulkan-rendered texture, process it with CUDA, and classify the digit using TensorRT.
3. **Observe the results** in the UI and logs.

## Requirements

- **Vulkan SDK:** 1.4 or newer ([Download](https://vulkan.lunarg.com/sdk/home))
- **CUDA Toolkit:** 12.9 ([Download](https://developer.nvidia.com/cuda-toolkit))
- **NVIDIA TensorRT:** 8.5 or newer ([Download](https://developer.nvidia.com/tensorrt))
- **CMake:** 3.15 or newer
- **C++ Compiler:** Supporting C++17 or newer
- **GLFW:** (for window/context management)
- **NVIDIA GPU:** Pascal (GTX 10xx) or newer with Vulkan and CUDA support
- **Operating System:** Linux (tested)


**Make sure your driver and runtime versions are compatible with your hardware and each other.**
---

## About

This project is a practical demonstration of advanced GPU interoperability and deep learning inference, suitable for research, education, and real-time AI applications.

---
