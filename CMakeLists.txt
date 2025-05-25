cmake_minimum_required(VERSION 3.25.2)
project(Cuda_Vulkan_Interop LANGUAGES C CXX CUDA)

# Add custom module paths
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

# CUDA setup
set(CMAKE_CUDA_COMPILER /usr/local/cuda-12.9/bin/nvcc)
set(CUDAToolkit_ROOT /usr/local/cuda-12.9)

find_package(CUDAToolkit REQUIRED)
find_library(NVINFER_LIB nvinfer)
find_library(NVONNXPARSER_LIB nvonnxparser)

# Position-independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CUDA architectures
set(CMAKE_CUDA_ARCHITECTURES 50 52 60 61 70 72 75 80 86 87 89 90 100 101 120)

# CUDA flags
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Wno-deprecated-gpu-targets")
if(ENABLE_CUDA_DEBUG)
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -G")
else()
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -lineinfo")
endif()

# Find required packages
set(CMAKE_PREFIX_PATH "/home/samo/KTX-Software/build")
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_library(KTX_LIBRARY NAMES ktx)
find_path(KTX_INCLUDE_DIR NAMES ktx.h)

# Sources
file(GLOB SOURCES
    main.cpp
    window/*
    vulkan/*
    tinygltf/*.cc
    model/*
    shaders/*.vert
    shaders/*.frag
    shaders/*.h
    imgui/*
    tools/*
    cuda/*
    tensorrt/*
    utils/*
)

# Executable
add_executable(Cuda_Vulkan_Interop ${SOURCES})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Language standard support
set_target_properties(Cuda_Vulkan_Interop PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)

# Extended lambdas for CUDA
target_compile_options(Cuda_Vulkan_Interop PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--std=c++20 --extended-lambda>
)
target_compile_features(Cuda_Vulkan_Interop PRIVATE cxx_std_20)
# Include paths
target_include_directories(Cuda_Vulkan_Interop PUBLIC
    
    vulkan window imgui include tinygltf model tools cuda Common tensorrt utils
    ${KTX_INCLUDE_DIR} ${CUDAToolkit_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(Cuda_Vulkan_Interop
    glfw
    glm::glm
    Vulkan::Vulkan
    ${KTX_LIBRARY}
    ${CUDA_LIBRARIES}
    ${NVINFER_LIB}
    ${NVONNXPARSER_LIB}
    CUDA::cudart # Add this line to link the CUDA runtime library
)

# Asset, shader, texture copy targets
file(GLOB ASSET_FILES assets/*)
add_custom_target(Assets DEPENDS ${ASSET_FILES})
add_dependencies(Cuda_Vulkan_Interop Assets)
add_custom_command(TARGET Assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Cuda_Vulkan_Interop>/assets"
)

file(GLOB GLSL_SOURCE_FILES shaders/*.spv)
add_custom_target(Shaders DEPENDS ${GLSL_SOURCE_FILES})
add_dependencies(Cuda_Vulkan_Interop Shaders)
add_custom_command(TARGET Shaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    "$<TARGET_FILE_DIR:Cuda_Vulkan_Interop>/shaders"
)

file(GLOB TEX_SOURCE_FILES textures/*)
add_custom_target(Textures DEPENDS ${TEX_SOURCE_FILES})
add_dependencies(Cuda_Vulkan_Interop Textures)
add_custom_command(TARGET Textures POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/textures"
    "$<TARGET_FILE_DIR:Cuda_Vulkan_Interop>/textures"
)

file(GLOB TENSORS_SOURCE_FILES tensorModels/*)
add_custom_target(Tensors DEPENDS ${TENSORS_SOURCE_FILES})
add_dependencies(Cuda_Vulkan_Interop Tensors)
add_custom_command(TARGET Tensors POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/tensorModels"
    "$<TARGET_FILE_DIR:Cuda_Vulkan_Interop>/tensorModels"
)
# Install
include(GNUInstallDirs)
install(TARGETS Cuda_Vulkan_Interop
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
