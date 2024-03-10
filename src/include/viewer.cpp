#include "viewer.h"
#include "math/math_util.h"
#include "scene/material.h"
#include "vk/vk_debug.h"
#include "vk/vk_helper.h"
#include "file.hpp"

#include <cfloat>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "utils/stb_image.h"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    
    bool isComplete(){
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

void ViewerApplication::setUpScene(const std::string& file_name){
    Scene scene;
    scene.init(file_name);
    camera_controller = std::make_shared<CameraController>(scene.getAllCameras(), width, height);
    animation_controller = std::make_shared<AnimationController>(scene.getDrivers());
    model_info_list = scene.getModelInfos();
    environmentLightingInfo = scene.getEnvironment();
}

void ViewerApplication::setCamera(const std::string& camera_name) {
    camera_controller->setCamera(camera_name);
}

void ViewerApplication::setPhysicalDevice(const std::string& _physical_device_name){
    physical_device_name = _physical_device_name;
}

void ViewerApplication::setDrawingSize(const int& w, const int& h){
    width = w;
    height = h;
    camera_controller->setHeightWdith(height, width);
}

void ViewerApplication::setCulling(const std::string& culling_) {
    culling = culling_;
}

void ViewerApplication::setHeadless(const std::string& event_file_name){
    headless = true;
    event_controller = std::make_shared<EventsController>();
    event_controller->load(event_file_name);
}

void ViewerApplication::enableMeasure(){
    does_measure = true;
}

void ViewerApplication::run(){
    if(!headless) {
        window_controller = std::make_shared<WindowController>();
        window_controller->initWindow(width, height);
        input_controller = std::make_shared<InputController>();
        input_controller->setCameraController(camera_controller);
        input_controller->setAnimationController(animation_controller);
        input_controller->setKeyCallback(window_controller->getWindow());
        camera_controller->setWindow(window_controller->getWindow());
    }
    initVulkan();
    mainLoop();
    cleanUp();
}

void ViewerApplication::listPhysicalDevice(){
    createInstance();
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::cout<<"List physical devices:\n";
    for(const auto& device: devices){
        VkPhysicalDeviceProperties deviceProperty;
        vkGetPhysicalDeviceProperties(device, &deviceProperty);
        std::cout<<deviceProperty.deviceName<<"\n";
    }

}


/* ---------------- Load models ---------------- */
void ViewerApplication::createModels(){
    auto loadModelInfo = [&](std::vector<std::shared_ptr<ModelInfo>>& model_infos, std::vector<std::shared_ptr<VkModel>>& model_list){
        for(auto info: model_infos){
            model_list.push_back(std::make_shared<VkModel>(info));
            model_list.back()->load();
            vertices_count += model_list.back()->mesh->vertices.size();
        }
    };
    loadModelInfo(model_info_list.simple_models, model_list.simple_models);
    loadModelInfo(model_info_list.env_models, model_list.env_models);
    loadModelInfo(model_info_list.mirror_models, model_list.mirror_models);
    loadModelInfo(model_info_list.pbr_models, model_list.pbr_models);
    loadModelInfo(model_info_list.lamber_models, model_list.lamber_models);

    std::cout<<"Total vertices count: "<<vertices_count<<"\n";
}

/** ---------------- main steps ---------------- */

void ViewerApplication::initVulkan(){
    createInstance();
    setupDebugMessenger(); 
    if(!headless) {
        window_controller->createSurface(instance, &surface);
    }
    else {
        createHeadlessSurface(instance, &surface);
    }
    pickPysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    
    createCommandPool();
    createDepthResources();
    createFramebuffers();

    loadEnvironment();
    createModels();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void ViewerApplication::mainLoop(){
    if(!headless) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        while(!window_controller->shouldClose()){
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            if(does_measure){
                total_time += deltaTime;
                ++frame_count;
                std::cout<<"MEASURE frame "<<deltaTime<<"\n";
                if(frame_count == MAX_FRAME_COUNT){
                    std::cout<<"Total time: "<<total_time<<"\n";
                    break;
                }
            }
            currentTime = newTime;
            camera_controller->moveCamera(deltaTime);
            animation_controller->driveAnimation(deltaTime);

            drawFrame();
        }
    } else {
        // Execute event
        eventLoop();
    }
    
    vkDeviceWaitIdle(device);
}

void ViewerApplication::cleanUp(){
    cleanupSwapChain();
    
    // destroy materials and textures
    model_list.destroyMaterial();
    destroyEnvironment();
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    
    model_list.destroy();
    
    pipelines.destroy();
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);

    vkDestroyRenderPass(device, renderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyDevice(device, nullptr);

    if (enableValidationLayers) {
        vk::DestroyDebugUtilsMessengerEXT(instance, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    if(!headless) {
        window_controller->destroy();
    }

    
   
}



void ViewerApplication::createInstance(){
    // check layers
    if(enableValidationLayers && !checkValidationLayerSupport()){
        throw std::runtime_error("validation layer requested, but not available!");
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    // set up debug messenger for createInstance process
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    // set up layers
    if(enableValidationLayers){
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        
        createInfo.pNext = nullptr;
    }
    
    // set up extensions  
    std::vector<const char *> requiredExtensions = getRequiredExtensions();

    // check extensions
    requiredExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    requiredExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    requiredExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);

    std::cout<<"rerequired extensions:\n";
    for(const auto& extension: requiredExtensions){
        std::cout<<"\t"<<extension<<"\n";
    }
    
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    
    createInfo.enabledExtensionCount = (uint32_t) requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> extensions(extensionCount);
    
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    
    std::cout<<"available extensions:\n";
    for(const auto& extensions: extensions){
        std::cout<<"\t"<<extensions.extensionName<<"\n";
    }
    
    if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS){
        throw std::runtime_error("failed to create instance!");
    }

}

/* ------------ Set up debug messenger ------------ */
void ViewerApplication::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo){
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; //Optional
}

void ViewerApplication::setupDebugMessenger(){
    if(!enableValidationLayers) return;
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);
    
    if(vk::CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr)!=VK_SUCCESS){
        throw std::runtime_error("fail to set up debug messenger!");
    }
}

/* ------------ Select physical device ------------ */
bool ViewerApplication::isDeviceSuitable(VkPhysicalDevice device){
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    
    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool ViewerApplication::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,availableExtensions.data());
    
    std::set<std::string> requiredExtensions(deviceExtensions.begin(),deviceExtensions.end());
    
    for(const auto& extension: availableExtensions){
        requiredExtensions.erase(extension.extensionName);
    }
    
    return requiredExtensions.empty();
}

void ViewerApplication::pickPysicalDevice(){
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    if(deviceCount == 0){
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    
    if(physical_device_name!="None") {
        bool found = false;
        for(const auto& device: devices){
            VkPhysicalDeviceProperties deviceProperty;
            vkGetPhysicalDeviceProperties(device, &deviceProperty);
            std::cout<<deviceProperty.deviceName<<"\n";
            if(deviceProperty.deviceName == physical_device_name){
                if(isDeviceSuitable(device)){
                    physicalDevice = device;
                    found = true;
                    break;
                } else {
                    throw std::runtime_error("required physical device is unsuitable!");
                }
            }
        }
        if(!found) {
            throw std::runtime_error("required physical device not found!");
        }
    } else {
        for(const auto& device: devices){
            if(isDeviceSuitable(device)){
                physicalDevice = device;
                break;
            }
        }
    }

    if(physicalDevice==VK_NULL_HANDLE){
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}


/* ----------- Logical Device ----------- */
void ViewerApplication::createLogicalDevice(){
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    
    float queuePriority = 1.0f;
    for(uint32_t queueFamily: uniqueQueueFamilies){
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    
    createInfo.pEnabledFeatures = &deviceFeatures;
    
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if(enableValidationLayers){
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    
    if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device)!=VK_SUCCESS){
        throw std::runtime_error("failed to create logical device");
    }
    
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

/* ------------ Queue ------------ */

QueueFamilyIndices ViewerApplication::findQueueFamilies(VkPhysicalDevice device){
    QueueFamilyIndices indices;
    // Assign index to queue families that could be found
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    int i = 0;
    for(const auto& queueFamily: queueFamilies){
        if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if(presentSupport) {
            indices.presentFamily = i;
        }
        if(indices.isComplete())
            break;
        
        i++;
    }
    
    return indices;
}



/* ------------ Validation layer ------------ */
std::vector<const char*> ViewerApplication::getRequiredExtensions(){
    std::vector<const char*> extensions;
    if(headless) {
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    } else {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        extensions =  std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
    }

    if(enableValidationLayers){
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    return extensions;
}

bool ViewerApplication::checkValidationLayerSupport(){
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for(const char* layerName: validationLayers){
        bool layerFound = false;
        
        for(const auto& layerProperties: availableLayers){
            if(strcmp(layerName, layerProperties.layerName)==0){
                layerFound = true;
                break;
            }
        }
        
        if(!layerFound)
            return false;
    }
    
    return true;
}



/* ----------- Surface ----------- */
// create window surface moved to window controller

void ViewerApplication::createHeadlessSurface(VkInstance& instance, VkSurfaceKHR* surface){
    VkHeadlessSurfaceCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;

    if(vkCreateHeadlessSurfaceEXT(instance, &createInfo, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create headless surface");
    }
}

/* ---------- Swap chian ----------- */
SwapChainSupportDetails ViewerApplication::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }
    
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR ViewerApplication::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    
    return availableFormats[0];
}

VkPresentModeKHR ViewerApplication::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ViewerApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        if(!headless) {
            window_controller->getFramebufferSize(&width, &height);
        } 

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void ViewerApplication::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Enable transfer source on swap chain images if supported
	if (swapChainSupport.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
		createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	} else {
        std::cout<<"Swap chain VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported\n";
    }
    
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }
    
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }
    
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

/* ------------ Image View ------------ */
void ViewerApplication::createImageViews(){
    swapChainImageViews.resize(swapChainImages.size());
    
    for(size_t i=0; i<swapChainImages.size(); i++){
        swapChainImageViews[i] = vkHelper.createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

/* ------------- Graphics Pipeline ------------- */
void ViewerApplication::createGraphicsPipeline() {
    // Create Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache), "fail to create pipeline cache");


    // Programmable shader stages
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    
    // Fixed function stage
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
    
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    // Alpha blending
//        colorBlendAttachment.blendEnable = VK_TRUE;
//        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
//        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
//        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
//        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
//        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
//        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    //setup push constants
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ModelPushConstant);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 1; // Optional
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange; // Optional

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
    
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional
    
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    // Simple pipeline
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    shaderStages[0] = loadShader(SIMPLE_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(SIMPLE_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create Simple pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.simple), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // Environment pipeline
    shaderStages[0] = loadShader(ENV_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(ENV_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create Env pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.env), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // Mirror pipeline
    shaderStages[0] = loadShader(MIRROR_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(MIRROR_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create Mirror pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.mirror), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
    
    // Lambertian pipeline
    shaderStages[0] = loadShader(LAMBER_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(LAMBER_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create Lambertian pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.lamber), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // Pbr pipeline
    shaderStages[0] = loadShader(PBR_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(PBR_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create Pbr pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.pbr), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
}

VkShaderModule ViewerApplication::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

VkPipelineShaderStageCreateInfo ViewerApplication::loadShader(const std::string shaderFileName, VkShaderStageFlagBits stageFlag) {
    auto shaderCode = readFile(shaderFileName);
    VkShaderModule shaderModule = createShaderModule(shaderCode);
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = stageFlag;
    vertShaderStageInfo.module = shaderModule;
    vertShaderStageInfo.pName = "main";

    return vertShaderStageInfo;
}

void ViewerApplication::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//color and depth data
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
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
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

/* ----------- Frame Buffer  ----------- */
// create a framebuffer for all of the images in the swap chain and use the one that corresponds to the retrieved image at drawing time
void ViewerApplication::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());
    
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

/* ----------- Drawing commands ----------- */
void ViewerApplication::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void ViewerApplication::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void ViewerApplication::recordCommandBuffer(VkCommandBuffer commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    if(culling == CULLING_NONE) {
        model_list.render(commandBuffer, pipelineLayout);
    } else if (culling == CULLING_FRUSTUM){
        // frustum culling
        mat4 VP;
        if(camera_controller->isDebug()) {
            VP = camera_controller->getPrevPerspective() * camera_controller->getPrevView();
        } else {
            VP = ubo.proj * ubo.view;
        }
        
        model_list.render(commandBuffer, pipelineLayout, culling, VP);
    }

    
    
    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}


void ViewerApplication::drawFrame() {
    //1. Wait for the previous frame to finish
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE /*wait for all fences*/, UINT64_MAX /*disables the timeout*/);
    
    //2. Acquire an image from the swap chain
    
    VkResult result = vkAcquireNextImageKHR(device,
                            swapChain,
                            UINT64_MAX,//timeout in nanoseconds for an image to become available
                            imageAvailableSemaphores[currentFrame],//signaled when the presentation engine is finished using the image
                            VK_NULL_HANDLE,
                            &imageIndex);//index of VkImage in swapChainImages
    if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    // Only reset the fence if we are submitting work
    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    //3. Record a command buffer which draws the scene onto that image
    vkResetCommandBuffer(commandBuffers[currentFrame], 0/*VkCommandBufferResetFlagBits*/);
    
    updateUniformBuffer(currentFrame);

    recordCommandBuffer(commandBuffers[currentFrame]);
    
    //4. Submit the recorded command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
    
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    //5. Present the swap chain image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    presentInfo.pResults = nullptr; // Optional
    
    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if(!headless) {
        // only check result if not headless
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window_controller->wasResized()) {
            window_controller->resetResized();
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
    } 
    

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}


/* ------- Creating the synchronization objects -------*/
void ViewerApplication::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }
}

/* ---- Recreate swap chain (in case of window size change) ---- */
void ViewerApplication::cleanupSwapChain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);
    
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void ViewerApplication::recreateSwapChain() {
    int width = 0, height = 0;
    window_controller->getFramebufferSize(&width, &height);
    
    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();

    // update projection matrix
    camera_controller->setHeightWdith(swapChainExtent.height, swapChainExtent.width);
}



/* ---------- Vertex Buffer ---------- */

/* ------------- Index Buffer ------------- */

/* --------------- Uniform buffers --------------- */

void ViewerApplication::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkHelper.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void ViewerApplication::updateUniformBuffer(uint32_t currentImage) {
    ubo.proj = camera_controller->getPerspective();
    ubo.proj[1][1] *= -1;
    ubo.view = camera_controller->getView();
    ubo.light = environmentLightingInfo.transform->worldToLocal(); // transform from world space to environment space
    // std::cout<<"ubo.light: "<<ubo.light;
    
    vec4 eyePos = camera_controller->getEyePos();
    ubo.eye = vec3(eyePos[0], eyePos[1], eyePos[2]);
    // std::cout<<"ubo.eye: "<<ubo.eye;
    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

/* --------------------- Decriptor Sets --------------------- */
VkDescriptorSetLayoutBinding ViewerApplication::createDescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = descriptorCount;
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = nullptr; // Optional
    return layoutBinding;
}


void ViewerApplication::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding = createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,  /*binding=*/0, 1);
    

    std::vector<VkDescriptorSetLayoutBinding> bindings = {uboLayoutBinding, 
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/1, 1),
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/2, 1),
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/3, 1),
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/4, 1),
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/5, 1),
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/6, 1),
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/7, 1),
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/8, 1)};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void ViewerApplication::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 1000;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 1000; //?

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1000;
    
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

VkWriteDescriptorSet ViewerApplication::writeDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType type, uint32_t dstBinding, VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount) {
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = dstBinding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = descriptorCount;
    descriptorWrite.pBufferInfo = bufferInfo;
    return descriptorWrite;
}

VkWriteDescriptorSet ViewerApplication::writeDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType type, uint32_t dstBinding, VkDescriptorImageInfo* imageInfo, uint32_t descriptorCount) {
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = dstBinding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = descriptorCount;
    descriptorWrite.pImageInfo = imageInfo;
    return descriptorWrite;
}

void ViewerApplication::allocateDescriptorSet(std::vector<VkDescriptorSet>& descriptorSets){
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
}

void ViewerApplication::VkTexture::updateDescriptorImageInfo(){
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo.imageView = textureImageView;
    descriptorImageInfo.sampler = textureSampler;
}

void ViewerApplication::createModelDescriptorSets(std::shared_ptr<VkModel> model) {
    allocateDescriptorSet(model->descriptorSets);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        // Add uniform buffer descriptor
        // Add normal map and displacement map is common for all material
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/0, &bufferInfo, 1),
            writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/1, &(model->material.normalMap->descriptorImageInfo), 1),
            writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/2, &(model->material.displacementMap->descriptorImageInfo), 1)
        };
        
        if(model->material.type == Material::Type::ENVIRONMENT || 
        model->material.type == Material::Type::MIRROR) {
            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/3, &(environmentMap.descriptorImageInfo), 1)
            );
        } else if(model->material.type == Material::Type::SIMPLE) {
            // No other texture to add
            
        } else if(model->material.type == Material::Type::LAMBERTIAN) {
            // Add environment, albedo
            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/3, &(lambertianEnvironmentMap.descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/4, &(model->material.albedo->descriptorImageInfo), 1)
            );
        } else if(model->material.type == Material::Type::PBR) {
            // Add prefiltered (lambertian) environment, PBR environment, LUT,  albedo, metalness, roughness
            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/3, &(lambertianEnvironmentMap.descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/4, &(pbrEnvironmentMap.descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/5, &(lut.descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &(model->material.albedo->descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/7, &(model->material.metalness->descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/8, &(model->material.roughness->descriptorImageInfo), 1)
            );
        }
        
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }
}

void ViewerApplication::createDescriptorSets() {
    for(auto model : model_list.getAllModels()) {
        createModelDescriptorSets(model);
    }
}












/* ------------ Texture  ------------ */

ViewerApplication::TextureInfo ViewerApplication::VkTexture::loadFromFile(const char* texture_file_path, int desired_channels = STBI_rgb_alpha) {
    TextureInfo info = {};
    // load texture in top-left mode
    info.pixels = stbi_load(texture_file_path, &info.texWidth, &info.texHeight, &info.texChannels, desired_channels);
    if (!info.pixels) {
        throw std::runtime_error("failed to load texture image at: "+std::string(texture_file_path));
    }
    std::cout<<texture_file_path<<" - width, height: "<<info.texWidth<<", "<<info.texHeight<<"\n";

    return info;
}

void ViewerApplication::VkTexture::createTextureImage(TextureInfo info, VkFormat format) {
    VkDeviceSize imageSize = info.texWidth * info.texHeight * info.texChannels;
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vkHelper.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, info.pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);
    
    stbi_image_free(info.pixels);
    
    vkHelper.createImage(info.texWidth, info.texHeight, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
    
    vkHelper.transitionImageLayout(textureImage, 
        VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);
    copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(info.texWidth), static_cast<uint32_t>(info.texHeight));
    
    vkHelper.transitionImageLayout(textureImage,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_ACCESS_SHADER_READ_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

ViewerApplication::TextureInfo ViewerApplication::VkTexture2D::loadLUTFromBinaryFile(const std::string texture_file_path) {
    int file_size = 512*512;
    std::ifstream zIn(texture_file_path, std::ios::in | std::ios::binary);
    float *chBuffer = new float[file_size * 2];
    zIn.read(reinterpret_cast<char*>(chBuffer), sizeof(float)*file_size *2);
    zIn.close();
    std::cout<<chBuffer[0]<<","<<chBuffer[1]<<"\n";
    
    TextureInfo info;
    info.texHeight = 512;
    info.texWidth = 512;
    info.texChannels = 2*sizeof(float);
    info.pixels = reinterpret_cast<unsigned char*>(chBuffer);
    return info;
}

void ViewerApplication::VkTexture2D::load(const std::string texture_file_path, VkFormat format){
    TextureInfo info;
    if (texture_file_path.find("txt") != std::string::npos) {
        // lutBrdf in binary file ends with txt
        info = loadLUTFromBinaryFile(texture_file_path);
        
    } else if (texture_file_path.find("png") != std::string::npos){
        stbi_set_flip_vertically_on_load(true);
        info = loadFromFile(texture_file_path.c_str(), STBI_rgb_alpha); //load 4 channels
    } else {
        throw std::runtime_error("texture file format not supported: "+texture_file_path);
    }
    
    
    createTextureImage(info,format);
    createTextureImageView(format);
    createTextureSampler(false, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_COMPARE_OP_NEVER, 1, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
    updateDescriptorImageInfo();
}

// reference: old RGBE-handling from 15-466 https://github.com/ixchow/15-466-ibl/blob/master/rgbe.hpp
void ViewerApplication::VkTextureCube::convertToRadianceValue(TextureInfo& info) {
    int width = info.texWidth;
    int height = info.texHeight;
    int total_pixels = width*height;
    uint8_t * buffer = static_cast<uint8_t*>(info.pixels);
    float pixels[total_pixels*4];
    for(int i=0; i<total_pixels*4; i+=4) {
        vec3 rgb = vec3(static_cast<float>(buffer[i]), static_cast<float>(buffer[i+1]), static_cast<float>(buffer[i+2]));
        // (0,0,0,0) should be mapped to (0,0,0)
        if(rgb != vec3()) {
            int exp = static_cast<int>(buffer[i+3]) - 128;

            pixels[i] = std::ldexp((rgb[0]+0.5f) / 256.0f, exp);
            pixels[i+1] = std::ldexp((rgb[1]+0.5f) / 256.0f, exp);
            pixels[i+2] = std::ldexp((rgb[2]+0.5f) / 256.0f, exp);
        }
        // std::cout<<static_cast<int>(buffer[i])<<","<<static_cast<int>(buffer[i+1])<<static_cast<int>(buffer[i+2])<<"\n";
        
    }
    std::cout<<static_cast<int>(buffer[0])<<"\n";
    std::cout<<static_cast<int>(buffer[1])<<"\n";
    std::cout<<static_cast<int>(buffer[2])<<"\n";
    std::cout<<static_cast<int>(buffer[3])<<"\n";
}

void ViewerApplication::VkTextureCube::load(std::string texture_file_path, VkFormat format, std::string type, bool isRgbe){
    stbi_set_flip_vertically_on_load(false);
    if (type == "") {
        // load original environment map
        
        std::vector<TextureInfo> infos = {loadFromFile(texture_file_path.c_str(), STBI_rgb_alpha)}; 
        std::cout<<"Load original environment map "<<texture_file_path<<"\n";
        createCubeTextureImage(infos, format);
        createCubeTextureImageView(format);
        createTextureSampler(isRgbe, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, 1);
    } else if (type == "lambertian"){
        
        // load prefiltered environment map for lambertian diffuse
        std::string common_file_path = texture_file_path.substr(0, texture_file_path.find_last_of("."));
        std::vector<TextureInfo> infos = {loadFromFile((common_file_path+".lambertian.png").c_str(), STBI_rgb_alpha)};
        std::cout<<"Load prefiltered environment map for lambertian diffuse "<<(common_file_path+".lambertian.png")<<"\n"; 
        createCubeTextureImage(infos, format);
        createCubeTextureImageView(format);
        createTextureSampler(isRgbe, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, 1);
    } else if (type == "pbr") {
        // load prefiltered environment maps for PBR
        std::vector<TextureInfo> infos;
        std::string common_file_path = texture_file_path.substr(0, texture_file_path.find_last_of("."));
        for(int i=0; i<ENVIRONMENT_MIP_LEVEL; ++i) {
            std::string mip_file_path = common_file_path+".ggx."+std::to_string(i)+".png";
            infos.push_back(loadFromFile(mip_file_path.c_str(), STBI_rgb_alpha)); 
            std::cout<<"Mipmap "<<mip_file_path<<": "<<infos.back().texWidth<<","<<infos.back().texHeight<<"\n";
        }
        
        createCubeTextureImage(infos, format);
        createCubeTextureImageView(format, ENVIRONMENT_MIP_LEVEL);
        createTextureSampler(isRgbe, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, ENVIRONMENT_MIP_LEVEL);
    }
    updateDescriptorImageInfo();
}

void ViewerApplication::VkTextureCube::createCubeTextureImage(std::vector<TextureInfo>& infos, VkFormat format) {
    int mipLevels = infos.size();

    VkDeviceSize imageSize{0};
    for(auto& info: infos) {
        imageSize += info.texWidth * info.texHeight * info.texChannels;
    }
    
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vkHelper.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    VkDeviceSize currentOffset{ 0 };
    for(auto& info: infos) {
        int mipSize = info.texWidth * info.texHeight * info.texChannels;
        memcpy(static_cast<std::byte*>(data) + currentOffset, info.pixels, static_cast<size_t>(mipSize));
        currentOffset += mipSize;
    }
    
    vkUnmapMemory(device, stagingBufferMemory);
    
    for(auto& info: infos) {
        stbi_image_free(info.pixels);
    }
    

    std::vector<VkBufferImageCopy> bufferCopyRegions;
    currentOffset = 0;
    // Order of image: front, back, up, down, right, left
    
    for(int level=0; level<mipLevels; ++level) {
        for (uint32_t face = 0; face < 6; face++) {
            auto& info = infos[level];
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = face;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = info.texWidth;
            bufferCopyRegion.imageExtent.height = info.texHeight / 6;
            bufferCopyRegion.imageExtent.depth = 1;
            // bufferCopyRegion.imageOffset = { 0, 0, 0 };
            bufferCopyRegion.bufferOffset = currentOffset; //in bytes
            bufferCopyRegion.bufferRowLength = info.texWidth;
            bufferCopyRegions.push_back(bufferCopyRegion);

            currentOffset += info.texWidth * info.texHeight / 6 * info.texChannels;
        }
    }
    
    
    vkHelper.createImage(infos[0].texWidth, infos[0].texHeight / 6, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, mipLevels);
    
    vkHelper.transitionImageLayout(textureImage, 
        VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        mipLevels,/*mipmap levelCount*/
        6/*layerCount*/);
    VkCommandBuffer commandBuffer = vkHelper.beginSingleTimeCommands();

    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer,
        textureImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(bufferCopyRegions.size()),
        bufferCopyRegions.data());

    vkHelper.endSingleTimeCommands(commandBuffer);
    
    vkHelper.transitionImageLayout(textureImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        mipLevels,/*mipmap levelCount*/
        6/*layerCount*/);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void ViewerApplication::loadEnvironment() {
    if(environmentLightingInfo.exist) {
        environmentMap.load(environmentLightingInfo.texture.src, VK_FORMAT_R8G8B8A8_UNORM, "", true);
        if(!model_info_list.lamber_models.empty() || !model_info_list.pbr_models.empty()) {
            lambertianEnvironmentMap.load(environmentLightingInfo.texture.src, VK_FORMAT_R8G8B8A8_UNORM, "lambertian", true);
        }
        if(!model_info_list.pbr_models.empty()) {
            std::string src = environmentLightingInfo.texture.src;
            pbrEnvironmentMap.load(src, VK_FORMAT_R8G8B8A8_UNORM, "pbr", true);
            
            std::string lut_file_path = src.substr(0, src.find_last_of("."))+".lut.png";
            // lut.load(lut_file_path, VK_FORMAT_R16G16_SFLOAT);
            lut.load(lut_file_path, VK_FORMAT_R8G8B8A8_UNORM);
        }
    }
}

void ViewerApplication::destroyEnvironment() {
    environmentMap.destroy();
    lambertianEnvironmentMap.destroy();
    pbrEnvironmentMap.destroy();
    lut.destroy();
}


/* ------------------ Defth Buffer ------------------ */
void ViewerApplication::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    vkHelper.createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = vkHelper.createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}      

VkFormat ViewerApplication::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

VkFormat ViewerApplication::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool ViewerApplication::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

/* ------------------ Animation ------------------ */
void ViewerApplication::disableAnimationLoop() {
    animation_controller->disableLoop();
}

/* ------------------ For events processing ------------------ */
void ViewerApplication::eventLoop() {
    auto current_time_measure = std::chrono::high_resolution_clock::now();
    float currt_frame_time = 0;
    float last_ts;
    while(!event_controller->isFinished()) {
        Event& e = event_controller->nextEvent();
        float time = e.ts / (float) 1e6;
        if(e.type == EventType::AVAILABLE) {
            animation_controller->driveAnimation(time - currt_frame_time);
            currt_frame_time = time;
            drawFrame();
        } else if (e.type == EventType::PLAY) {
            animation_controller->setPlaybackTimeRate(time, e.rate);
        } else if (e.type == EventType::SAVE) {
            screenshotSwapChain(IMG_STORAGE_PATH+e.filename);
        } else if (e.type == EventType::MARK) {
            std::cout<<"MARK "<<e.description_words<<"\n";
        }
        
        if(does_measure && last_ts != e.ts){
            auto new_time_measure = std::chrono::high_resolution_clock::now();
            float delta_time = std::chrono::duration<float, std::chrono::seconds::period>(new_time_measure - current_time_measure).count();
            std::cout<<"MEASURE frame "<<delta_time<<"\n";
            current_time_measure = new_time_measure;
            total_time += delta_time;
            last_ts=e.ts;
        }
        
    }
    if(does_measure)
        std::cout<<"Total time: "<<total_time<<"\n";
}

// Get the most-recently-rendered image in the swap chain
void ViewerApplication::screenshotSwapChain(const std::string& filename) {
    bool supportsBlit = true;

    // Check blit support for source and destination
    VkFormatProperties formatProps;

    // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
    vkGetPhysicalDeviceFormatProperties(physicalDevice, swapChainImageFormat, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        std::cerr << "Device does not support blitting from optimal tiled images, using copy instead of blit!" << std::endl;
        supportsBlit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        std::cerr << "Device does not support blitting to linear tiled images, using copy instead of blit!" << std::endl;
        supportsBlit = false;
    }

    // Source for the copy is the last rendered swapchain image
    VkImage srcImage = swapChainImages[imageIndex];

    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image
    VkImage dstImage;
    if(vkCreateImage(device, &imageCreateInfo, nullptr, &dstImage) != VK_SUCCESS){
        throw std::runtime_error("failed to create image for screenshot!");
    }
    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType =VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memRequirements.size;

    VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = vkHelper.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate memory for screenshot!");
    }
    vkBindImageMemory(device, dstImage, dstImageMemory, 0);

    // Do the actual blit from the swapchain image to our host visible destination image
    
    // Transition destination image to transfer destination layout
    vkHelper.transitionImageLayout(dstImage, 
                VK_IMAGE_LAYOUT_UNDEFINED, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0,
			    VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
			    VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Transition swapchain image from present to transfer source layout
    vkHelper.transitionImageLayout(srcImage, 
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_MEMORY_READ_BIT,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
			    VK_PIPELINE_STAGE_TRANSFER_BIT);

    // If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
    VkCommandBuffer commandBuffer = vkHelper.beginSingleTimeCommands();
    if (supportsBlit)
    {
        // Define the region to blit (we will blit the whole swapchain image)
        VkOffset3D blitSize;
        blitSize.x = width;
        blitSize.y = height;
        blitSize.z = 1;
        VkImageBlit imageBlitRegion{};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = blitSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = blitSize;

        // Issue the blit command
        vkCmdBlitImage(
            commandBuffer,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageBlitRegion,
            VK_FILTER_NEAREST);
    }
    else
    {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy imageCopyRegion{};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = width;
        imageCopyRegion.extent.height = height;
        imageCopyRegion.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(
            commandBuffer,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageCopyRegion);
    }
    vkHelper.endSingleTimeCommands(commandBuffer);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    vkHelper.transitionImageLayout(dstImage, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_MEMORY_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
			    VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Transition back the swap chain image after the blit is done
    vkHelper.transitionImageLayout(srcImage, 
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_MEMORY_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
			    VK_PIPELINE_STAGE_TRANSFER_BIT);
    

    // vulkanDevice->flushCommandBuffer(copyCmd, queue);

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

    // Map image memory so we can start copying from it
    const char* data;
    vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    data += subResourceLayout.offset;

    std::ofstream file(filename, std::ios::out | std::ios::binary);

    // ppm header
    file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";

    // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
    bool colorSwizzle = false;
    // Check if source is BGR
    // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
    if (!supportsBlit)
    {
        std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
        colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChainImageFormat) != formatsBGR.end());
    }

    // ppm binary pixel data
    for (uint32_t y = 0; y < height; y++)
    {
        unsigned int *row = (unsigned int*)data;
        for (uint32_t x = 0; x < width; x++)
        {
            if (colorSwizzle)
            {
                file.write((char*)row+2, 1);
                file.write((char*)row+1, 1);
                file.write((char*)row, 1);
            }
            else
            {
                file.write((char*)row, 3);
            }
            row++;
        }
        data += subResourceLayout.rowPitch;
    }
    file.close();

    std::cout << "Screenshot saved to disk" << std::endl;

    // Clean up resources
    vkUnmapMemory(device, dstImageMemory);
    vkFreeMemory(device, dstImageMemory, nullptr);
    vkDestroyImage(device, dstImage, nullptr);
}