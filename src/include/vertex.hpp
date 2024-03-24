//
//  vertex.hpp
//  VulkanTesting
//
//  Created by qiru hu on 1/20/24.
//

#pragma once

// #include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

#include "mathlib.h"
#include "light.h"
#include "constants.h"

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec3 color;
    vec4 tangent;
    vec2 texCoord;
    
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }
    
    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, color);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, tangent);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

struct UniformBufferObjectScene {
    alignas(16) mat4 view;
    alignas(16) mat4 proj;
    alignas(16) mat4 light;
    alignas(16) vec4 eye; // camera position
};

struct UniformBufferObjectShadow {
    alignas(4) float zNear;
    alignas(4) float zFar;
    alignas(16) mat4 view;
    alignas(16) mat4 proj;
};

struct PushConstantShadow {
    alignas(16) mat4 model;
    alignas(16) mat4 lightVP;
};

struct UniformBufferObjectLight {
    alignas(4) uint32_t spotLightCount;
    alignas(4) uint32_t sphereLightCount;
    alignas(4) uint32_t directionalLightCount;
    alignas(4) uint32_t padding;
    alignas(16) SphereLight sphereLights[MAX_LIGHT_COUNT];
    alignas(16) SpotLight spotLights[MAX_LIGHT_COUNT];
    alignas(16) DirectionalLight directionalLights[MAX_LIGHT_COUNT];
};

struct ModelPushConstant {
    alignas(16) mat4 model;
    alignas(16) mat4 invModel;
};

/* vertex_hpp */
