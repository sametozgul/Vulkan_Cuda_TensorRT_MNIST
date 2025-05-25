#include "Core.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <set>

#include <glm/gtx/string_cast.hpp>
Core::Core()
{
}

Core::~Core()
{
    cleanUp();
}

void Core::init()
{
    createInstance();
    validation.setupDebugMessenger(instance_);
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    swapChain.init(context);
    swapChain.create();
    swapChain.createImageViews();
    context.swapChainExtent = swapChain.extent;
    createRenderPass();
    createPipelineCache();
    depthBufferObject.init(context);
    createColorResources();
    depthBufferObject.createSources();
    createFramebuffers();
    createCommandBuffer();
    
    createSyncObjects();
    mUserInterface.init(context);
    mUserInterface.shaders = {vertexShaders.createShaderModule("shaders/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, logicalDevice_),
                              fragmentShaders.createShaderModule("shaders/uioverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, logicalDevice_)};
    mUserInterface.prepareResources();
    mUserInterface.preparePipeline(pipelineCache, renderPass_, swapChain.imageFormat, depthFormat);
    vertexShaders.cleanUp(logicalDevice_);
    fragmentShaders.cleanUp(logicalDevice_);
}

void Core::cleanUp()
{
    vkDeviceWaitIdle(logicalDevice_);

    depthBufferObject.cleanUp();
    vkDestroyImageView(logicalDevice_, colorImageView, nullptr);
    vkDestroyImage(logicalDevice_, colorImage, nullptr);
    vkFreeMemory(logicalDevice_, colorImageMemory, nullptr);
    vkDestroyPipelineCache(logicalDevice_, pipelineCache, nullptr);
    swapChain.cleanup();
    vkDestroyRenderPass(logicalDevice_, renderPass_, nullptr);

    for(size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
        vkDestroySemaphore(logicalDevice_, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(logicalDevice_, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(logicalDevice_, inFlightFences[i], nullptr);
    }
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    mUserInterface.cleanUp();
    vkDestroyCommandPool(logicalDevice_, commandPool_, nullptr);
    vkDestroyDevice(logicalDevice_, nullptr);
    validation.cleanUp(instance_);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}

void Core::windowResize()
{
    if (!prepared)
    {
        return;
    }
    prepared = false;
    framebufferResized = true;
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(logicalDevice_);
    clearSwapChain();
    vkFreeCommandBuffers(logicalDevice_, commandPool_, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    createCommandBuffer();
    buildCommandBuffers();
    vkDeviceWaitIdle(logicalDevice_);
    if ((width > 0.0f) && (height > 0.0f))
    {
        camera.updateAspectRatio((float)width / (float)height);
    }
    prepared = true;
}

void Core::updateCamera(const int &key)
{
    if (key == GLFW_KEY_ESCAPE)
    {
        std::cout << "Escape key pressed. Closing window..." << std::endl;
        glfwSetWindowShouldClose(window_, true); // Close the window
    }
    else if (key == GLFW_KEY_W)
    {
        camera.keys.up = true;
        camera.keys.left = false;
        camera.keys.right = false;
        camera.keys.down = false;
        camera.update(0.5);
    }
    else if (key == GLFW_KEY_S)
    {
        camera.keys.down = true;
        camera.keys.up = false;
        camera.keys.left = false;
        camera.keys.right = false;
        camera.update(0.5);
    }
    else if (key == GLFW_KEY_A)
    {
        camera.keys.left = true;
        camera.keys.right = false;
        camera.keys.down = false;
        camera.keys.up = false;
        camera.update(0.5);
    }
    else if (key == GLFW_KEY_D)
    {
        camera.keys.right = true;
        camera.keys.left = false;
        camera.keys.down = false;
        camera.keys.up = false;
        camera.update(0.5);
    }
}

void Core::mousePressCallBack(int button, int action, int mods)
{
    ImGuiIO &io = ImGui::GetIO();
    if (button >= 0 && button < ImGuiMouseButton_COUNT)
    {
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    }

    /* hide from application */
    if (io.WantCaptureMouse)
    {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        mouse.buttons.left = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        mouse.buttons.right = true;
    }
    if (action == GLFW_RELEASE)
    {
        mouse.buttons.left = false;
        mouse.buttons.right = false;
    }
}

void Core::mouseCursorPosCallBack(const double &x, const double &y)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent((float)x, (float)y);

    /* hide from application */
    if (io.WantCaptureMouse)
    {
        return;
    }
    int32_t dx = (int32_t)mouse.position.x - x;
    int32_t dy = (int32_t)mouse.position.y - y;
    bool handled = false;

    if (handled)
    {
        mouse.position = glm::vec2((float)x, (float)y);
        return;
    }

    if (mouse.buttons.left)
    {
        camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
        viewUpdated = true;
    }
    mouse.position = glm::vec2((float)x, (float)y);
}

void Core::mouseCursorCallBack(const double &x, const double &y)
{
}

void Core::createInstance()
{

    // Check validation layer is enabled and requested layer is supported
    if (enableValidationLayers && !checkRequestedLayerSupported())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    // Create VkApplicationInfo structure is needed to define specifation about API
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Book Review API";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;
    applicationInfo.pEngineName = "No Engine";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    // Get required extensions
    auto extensions = getRequiredExtensions();
    // Create VkInstanceCreateInfo structure is needed to define about instance info.
    VkInstanceCreateInfo createInstanceInfo{};
    createInstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInstanceInfo.pApplicationInfo = &applicationInfo;
    createInstanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInstanceInfo.ppEnabledLayerNames = validationLayers.data();
    createInstanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInstanceInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    validation.populateDebugMessengerCreateInfo(debugCreateInfo);
    // To capture events that occur while creating or destroying an instance an application
    createInstanceInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    // Create an instance without layers.
    VK_CHECK(vkCreateInstance(&createInstanceInfo, nullptr, &instance_));

    context.instance = instance_;
    std::cout << "A vulkan instance is initialized..." << std::endl;
}

void Core::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    // Enumarates physical device on the system.
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        std::cerr << "No device found" << std::endl;
    }
    // Lists available devices.
    std::vector<VkPhysicalDevice> devices(deviceCount);

    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    // Get suitable device for application.
    for (const auto &device : devices)
    {
        if (isDeviceSuitable(device))
        {
            physicalDevice_ = device;
            msaaSamples = VK_SAMPLE_COUNT_1_BIT;
            context.msaaSamples = msaaSamples;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    context.physicalDevice = physicalDevice_;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);

    uiData.physicalDeviceName = properties.deviceName;

    std::cout << "Physical device is found..." << std::endl;
}

VkSampleCountFlagBits Core::getMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice_, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT)
    {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT)
    {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT)
    {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

void Core::createLogicalDevice()
{
    // Get selected queue family.
    indices = findQueueFamilies(physicalDevice_, surface_);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

    // Define Queue properties
    float queuePriority = 1.0f;
    for (auto queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.emplace_back(queueCreateInfo);
    }
    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    // Device creation needs to define validation and extension similar to Instance creation.
    // But those are specific to device.
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    // Previous Vulkan implementations needed to distinct instance validation layer and device validation layer but this no longer case.
    if (enableValidationLayers)
    {
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        deviceCreateInfo.enabledLayerCount = 0;
    }
    VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &logicalDevice_));

    std::cout << "Logical device is initialized..." << std::endl;
    // The device queues are implicitly cleaned up when the device is detroyed so no need to any clean up operation in cleanUp() function.
    vkGetDeviceQueue(logicalDevice_, indices.graphicsAndComputeFamily.value(), 0, &graphicQueue_);
    vkGetDeviceQueue(logicalDevice_, indices.presentFamily.value(), 0, &presentQueue_);

    context.device = logicalDevice_;
    context.graphicQueue = graphicQueue_;

    VkPhysicalDeviceIDProperties vkPhysicalDeviceIDProperties = {};
    vkPhysicalDeviceIDProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    vkPhysicalDeviceIDProperties.pNext = NULL;

    VkPhysicalDeviceProperties2 vkPhysicalDeviceProperties2 = {};
    vkPhysicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkPhysicalDeviceProperties2.pNext = &vkPhysicalDeviceIDProperties;

    PFN_vkGetPhysicalDeviceProperties2 fpGetPhysicalDeviceProperties2;
    fpGetPhysicalDeviceProperties2 =
        (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceProperties2");
    if (fpGetPhysicalDeviceProperties2 == NULL)
    {
        throw std::runtime_error("Vulkan: Proc address for \"vkGetPhysicalDeviceProperties2KHR\" not "
                                 "found.\n");
    }

    fpGetPhysicalDeviceProperties2(physicalDevice_, &vkPhysicalDeviceProperties2);

    memcpy(vkDeviceUUID_, vkPhysicalDeviceIDProperties.deviceUUID, VK_UUID_SIZE);

    context.vkDeviceUUID = vkDeviceUUID_;

    VkBool32 validFormat{false};
    if (requiresStencil)
    {
        validFormat = getSupportedDepthStencilFormat(physicalDevice_, &depthFormat);
    }
    else
    {
        validFormat = getSupportedDepthFormat(physicalDevice_, &depthFormat);
    }
    assert(validFormat);
}

bool Core::checkRequestedLayerSupported()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : validationLayers)
    {
        bool layerFound = false;

        for (const auto &layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

bool Core::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtension(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtension.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : availableExtension)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

std::vector<const char *> Core::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool Core::isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device, surface_);

    bool extensionSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface_);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy && supportedFeatures.geometryShader;
}

void Core::createSurface()
{
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
    context.surface = surface_;
}

void Core::createColorResources()
{
    VkFormat colorFormat = swapChain.imageFormat;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChain.extent.width;
    imageInfo.extent.height = swapChain.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.samples = msaaSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    colorImage = createImage(context, imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImageMemory);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = colorFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.image = colorImage;

    colorImageView = createImageView(context.device, viewInfo);
}

void Core::clearSwapChain()
{
    depthBufferObject.cleanUp();
    vkDestroyImageView(logicalDevice_, colorImageView, nullptr);
    vkDestroyImage(logicalDevice_, colorImage, nullptr);
    vkFreeMemory(logicalDevice_, colorImageMemory, nullptr);

    swapChain.init(context);
    swapChain.recreate();
    context.swapChainExtent = swapChain.extent;
    createColorResources();
    depthBufferObject.init(context);
    depthBufferObject.createSources();
    createFramebuffers();
}

void Core::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK(vkCreatePipelineCache(logicalDevice_, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void Core::createRenderPass()
{

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // We have only single color buffer attechment represented by one of the images from the swap chain.
    VkAttachmentDescription colorAttachment{};
    // The format of the color attachment should match the format of the swap chain images
    // we're not doing anything with multisampling yet, so we'll stick to 1 sample.
    colorAttachment.format = swapChain.imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // The loadOp and storeOp determine what to do with the data in the attachment before rendering and after rendering.

    //  Clear the values to a constant at the start
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Rendered contents will be stored in memory and can be read later
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // do anything with the stencil buffer, so the results of loading and storing are irrelevant.
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based on what you're trying to do with an image.
    // The initialLayout specifies which layout the image will have before the render pass begins.
    // VK_IMAGE_LAYOUT_UNDEFINED means that we don't care what previous layout the image was in
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // The finalLayout specifies the layout to automatically transition to when the render pass finishes.
    // We want the image to be ready for presentation using the swap chain after rendering, which is why we use VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout.
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Subpasses and attachment references
    // A single render pass can consist of multiple subpasses. Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another.
    // If you group these rendering operations into one render pass, then Vulkan is able to reorder the operations and conserve memory bandwidth for possibly better performance.

    VkAttachmentReference colorAttachmentRef{};
    // The attachment parameter specifies which attachment to reference by its index in the attachment descriptions array. Our array consists of a single VkAttachmentDescription, so its index is 0.
    colorAttachmentRef.attachment = 0;
    // The layout specifies which layout we would like the attachment to have during a subpass that uses this reference.
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;
    subpass.pResolveAttachments = nullptr;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = 0;

    dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass = 0;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = 0;
    dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = 0;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(logicalDevice_, &renderPassInfo, nullptr, &renderPass_));

    std::cout << "Renderpass is initialized..." << std::endl;
    context.renderpass = renderPass_;
}

void Core::createFramebuffers()
{
    // The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object
    // A framebuffer object references all of the VkImageView objects that represent the attachments

    // we have to create a framebuffer for all of the images in the swap chain
    swapChain.framebuffers.resize(swapChain.imageViews.size());
    // You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments.
    // The next step is to modify the framebuffer creation to bind the depth image to the depth attachment.
    for (size_t i = 0; i < swapChain.imageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {
            swapChain.imageViews[i],
            depthBufferObject.imageView};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChain.extent.width;
        framebufferInfo.height = swapChain.extent.height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(logicalDevice_, &framebufferInfo, nullptr, &swapChain.framebuffers[i]));
    }

    std::cout << "FrameBuffers is initialized..." << std::endl;
}

void Core::createCommandPool()
{
    // If we want send command graphic card we should create a command pool. Beacuse Vulkan handles all available command by itself.
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice_, surface_);
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // There are two possible flags for command pools:
    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together

    // We will be recording a command buffer every frame, so we want to be able to reset and rerecord over it. Thus, we need to set the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag bit for our command pool.
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value();

    // Command buffers are executed by submitting them on one of the device queues, like graphic and presentation queues we retrieved.
    // Each command pool can only allocate command buffers that are submitted on a single type of queue.
    // We're going to record commands for drawing, which is why we've chosen the graphics queue family.
    VK_CHECK(vkCreateCommandPool(logicalDevice_, &createInfo, nullptr, &commandPool_));

    context.commandPool = commandPool_;
    std::cout << "CommanPool is initialized..." << std::endl;
}

void Core::createCommandBuffer()
{
    commandBuffers.resize(swapChain.images.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    VK_CHECK(vkAllocateCommandBuffers(logicalDevice_, &allocInfo, commandBuffers.data()));
}

void Core::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
}

void Core::nextFrame()
{
    if (viewUpdated)
    {
        viewUpdated = false;
    }
    render();
    camera.update(uiData.frameTime);

    if (camera.moving())
    {
        viewUpdated = true;
    }
}

void Core::prepareFrame(uint32_t& imageIndex, size_t& currentFrameIdx)
{
    vkWaitForFences(logicalDevice_, 1, &inFlightFences[currentFrameIdx], VK_TRUE, UINT64_MAX);

    VkResult result = swapChain.acquireNextImage(imageAvailableSemaphores[currentFrameIdx], imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        windowResize();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    vkResetFences(logicalDevice_, 1, &inFlightFences[currentFrameIdx]);
}

void Core::submitFrame(uint32_t& imageIndex, size_t& currentFrameIdx)
{
    
    VkResult result{};
    result = swapChain.queuePresent(graphicQueue_, imageIndex, renderFinishedSemaphores[currentFrameIdx]);

    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR) ) {
        windowResize();
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return;
        }
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    VK_CHECK(vkQueueWaitIdle(graphicQueue_));
    currentFrame++;

}

void Core::createSyncObjects()
{

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo         = {};
    fenceInfo.sType                     = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags                     = VK_FENCE_CREATE_SIGNALED_BIT;

    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(logicalDevice_, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image available semaphore!");
        }
        if (vkCreateSemaphore(logicalDevice_, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image available semaphore!");
        }
        if (vkCreateFence(logicalDevice_, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image available semaphore!");
        }
    }

}


void Core::prepare()
{
}

void Core::renderFrame()
{

}

void Core::buildCommandBuffers()
{
}

void Core::OnUpdateUIOverlay(UserInterface *userInterface)
{
}

void Core::drawUI(const VkCommandBuffer commandBuffer)
{

    const VkViewport viewport = initializers::viewport((float)swapChain.extent.width, (float)swapChain.extent.height, 0.0f, 1.0f);
    const VkRect2D scissor = initializers::rect2D(swapChain.extent.width, swapChain.extent.height, 0, 0);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    mUserInterface.draw(commandBuffer);
}


