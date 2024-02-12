//
//  viewer.h
//  VulkanTesting
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
#define STB_IMAGE_IMPLEMENTATION
// #include "stb_image.h"

// #define GLM_FORCE_RADIANS
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>

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

#include "file.hpp"
#include "vertex.hpp"
#include "scene.h"
#include "mathlib.h"
#include "bbox.h"
#include "constants.h"
#include "camera_controller.h"
#include "input_controller.h"
#include "window_controller.h"
#include "animation_controller.h"
#include "events_controller.h"

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

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

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
    std::vector<std::shared_ptr<ModelInfo>> modelInfos;
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

    UniformBufferObject ubo = {};
    
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
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
    VkPipeline graphicsPipeline;
    
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
    static inline VkCommandPool commandPool = NULL;
    std::vector<VkCommandBuffer> commandBuffers;
    
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    
    bool framebufferResized = false;
    uint32_t currentFrame = 0;
    
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    
    // VkImage textureImage;
    // VkDeviceMemory textureImageMemory;
    // VkImageView textureImageView;
    // VkSampler textureSampler;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    struct BufferHelper {
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
    };

    BufferHelper bufferHelper {};

    struct VkModel:BufferHelper {

        ModelPushConstant pc = {}; //model
        std::shared_ptr<ModelInfo> info; //mesh and transform
        // std::vector<uint16_t> indices
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        VkModel() = default;

        VkModel(std::shared_ptr<ModelInfo> info_){
            info = info_;
            pc.model = info->model();
        }

        void cleanUp(){
            vkDestroyBuffer(device, vertexBuffer, nullptr);
            vkFreeMemory(device, vertexBufferMemory, nullptr);
            if(ENABLE_INDEX_BUFFER) {
                vkDestroyBuffer(device, indexBuffer, nullptr);
                vkFreeMemory(device, indexBufferMemory, nullptr);
            }
        }

        void load(){
            createVertexBuffer();
            if(ENABLE_INDEX_BUFFER) {
                createIndexBuffer();
            }
        }

        void updateModel() {
            pc.model = info->model();
        }

        void render(VkCommandBuffer& commandBuffer){
            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            if(ENABLE_INDEX_BUFFER) {
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(commandBuffer,
                                static_cast<uint32_t>(info->mesh->indices.size()),
                                1,/*number of instances*/
                                0,/*offset into the index buffer*/
                                0,//offset to add to the indices
                                0);//offset for instancing
            }
            vkCmdDraw(commandBuffer, static_cast<uint32_t>(info->mesh->vertices.size()), 1, 0, 0);//vertexCount=3, instanceCount=1, firstVertex=0, firstInstance=0
        
        }

        

        void createVertexBuffer() {
            VkDeviceSize bufferSize = sizeof(info->mesh->vertices.at(0)) * info->mesh->vertices.size();
            
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
            
            void* data;
            vkMapMemory(device, stagingBufferMemory, 0/*offset*/, bufferSize, 0/*flag*/, &data);
            memcpy(data, info->mesh->vertices.data(), (size_t) bufferSize);
            vkUnmapMemory(device, stagingBufferMemory);
            
            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
            
            copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
            
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

        void createIndexBuffer() {
            VkDeviceSize bufferSize = sizeof(info->mesh->indices[0]) * info->mesh->indices.size();

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, info->mesh->indices.data(), (size_t) bufferSize);
            vkUnmapMemory(device, stagingBufferMemory);

            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

            copyBuffer(stagingBuffer, indexBuffer, bufferSize);

            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }
    };

    std::vector<VkModel> models;

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
    void createDescriptorSetLayout();
    
    void createUniformBuffers();
    
    void updateUniformBuffer(uint32_t currentImage);
    
    void createDescriptorPool();
    
    void createDescriptorSets();
    
    /* ------------------- Texture mapping ------------------- */
    // void createTextureImage();
    
    void createImage(uint32_t width, uint32_t height, VkFormat format, 
                    VkImageTiling tiling, VkImageUsageFlags usage, 
                    VkMemoryPropertyFlags properties, VkImage& image, 
                    VkDeviceMemory& imageMemory);

    void transitionImageLayout(VkImage image, 
                    VkImageLayout oldLayout, VkImageLayout newLayout,
                    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
			        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);
    
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    
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
