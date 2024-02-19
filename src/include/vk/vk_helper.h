#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <string>

#include "utils/file.hpp"

#define VK_CHECK_RESULT( FN , std::string error_message) \
    if (VkResult result = FN) { \
        throw std::runtime_error("Call '" #FN "' returned " + std::to_string(result) + " [" + std::string(string_VkResult(result)) + "]." +  error_message); \
    }



// Shader loading ---------------

VkShaderModule createShaderModule(const std::vector<char>& code, VkDevice device) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create shader module!")
    
    return shaderModule;
}

VkPipelineShaderStageCreateInfo loadShader(
    std::string shader_file_path, 
    VkDevice device,
    VkShaderStageFlagBits stageBit) {
    auto shaderCode = readFile(shader_file_path);
    VkShaderModule shaderModule = createShaderModule(shaderCode, device);
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stageBit;
    shaderStageInfo.module = vertShaderModule;
    shaderStageInfo.pName = "main";
}