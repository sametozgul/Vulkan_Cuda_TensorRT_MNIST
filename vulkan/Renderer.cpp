#include "Renderer.h"
#include <fstream>
#include "stb_image_write.h"
#include <chrono>
#include <thread>
Renderer::Renderer(GLFWwindow &window)
{
    window_ = &window;
    context.window = window_;

    Renderer::prepare();
}

Renderer::~Renderer()
{
    vkDeviceWaitIdle(logicalDevice_);
    vkDestroyPipeline(logicalDevice_, graphics.pipeline, nullptr);
    vkDestroyDescriptorPool(logicalDevice_, descriptorPool, nullptr);
    vkDestroyPipelineLayout(logicalDevice_, graphics.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(logicalDevice_, graphics.descriptorSetLayout, nullptr);
    graphics.sceneDataUniformBuffer.cleanUp();
    graphics.vertexBuffer.cleanUp();
    graphics.indexBuffer.cleanUp();
}

void Renderer::render()
{
    auto tStart = std::chrono::high_resolution_clock::now();
    if (!prepared)
        return;
    updateUniformBuffers();

    size_t currentFrameIdx = currentFrame % MAX_FRAMES_IN_FLIGHT;
    uint32_t imageIndex;
    Core::prepareFrame(imageIndex, currentFrameIdx);
    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;

    waitSemaphores.push_back(imageAvailableSemaphores[currentFrameIdx]);
    waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    if (currentFrame != 0)
    {
        cudaManager->getWaitFrameSemaphores(waitSemaphores, waitStages);
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    std::vector<VkSemaphore> signalSemaphores;
    signalSemaphores.push_back(renderFinishedSemaphores[currentFrameIdx]);
    cudaManager->getSignalFrameSemaphores(signalSemaphores);
    submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    VK_CHECK(vkQueueSubmit(graphicQueue_, 1, &submitInfo, inFlightFences[currentFrameIdx]);)
    Core::submitFrame(imageIndex, currentFrameIdx);
    cudaManager->cudaUpdateVkImage(currentTextureIndex);
    std::this_thread::sleep_for(std::chrono::microseconds(10000));

    frameCounter++;
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    frameTimer = tDiff / 1000.0f;
    timer += timerSpeed * frameTimer;
    if (timer > 1.0)
    {
        timer -= 1.0f;
    }
    float fpsTimer = std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count();
    if (fpsTimer > 1000.0f)
    {
        lastFPS = (float)frameCounter * (1000.0f / fpsTimer);
        frameCounter = 0;
        lastTimestamp = tEnd;
    }
    mUserInterface.updateTimer -= frameTimer;
    if (mUserInterface.updateTimer >= 0.0f)
    {
        return;
    }
    mUserInterface.updateTimer = 1.0f / 30.0f;
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)swapChain.extent.width, (float)swapChain.extent.height);
    io.DeltaTime = frameTimer;

    ImGui::NewFrame();
    ImGuiWindowFlags imguiWindowFlags = 0;
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("Control", nullptr, imguiWindowFlags);
    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);
    ImGui::NewLine();
    ImGui::Text("Detected Text: %d", cudaManager->getDetection());
    OnUpdateUIOverlay(&mUserInterface);
    ImGui::End();
    ImGui::Render();

    if (mUserInterface.update() || mUserInterface.updated)
    {
        buildCommandBuffers();
        mUserInterface.updated = false;
    }
    textureSwitchTimer += frameTimer;
    if (textureSwitchTimer >= 1.0f) // Switch every second
    {
        textureSwitchTimer = 0.0f;
        currentTextureIndex = (currentTextureIndex + 1) % TEXTURE_COUNT;

        buildCommandBuffers();
    }
}

void Renderer::draw()
{
}

void Renderer::cleanUp()
{
}

void Renderer::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto app = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    app->context.window = window;
    app->windowResize();
}

void Renderer::keyboardInputCallBack(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    auto app = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    app->updateCamera(key);
}

void Renderer::handleMousePressCallBack(GLFWwindow *window, int button, int action, int mods)
{
    auto app = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    app->mousePressCallBack(button, action, mods);
}

void Renderer::handleCursorPosCallBack(GLFWwindow *window, double xPos, double yPos)
{
    auto app = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    app->mouseCursorPosCallBack(xPos, yPos);
}

void Renderer::handleMouseReleasedCallBack(GLFWwindow *window, double xPos, double yPos)
{
}

void Renderer::handleCursorCallBack(GLFWwindow *window, double xPos, double yPos)
{
    auto app = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    app->mouseCursorCallBack(xPos, yPos);
}

void Renderer::prepare()
{
    Core::init();
    generateQuad();
    cudaInitialize();
    createCamera();
    createUniformBuffers();
    setupDescriptors();
    createGraphicsPipeline();
    buildCommandBuffers();
    prepared = true;
    Core::windowResize();
}

void Renderer::OnUpdateUIOverlay(UserInterface *userInterface)
{
   
}

void Renderer::buildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();
    cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    VkClearValue clearValues[2];
    clearValues[0].color = {{0.025f, 0.025f, 0.025f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderPass_;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = swapChain.extent.width;
    renderPassBeginInfo.renderArea.extent.height = swapChain.extent.height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    for (int32_t i = 0; i < commandBuffers.size(); ++i)
    {

        renderPassBeginInfo.framebuffer = swapChain.framebuffers[i];

        VK_CHECK(vkBeginCommandBuffer(commandBuffers[i], &cmdBufInfo));

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = initializers::viewport((float)swapChain.extent.width, (float)swapChain.extent.height, 0.0f, 1.0f);
        vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);

        VkRect2D scissor = initializers::rect2D(swapChain.extent.width, swapChain.extent.height, 0, 0);
        vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);

        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1, &graphics.descriptorSets[currentTextureIndex], 0, NULL);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);

        // cudaManager->fillRenderingCommandBuffer(commandBuffers[i]);
        VkDeviceSize offsets[1] = {0};
        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &graphics.vertexBuffer.buffer, offsets);
        vkCmdBindIndexBuffer(commandBuffers[i], graphics.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffers[i], graphics.indexCount, 1, 0, 0, 0);

        drawUI(commandBuffers[i]);

        vkCmdEndRenderPass(commandBuffers[i]);

        VK_CHECK(vkEndCommandBuffer(commandBuffers[i]));
    }
}

void Renderer::createGraphicsPipeline()
{
    // Layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&graphics.descriptorSetLayout, 1);
    VK_CHECK(vkCreatePipelineLayout(logicalDevice_, &pipelineLayoutCreateInfo, nullptr, &graphics.pipelineLayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState blendAttachmentState = initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    // Shaders
    shaderStages[0] = vertexShaders.createShaderModule("shaders/shader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, logicalDevice_);
    shaderStages[1] = fragmentShaders.createShaderModule("shaders/shader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, logicalDevice_);

    // Vertex input state
    std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
        initializers::vertexInputBindingDescription(0, sizeof(quadVertex), VK_VERTEX_INPUT_RATE_VERTEX)};
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
        initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(quadVertex, pos)),
        initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(quadVertex, uv)),
        initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(quadVertex, normal)),
    };
    VkPipelineVertexInputStateCreateInfo vertexInputState = initializers::pipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = initializers::pipelineCreateInfo(graphics.pipelineLayout, renderPass_, 0);
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();
    VK_CHECK(vkCreateGraphicsPipelines(logicalDevice_, pipelineCache, 1, &pipelineCreateInfo, nullptr, &graphics.pipeline));

    vertexShaders.cleanUp(logicalDevice_);
    fragmentShaders.cleanUp(logicalDevice_);
}

void Renderer::createCamera()
{
    timerSpeed *= 0.25f;

    camera.type = Camera::CameraType::firstperson;
    camera.type = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
    camera.setRotation(glm::vec3(0.0f, 15.0f, 0.0f));
    camera.setPerspective(60.0f, (float)swapChain.extent.width / (float)swapChain.extent.height, 0.1f, 256.0f);
}

void Renderer::updateUniformBuffers()
{

    sceneData.projection = camera.matrices.perspective;
    sceneData.view = camera.matrices.view;
    sceneData.model = glm::mat4(1.0f);

    graphics.sceneDataUniformBuffer.updateData(sceneData);
}
void Renderer::setupDescriptors()
{
    // Pool
    graphics.descriptorSets.resize(TEXTURE_COUNT);
    std::vector<VkDescriptorPoolSize> poolSizes = {
        initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, TEXTURE_COUNT),
        initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TEXTURE_COUNT)

    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = initializers::descriptorPoolCreateInfo(poolSizes, TEXTURE_COUNT);
    VK_CHECK(vkCreateDescriptorPool(logicalDevice_, &descriptorPoolInfo, nullptr, &descriptorPool));

    // Layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0 : Shader uniform ubo
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice_, &descriptorLayout, nullptr, &graphics.descriptorSetLayout));
    std::vector<VkDescriptorSetLayout> layouts(TEXTURE_COUNT, graphics.descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = initializers::descriptorSetAllocateInfo(descriptorPool, layouts.data(), TEXTURE_COUNT);
    VK_CHECK(vkAllocateDescriptorSets(logicalDevice_, &allocInfo, graphics.descriptorSets.data()));

    // Fill each descriptor set with the corresponding texture
    for (uint32_t i = 0; i < TEXTURE_COUNT; ++i)
    {
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            initializers::writeDescriptorSet(graphics.descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &graphics.sceneDataUniformBuffer.descriptor),
            initializers::writeDescriptorSet(graphics.descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cudaManager->textures[i].descriptor),
        };
        vkUpdateDescriptorSets(logicalDevice_, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }
}

void Renderer::createUniformBuffers()
{
    graphics.sceneDataUniformBuffer.init(context);
    updateUniformBuffers();
}

void Renderer::generateQuad()
{
    std::vector<quadVertex> vertices = {
        // Centered quad (NDC: -0.5 to +0.5)
        {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},  // Top-left
        {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},   // Top-right
        {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},  // Bottom-right
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // Bottom-left
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0 // Single quad
    };
    graphics.indexCount = static_cast<uint32_t>(indices.size());
    graphics.vertexBuffer.create(context, vertices,
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 sizeof(vertices[0]) * vertices.size());
    graphics.indexBuffer.create(context, indices,
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                sizeof(indices[0]) * indices.size());
}

void Renderer::cudaInitialize()
{
    cudaManager = std::make_unique<CudaManager>(context, TEXTURE_COUNT);
}
