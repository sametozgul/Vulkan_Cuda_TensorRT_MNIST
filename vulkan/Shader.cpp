#include "Shader.h"
#include "Context.h"
#include <fstream>

std::vector<char> Shader::readFile(const std::string &filename)
{

    // ate -> Start reading at th end of file
    // binary -> Read as the binary
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if(!file.is_open()){
        throw std::runtime_error("The file could not be read");
    }
    // The advantage of starting to read at the end of file is that it is possible to determine file size.
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;

}

VkPipelineShaderStageCreateInfo Shader::createShaderModule(const std::string &filename, VkShaderStageFlagBits stage ,const VkDevice& device)
{

    std::vector<char> code = readFile(filename);
    // To create a VkShaderModule, We need to specify a pointer to the buffer with bytecode and length of it.
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());


    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shader));

    // types of objects we have now
    // Shader stages: the shader modules that define the functionality of the programmable stages of the graphics pipeline
    // Fixed-function state: all of the structures that define the fixed-function stages of the pipeline, like input assembly, rasterizer, viewport and color blending
    // Pipeline layout: the uniform and push values referenced by the shader that can be updated at draw time
    // Render pass: the attachments referenced by the pipeline stages and their usage

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    // Shader module the containing the code
    shaderStageInfo.module = shader;
    // function to invoke known as entrypoint. It is possible to combine multiple fragment shaders into a single shader module nad use different entrypoint to differentiate between their behaviors.
    shaderStageInfo.pName = "main";

    return shaderStageInfo;

}

void Shader::cleanUp(const VkDevice& device)
{
    vkDestroyShaderModule(device, shader, nullptr);
}

