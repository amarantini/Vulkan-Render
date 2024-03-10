//
//  viewer.h
//
//  Created by qiru hu on 1/29/24.
//

#pragma once

//
//  main.cpp
//  VulkanTesting
//
//  Created by qiru hu on 1/16/24.
//



#include "scene/material.h"
#include <_types/_uint8_t.h>
#include <malloc/_malloc.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_beta.h>

#include <chrono>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <string>


#include "vertex.hpp"
#include "scene.h"
#include "material.h"
#include "math/mathlib.h"
#include "utils/constants.h"
#include "controllers/camera_controller.h"
#include "controllers/input_controller.h"
#include "controllers/window_controller.h"
#include "controllers/animation_controller.h"
#include "controllers/events_controller.h"




const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 450;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


struct QueueFamilyIndices;

struct SwapChainSupportDetails;



class ViewerApplication {
    friend class WindowController;
public:
    void setUpScene(const std::string& file_name);

    void setCamera(const std::string& camera_name);

    void setPhysicalDevice(const std::string& _physical_device_name);

    void setDrawingSize(const int& w, const int& h);

    void setCulling(const std::string& culling_);

    void setHeadless(const std::string& event_file_name);

    void disableAnimationLoop();

    void enableMeasure();

    void run();

    void listPhysicalDevice();

private:
    ModelInfoList model_info_list;

    std::shared_ptr<CameraController> camera_controller;
    std::shared_ptr<InputController> input_controller;
    std::shared_ptr<WindowController> window_controller;
    std::shared_ptr<AnimationController> animation_controller;
    std::shared_ptr<EventsController> event_controller;

    std::string physical_device_name = "None";
    int width = WIDTH;
    int height = HEIGHT;
    bool headless = false;
    std::string culling = CULLING_NONE;
    // For performance measuring
    bool does_measure = false;
    int frame_count = 0;
    float total_time = 0.0f;
    int vertices_count = 0;

    UniformBufferObject ubo = {};
    
    VkInstance instance;
    
    VkSurfaceKHR surface;
    
    static inline VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    static inline VkDevice device = NULL; //logical device
    
    static inline VkQueue graphicsQueue = NULL;
    VkQueue presentQueue;
    
    VkSwapchainKHR swapChain;//a queue of images that are waiting to be presented to the screen
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    uint32_t imageIndex;
    
    std::vector<VkImageView> swapChainImageViews;
    
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;

    VkPipelineCache pipelineCache;
    //Multiple pipelines
    struct Pipelines {
        VkPipeline simple = VK_NULL_HANDLE;
        VkPipeline env = VK_NULL_HANDLE;
        VkPipeline mirror = VK_NULL_HANDLE;
        VkPipeline lamber = VK_NULL_HANDLE;
        VkPipeline pbr = VK_NULL_HANDLE;

        Pipelines() {
            simple = VK_NULL_HANDLE;
            env = VK_NULL_HANDLE;
            mirror = VK_NULL_HANDLE;
            lamber = VK_NULL_HANDLE;
            pbr = VK_NULL_HANDLE;
        }

        void destroy() {
            vkDestroyPipeline(device, simple, nullptr);
            vkDestroyPipeline(device, env, nullptr);
            vkDestroyPipeline(device, mirror, nullptr);
            vkDestroyPipeline(device, lamber, nullptr);
            vkDestroyPipeline(device, pbr, nullptr);
        }
    };
    
    inline static Pipelines pipelines = Pipelines();
    
    
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
    static inline VkCommandPool commandPool = NULL;
    std::vector<VkCommandBuffer> commandBuffers;
    
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    
    bool framebufferResized = false;
    static inline uint32_t currentFrame = 0;
    
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    static inline VkDescriptorPool descriptorPool = NULL;
    
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    struct VkHelper {
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                        VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                        VkDeviceMemory& bufferMemory) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to create buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate buffer memory!");
            }

            vkBindBufferMemory(device, buffer, bufferMemory, 0);
        }

        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();
            
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0; // Optional
            copyRegion.dstOffset = 0; // Optional
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
            
            endSingleTimeCommands(commandBuffer);
        }

        VkCommandBuffer beginSingleTimeCommands() {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = commandPool;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            return commandBuffer;
        }

        void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(graphicsQueue);

            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        }
        
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
            
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                        return i;
                    }
            }

            throw std::runtime_error("failed to find suitable memory type!");
        }

        void transitionImageLayout(VkImage image, 
            VkImageLayout oldLayout, 
            VkImageLayout newLayout,
            VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
            int levelCount = 1, int layerCount = 1 ) {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();
            
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcAccessMask = srcAccessMask;
            barrier.dstAccessMask = dstAccessMask;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = levelCount;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = layerCount;
            
            vkCmdPipelineBarrier(
                commandBuffer,
                srcStageMask /* pipeline stage the operations occur that should happen before the barrier */, 
                dstStageMask /* pipeline stage in which operations will wait on the barrier */,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            endSingleTimeCommands(commandBuffer);
        }

        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, int arrayLayers = 1, VkImageCreateFlags flags = 0, int mipLevels = 1) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = mipLevels;
            imageInfo.arrayLayers = arrayLayers;
            imageInfo.format = format;
            imageInfo.tiling = tiling;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = usage;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.flags = flags;

            if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image!");
            }

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(device, image, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = vkHelper.findMemoryType(memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate image memory!");
            }

            vkBindImageMemory(device, image, imageMemory, 0);
        }

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, int layerCount = 1, int levelCount = 1, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image;
            viewInfo.viewType = viewType;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = aspectFlags;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = levelCount;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = layerCount;

            VkImageView imageView;
            if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image view!");
            }

            return imageView;
        }

    };

    inline static VkHelper vkHelper {};

    /* ------------------- Texture mapping ------------------- */
    struct TextureInfo {
        int texWidth;
        int texHeight;
        int texChannels;
        unsigned char * pixels;
    };

    struct VkTexture {
        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;
        VkDescriptorImageInfo descriptorImageInfo = {};

        VkTexture() = default;
        
        void destroy(){
            vkDestroySampler(device, textureSampler, nullptr);
            vkDestroyImageView(device, textureImageView, nullptr);
            vkDestroyImage(device, textureImage, nullptr);
            vkFreeMemory(device, textureImageMemory, nullptr);
        }

        TextureInfo loadFromFile(const char* texture_file_path, int desired_channels);

        // void free() {
        //     vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
        // }

        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int layerCount = 1, int mipLevel = 0) {
            VkCommandBuffer commandBuffer = vkHelper.beginSingleTimeCommands();
            
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = mipLevel;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = layerCount;

            region.imageOffset = {0, 0, 0};
            region.imageExtent = {
                width,
                height,
                1
            };
            vkCmdCopyBufferToImage(
                commandBuffer,
                buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region
            );

            vkHelper.endSingleTimeCommands(commandBuffer);
        }

        
        void createTextureImage(TextureInfo info, VkFormat format);

        void createTextureImageView(VkFormat format) {
            textureImageView = vkHelper.createImageView(textureImage, format, VK_IMAGE_ASPECT_COLOR_BIT);
        }

        void createTextureSampler(bool isRgbe = false, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS, int mipLevels = 1, VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK) {
            VkFilter filter = VK_FILTER_LINEAR; 
            VkSamplerMipmapMode samplerMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; 
            if(isRgbe) {
                filter = VK_FILTER_NEAREST;
                samplerMipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            }

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            
            samplerInfo.magFilter = filter; //oversampling
            samplerInfo.minFilter = filter; //undersampling
            samplerInfo.addressModeU = samplerAddressMode;
            samplerInfo.addressModeV = samplerAddressMode;
            samplerInfo.addressModeW = samplerAddressMode;
            samplerInfo.anisotropyEnable = VK_TRUE;
            
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
            samplerInfo.borderColor = borderColor;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = compareOp;
            samplerInfo.mipmapMode = samplerMipmapMode;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 4.0f;
            
            if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create texture sampler!");
                }
        }

        void updateDescriptorImageInfo();
    };

    struct VkTexture2D : VkTexture {
        VkTexture2D() : VkTexture() {}

        TextureInfo loadLUTFromBinaryFile(const std::string texture_file_path);

        void load(const std::string texture_file_path, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

        void load(vec3 constant,
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM){
            int edge_len = 4;
            uint8_t data[4*edge_len*edge_len];
            vec<uint8_t,4> constant_int = vec<uint8_t,4>(static_cast<uint8_t>(constant[0]*255), static_cast<uint8_t>(constant[1]*255),static_cast<uint8_t>(constant[2]*255), 0);
            for(int i=0; i<4*edge_len*edge_len; i+=4)
            {
                data[i] = constant_int[0];
                data[i+1] = constant_int[1];
                data[i+2] = constant_int[2];
                data[i+3] = constant_int[3];
            }
            TextureInfo info = {
                edge_len,
                edge_len,
                4,
                static_cast<unsigned char*>(std::malloc(sizeof(data)))
            };
            std::memcpy(info.pixels, data, sizeof(data));
            
            createTextureImage(info, format);//TODO
            createTextureImageView(format);
            createTextureSampler();
            updateDescriptorImageInfo();
        }

        void load(float constant,
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM){
            int edge_len = 4;
            uint8_t data[4*edge_len*edge_len];
            vec<uint8_t,4> constant_int = vec<uint8_t,4>(static_cast<uint8_t>(constant*255), static_cast<uint8_t>(constant*255),static_cast<uint8_t>(constant*255), 0);
            for(int i=0; i<4*edge_len*edge_len; i+=4)
            {
                data[i] = constant_int[0];
                data[i+1] = constant_int[1];
                data[i+2] = constant_int[2];
                data[i+3] = constant_int[3];
            }

            TextureInfo info = {
                edge_len,
                edge_len,
                4,
                static_cast<unsigned char*>(std::malloc(sizeof(data)))
            };
            std::memcpy(info.pixels, &data, sizeof(data));
            createTextureImage(info,format);//TODO
            createTextureImageView(format);
            createTextureSampler();
            updateDescriptorImageInfo();
        }
    };

    struct VkTextureCube : VkTexture {
        VkTextureCube() : VkTexture() {}

        void load(std::string texture_file_path, VkFormat format=VK_FORMAT_R8G8B8A8_UNORM, std::string type = "", bool isRgbe = false
        );

        void createCubeTextureImage(std::vector<TextureInfo>& info, VkFormat format);

        void convertToRadianceValue(TextureInfo& info);

        void createCubeTextureImageView(VkFormat format, int mipLevel = 1) {
            textureImageView = vkHelper.createImageView(textureImage, format, VK_IMAGE_ASPECT_COLOR_BIT, 6, mipLevel, VK_IMAGE_VIEW_TYPE_CUBE);
        }
    };

    struct VkMaterial {
        Material::Type type;
        std::shared_ptr<VkTexture2D> normalMap = nullptr; // default to constant (0,0,1)
        std::shared_ptr<VkTexture2D> displacementMap = nullptr; // default to constant 0
		// PBR texture maps
		std::shared_ptr<VkTexture2D> albedo = nullptr; // default to constant (1,1,1) (also used by lambertian)
        std::shared_ptr<VkTexture2D> metalness = nullptr; // default to constant 0.0
		std::shared_ptr<VkTexture2D> roughness = nullptr; // default to constant 1.0

        void load(std::shared_ptr<Material> material_ptr) {
            type = material_ptr->type;

            normalMap = std::make_shared<VkTexture2D>();
            if(material_ptr->normal_map.has_value()){
                normalMap->load(material_ptr->normal_map->src, VK_FORMAT_R8G8B8A8_UNORM);
            } else {
                normalMap->load(vec3(0,0,1)*0.5+0.5,VK_FORMAT_R8G8B8A8_UNORM);
            }

            displacementMap = std::make_shared<VkTexture2D>();
            if(material_ptr->displacement_map.has_value()){
                displacementMap->load(material_ptr->displacement_map->src, VK_FORMAT_R8G8B8A8_UNORM);
            } else {
                displacementMap->load(0.0, VK_FORMAT_R8G8B8A8_UNORM);
            }

            if(type == Material::Type::LAMBERTIAN){
                Lambertian lamber = material_ptr->lambertian();

                albedo = std::make_shared<VkTexture2D>();
                if(lamber.albedo.has_value()) {
                    albedo->load(lamber.albedo.value(), VK_FORMAT_R8G8B8A8_UNORM);
                } else if (lamber.albedo_texture.has_value()) {
                    albedo->load(lamber.albedo_texture->src, VK_FORMAT_R8G8B8A8_UNORM);
                } else {
                    // default to (1,1,1)
                    albedo->load(vec3(1,1,1), VK_FORMAT_R8G8B8A8_UNORM);
                } 
            } 

            if(type == Material::Type::PBR){
                Pbr pbr = material_ptr->pbr();
                 albedo = std::make_shared<VkTexture2D>();
                if(pbr.albedo.has_value()) {
                    albedo->load(pbr.albedo.value(), VK_FORMAT_R8G8B8A8_UNORM);
                } else if (pbr.albedo_texture.has_value()) {
                    albedo->load(pbr.albedo_texture->src, VK_FORMAT_R8G8B8A8_UNORM);
                } else {
                    // default to (1,1,1)
                    albedo->load(vec3(1,1,1), VK_FORMAT_R8G8B8A8_UNORM);
                } 

                roughness = std::make_shared<VkTexture2D>();
                if(pbr.roughness.has_value()) {
                    roughness->load(pbr.roughness.value(), VK_FORMAT_R8G8B8A8_UNORM);
                } else if (pbr.roughness_texture.has_value()) {
                    roughness->load(pbr.roughness_texture->src, VK_FORMAT_R8G8B8A8_UNORM);
                } else {
                    // default to 1.0
                    roughness->load(1.0, VK_FORMAT_R8G8B8A8_UNORM);
                } 

                metalness = std::make_shared<VkTexture2D>();
                if(pbr.metalness.has_value()) {
                    metalness->load(pbr.metalness.value(), VK_FORMAT_R8G8B8A8_UNORM);
                } else if (pbr.metalness_texture.has_value()) {
                    metalness->load(pbr.metalness_texture->src, VK_FORMAT_R8G8B8A8_UNORM);
                } else {
                    // default to 0.0
                    metalness->load(0.0, VK_FORMAT_R8G8B8A8_UNORM);
                } 
            } 
        }

        void destroy() {
            normalMap->destroy();
            displacementMap->destroy();
            if(albedo!=nullptr){
                albedo->destroy();
            }
            if(metalness!=nullptr){
                metalness->destroy();
            }
            if(roughness!=nullptr){
                roughness->destroy();
            }
        }
    };

    EnvironmentLightingInfo environmentLightingInfo;
    VkTextureCube environmentMap; //cube map texture giving radiance (in watts per steradian per square meter in each spectral band) incoming from the environment
    VkTextureCube lambertianEnvironmentMap;
    VkTextureCube pbrEnvironmentMap;
    VkTexture2D lut; // pbr BRDF look up table

    void loadEnvironment();

    void destroyEnvironment();

    /* ------------------- Model loading & rendering ------------------- */
    struct VkModel {
        ModelPushConstant pc = {}; //model
        std::shared_ptr<Transform> transform;
        std::shared_ptr<Mesh> mesh;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        std::vector<VkDescriptorSet> descriptorSets; 
        VkMaterial material;

        VkModel() = default;

        VkModel(std::shared_ptr<ModelInfo> info_){
            mesh = info_->mesh;
            transform = info_->transform;
            updateModel();
        }

        void destroy(){
            vkDestroyBuffer(device, vertexBuffer, nullptr);
            vkFreeMemory(device, vertexBufferMemory, nullptr);
            vkDestroyBuffer(device, indexBuffer, nullptr);
            vkFreeMemory(device, indexBufferMemory, nullptr);
        }

        void load(){
            createVertexBuffer();
            createIndexBuffer();
            material = {};
            material.load(mesh->material);
        }

        void updateModel() {
            pc.model = transform->model();
            pc.invModel = mat4::transpose(inverse(pc.model));
        }

        void render(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout){
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelPushConstant), &pc);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);//?

            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(commandBuffer,
                            static_cast<uint32_t>(mesh->indices.size()),
                            1,/*number of instances*/
                            0,/*offset into the index buffer*/
                            0,//offset to add to the indices
                                0);//offset for instancing

            // With no index buffer:
            // vkCmdDraw(commandBuffer, static_cast<uint32_t>(mesh->vertices.size()), 1, 0, 0);//vertexCount=3, instanceCount=1, firstVertex=0, firstInstance=0
            
        
        }

        

        void createVertexBuffer() {
            VkDeviceSize bufferSize = sizeof(mesh->vertices.at(0)) * mesh->vertices.size();
            
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            vkHelper.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
            
            void* data;
            vkMapMemory(device, stagingBufferMemory, 0/*offset*/, bufferSize, 0/*flag*/, &data);
            memcpy(data, mesh->vertices.data(), (size_t) bufferSize);
            vkUnmapMemory(device, stagingBufferMemory);
            
            vkHelper.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
            
            vkHelper.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
            
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

        void createIndexBuffer() {
            VkDeviceSize bufferSize = sizeof(mesh->indices[0]) * mesh->indices.size();

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            vkHelper.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, mesh->indices.data(), (size_t) bufferSize);
            vkUnmapMemory(device, stagingBufferMemory);

            vkHelper.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

            vkHelper.copyBuffer(stagingBuffer, indexBuffer, bufferSize);

            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }
    };

    struct VkModelList {
        std::vector<std::shared_ptr<VkModel> > simple_models;
        std::vector<std::shared_ptr<VkModel>> env_models;
        std::vector<std::shared_ptr<VkModel>> mirror_models;
        std::vector<std::shared_ptr<VkModel>> pbr_models;
        std::vector<std::shared_ptr<VkModel>> lamber_models;

        void render(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, const std::string culling=CULLING_NONE, mat4 VP=mat4::I){
            auto renderHelper = [&commandBuffer, &pipelineLayout, &VP, &culling](std::vector<std::shared_ptr<VkModel>>& models, VkPipeline& pipeline){
                if(models.empty()) {
                    return;
                }
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                if(culling == CULLING_NONE) {
                    for(auto model: models){
                        model->updateModel();
                        model->render(commandBuffer, pipelineLayout);
                    }
                } else {
                    for(auto model: models){
                        model->updateModel();
                        mat4 MVP = VP * model->pc.model;
                        if(frustum_cull_test(MVP, model->mesh->bbox)){
                            model->render(commandBuffer, pipelineLayout);
                        }
                    }
                }
            };
            renderHelper(simple_models, pipelines.simple);
            renderHelper(env_models, pipelines.env);
            renderHelper(mirror_models, pipelines.mirror);
            renderHelper(pbr_models, pipelines.pbr);
            renderHelper(lamber_models, pipelines.lamber);
            
        }

        std::vector<std::shared_ptr<VkModel>> getAllModels(){
            std::vector<std::shared_ptr<VkModel>> models;
            models.insert(models.end(), simple_models.begin(), simple_models.end());
            models.insert(models.end(), env_models.begin(), env_models.end());
            models.insert(models.end(), mirror_models.begin(), mirror_models.end());
            models.insert(models.end(), pbr_models.begin(), pbr_models.end());
            models.insert(models.end(), lamber_models.begin(), lamber_models.end());

            return models;
        }

        void destroyMaterial() {
            auto destroyMaterialHelper = [](std::vector<std::shared_ptr<VkModel>>& models) {
                for(auto model: models){
                    model->material.destroy();
                }
            };
            destroyMaterialHelper(simple_models);
            destroyMaterialHelper(env_models);
            destroyMaterialHelper(mirror_models);
            destroyMaterialHelper(pbr_models);
            destroyMaterialHelper(lamber_models);
        }

        void destroy() {
            auto destroyHelper = [](std::vector<std::shared_ptr<VkModel>>& models) {
                for(auto model: models){
                    model->destroy();
                }
            };
            destroyHelper(simple_models);
            destroyHelper(env_models);
            destroyHelper(mirror_models);
            destroyHelper(pbr_models);
            destroyHelper(lamber_models);
        }

    } model_list;

    int vertices_count = 0;

    


    /* ---------------- Load models ---------------- */
    void createModels();

    /** ---------------- main steps ---------------- */
    
    void initVulkan();
    
    void mainLoop();
    
    void cleanUp();
    
    
    
    void createInstance();
    
    /* ------------ Set up debug messenger ------------ */
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    
    void setupDebugMessenger();
    
    /* ------------ Select physical device ------------ */
    bool isDeviceSuitable(VkPhysicalDevice device);
    
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    
    void pickPysicalDevice();
    
    
    /* ----------- Logical Device ----------- */
    void createLogicalDevice();
    
    /* ------------ Queue ------------ */
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    
    
    
    /* ------------ Validation layer ------------ */
    std::vector<const char*> getRequiredExtensions();
    
    bool checkValidationLayerSupport();
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData){
        std::cerr<<"validation layer: "<<pCallbackData->pMessage<<std::endl;
        
        return VK_FALSE;
    }
    
    /* ----------- Surface ----------- */
    // moved to window controller
    void createHeadlessSurface(VkInstance& instance, VkSurfaceKHR* surface);
    
    /* ---------- Swap chian ----------- */
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    
    void createSwapChain();
    
    /* ------------ Image View ------------ */
    void createImageViews();
    
    /* ------------- Graphics Pipeline ------------- */
    void createGraphicsPipeline();
    
    VkShaderModule createShaderModule(const std::vector<char>& code);

    VkPipelineShaderStageCreateInfo loadShader(const std::string shaderFileName, VkShaderStageFlagBits stageFlag);
    
    void createRenderPass();
    
    /* ----------- Frame Buffer  ----------- */
    // create a framebuffer for all of the images in the swap chain and use the one that corresponds to the retrieved image at drawing time
    void createFramebuffers();
    
    /* ----------- Drawing commands ----------- */
    void createCommandPool();
    
    void createCommandBuffers();
    
    void recordCommandBuffer(VkCommandBuffer commandBuffer);
    
    void drawFrame();
    
    
    /* ------- Creating the synchronization objects -------*/
    void createSyncObjects();
    
    /* ---- Recreate swap chain (in case of window size change) ---- */
    void cleanupSwapChain();

    void recreateSwapChain();
    
    
    
    /* ---------- Vertex Buffer ---------- */
    
    /* ------------- Index Buffer ------------- */
    
    /* --------------- Uniform buffers --------------- */
    void createUniformBuffers();
    
    void updateUniformBuffer(uint32_t currentImage);

    /* --------------- Descriptor sets --------------- */

    VkDescriptorSetLayoutBinding createDescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount = 1);

    void createDescriptorSetLayout();
    
    void createDescriptorPool();

    void allocateDescriptorSet(std::vector<VkDescriptorSet>& descriptorSets);
    
    void createModelDescriptorSets(std::shared_ptr<VkModel> model);

    void createDescriptorSets();

    VkWriteDescriptorSet writeDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType type, uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount = 1);

    VkWriteDescriptorSet writeDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType type, uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t descriptorCount = 1);
    
    /* ------------------- Texture mapping ------------------- */
    // void createTextureImage();
    
    // void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    
    // VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    
    // void createTextureImageView();
    
    /* ------------ Texture Sampler ------------ */
    // void createTextureSampler();

    /* ------------------ Defth Buffer ------------------ */
    void createDepthResources();
    
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkFormat findDepthFormat();

    bool hasStencilComponent(VkFormat format);
    
    /* ------------------ For events processing ------------------ */
    void eventLoop();

    // Save the most-recently-rendered image in the swap chain to file
    // Take a screenshot from the current swapchain image
    // Referenced from https://github.com/SaschaWillems/Vulkan/blob/master/examples/screenshot/screenshot.cpp
    void screenshotSwapChain(const std::string& filename);

};

/* viewer_h */
