#ifndef SHADER_H
#define SHADER_H

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
class Shader
{
public:
    Shader() = default;
    std::vector<char> readFile(const std::string& filename);
    VkPipelineShaderStageCreateInfo createShaderModule(const std::string &filename, VkShaderStageFlagBits stage, const VkDevice &device);
    VkShaderModule shader;
    void cleanUp(const VkDevice& device);

};

#endif // SHADER_H
