#ifndef RENDERER_H
#define RENDERER_H
#define GLFW_INCLUDE_VULKAN
#include "Core.h"
#include <chrono>
#include "CudaManager.h"
#include "UniformBuffer.h"
#include "StagingBuffer.h"

class Renderer : public Core
{
public:
    Renderer(GLFWwindow &window);
    // void init(GLFWwindow& window);
    ~Renderer();
    void render() override;
    void draw();
    bool uiVisible = false;
    void cleanUp();
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    static void keyboardInputCallBack(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void handleMousePressCallBack(GLFWwindow *window, int button, int action, int mods);
    static void handleCursorPosCallBack(GLFWwindow *window, double xPos, double yPos);
    static void handleMouseReleasedCallBack(GLFWwindow *window, double xPos, double yPos);
    static void handleCursorCallBack(GLFWwindow *window, double xPos, double yPos);

private:
    void prepare() override;

private:
    void OnUpdateUIOverlay(UserInterface *userInterface) override;

private:
    float zNear{0.1f};
    float zFar{1024.0f};

private:
    void buildCommandBuffers() override;

private:
    void createGraphicsPipeline();
    void setupDescriptors();
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};

private:
    void createUniformBuffers();
    void updateUniformBuffers();

private:
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;
    float frameTimer = 1.0f;
    float timer = 0.0f;
    float timerSpeed = 0.25f;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp, tPrevEnd;

private:
    void createCamera();

private:
    struct SceneData
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;

    } sceneData;
    struct Graphics
    {
        UniformBuffer<SceneData> sceneDataUniformBuffer{};
        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> descriptorSets;
        VkPipeline pipeline{VK_NULL_HANDLE};
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        StagingBuffer vertexBuffer;
        StagingBuffer indexBuffer;
        uint32_t indexCount{0};
    } graphics;

protected:
    virtual void cudaInitialize();
    void submitVulkan(uint32_t imageIndex);

private:
    void generateQuad();
    std::unique_ptr<CudaManager> cudaManager = nullptr;
    struct quadVertex
    {
        float pos[3];
        float uv[2];
        float normal[3];
    };
    uint32_t currentTextureIndex = 0;
    const uint32_t TEXTURE_COUNT = 10;
    float textureSwitchTimer = 0.0f;
};

#endif // RENDERER_H
