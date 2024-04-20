#include "viewer.h"
#include "math/math_util.h"
#include "scene/material.h"
#include "vk/vk_debug.h"
#include "file.hpp"

#include <cfloat>
#include <memory>
#include <stdexcept>
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
    environment_lighting_info = scene.getEnvironment();
    light_info_list = scene.getLightInfos();

    uboLight.sphereLightCount = light_info_list.sphere_lights.size();
    uboLight.spotLightCount = light_info_list.spot_lights.size();
    uboLight.directionalLightCount = light_info_list.directional_lights.size();
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
            std::cout<<"load "<<info->mesh->name<<"\n";
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
    createShadowMapPasses();
    createShadowMapPassesSphere();

    loadEnvironment();
    createModels();

    createUniformBuffers();
    createDescriptorPool();

    createSSAOPassList();
    
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
        uniformBuffers[i].destroy();
        lightUniformBuffers[i].destroy();
    }
    
    shadowMapPassList.destroy();
    gBufferPass.destroy();
    ssaoPassList.destroy();

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutScene, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutMaterial, nullptr);
    
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
    appInfo.pApplicationName = "Vulkan Viewer";
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
                    physicalDeviceProperties = deviceProperty;
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
                vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);
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
        vkHelper.createImageView(swapChainImageViews[i], swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
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

    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();
    
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
    pushConstantRange.size = sizeof(PushConstantModel);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {descriptorSetLayoutScene, descriptorSetLayoutMaterial};
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()); // Optional
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data(); // Optional
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
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    VkPipelineVertexInputStateCreateInfo emptyInputState {}; // Empty vertex input state
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    pipelineInfo.pVertexInputState = &emptyInputState;
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

    // SSAO pipeline
    pipelineInfo.renderPass = ssaoPassList.renderPass;
    shaderStages[0] = loadShader(SSAO_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(SSAO_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create SSAO pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.ssao), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // SSAO pipeline
    shaderStages[0] = loadShader(SSAO_BLUR_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(SSAO_BLUR_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create SSAO Blur pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.ssaoBlur), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // Gbuffer pipeline
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.renderPass = gBufferPass.renderPass;
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.colorWriteMask = 0xf;
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    std::array<VkPipelineColorBlendAttachmentState, 5> blendAttachmentStates = {
        colorBlendAttachmentState,
        colorBlendAttachmentState,
        colorBlendAttachmentState,
        colorBlendAttachmentState,
        colorBlendAttachmentState
    };
    colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
    colorBlending.pAttachments = blendAttachmentStates.data();
    shaderStages[0] = loadShader(GBUFFER_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(GBUFFER_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create GBuffer pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.gbuffer), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    pipelineInfo.renderPass = renderPass;

    // Debug shadow pipeline
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    pipelineInfo.pVertexInputState = &emptyInputState;
    shaderStages[0] = loadShader(DEBUG_SHADOW_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(DEBUG_SHADOW_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create Debug shadow pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.debug), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // Debug shadow cube pipeline
    shaderStages[0] = loadShader(DEBUG_SHADOW_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(DEBUG_SHADOW_CUBE_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::cout<<"Create Debug shadow cube pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.debugCube), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // Shadow cube pipeline
    shaderStages[0] = loadShader(SHADOW_CUBE_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader(SHADOW_CUBE_FSHADER, VK_SHADER_STAGE_FRAGMENT_BIT);
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.renderPass = shadowMapPassList.renderPassSphere;
    std::cout<<"Create Shadow cube pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.shadowCube), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

    // Shadow pipeline
    shaderStages[0] = loadShader(SHADOW_VSHADER, VK_SHADER_STAGE_VERTEX_BIT); 
    pipelineInfo.stageCount = 1;
    // No blend attachment states (no color attachments used)
    colorBlending.attachmentCount = 0;
    // Disable culling, so all faces contribute to shadows
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;//VK_CULL_MODE_NONE;//
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    // Enable depth bias
    rasterizer.depthBiasEnable = VK_TRUE;
    // Add depth bias to dynamic state, so we can change it at runtime
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    dynamicStateInfo.pDynamicStates = dynamicStates.data();
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    pipelineInfo.renderPass = shadowMapPassList.renderPassSpot;
    std::cout<<"Create Shadow pipeline\n";
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.shadow), "Failed to create pipeline!");
    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    
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
    
    depthFormat = findDepthFormat();
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

    shadowMapPassList.createSpotRenderPass(depthFormat);
    shadowMapPassList.createSphereRenderPass(depthFormat);

    gBufferPass.createRenderPass(depthFormat);
    ssaoPassList.createRenderPass();
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

VkViewport ViewerApplication::createViewPort(float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

VkRect2D ViewerApplication::createScissor(int32_t width, int32_t height, int32_t offsetX, int32_t offsetY) {
    VkRect2D rect2D {};
    rect2D.extent.width = width;
    rect2D.extent.height = height;
    rect2D.offset.x = offsetX;
    rect2D.offset.y = offsetY;
    return rect2D;
}

void ViewerApplication::recordCommandBuffer(VkCommandBuffer commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin recording command buffer!");

    std::array<VkClearValue, 2> clearValues{};
    VkViewport viewport{};
    VkRect2D scissor{};

    /* Deferred shading to generate GBuffer
    */
    {
        std::array<VkClearValue,6> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[4].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[5].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = gBufferPass.renderPass;
        renderPassInfo.framebuffer = gBufferPass.frameBuffer;
        renderPassInfo.renderArea.extent.width = width;
        renderPassInfo.renderArea.extent.height = height;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        viewport = createViewPort(width, height, 0.0f, 1.0f);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        scissor = createScissor(width, height, 0, 0);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.gbuffer);
        // Bind scene descriptor set
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetsScene[currentFrame], 0, nullptr);
        if(culling == CULLING_NONE) {
            model_list.renderForGBuffer(commandBuffer, pipelineLayout);
        } else if (culling == CULLING_FRUSTUM){
            // frustum culling
            mat4 VP;
            if(camera_controller->isDebug()) {
                VP = camera_controller->getPrevPerspective() * camera_controller->getPrevView();
            } else {
                VP = uboScene.proj * uboScene.view;
            }
            
            model_list.renderForGBuffer(commandBuffer, pipelineLayout, culling, VP);
        }

        vkCmdEndRenderPass(commandBuffer);
    }

    /* SSAO
    */
    {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = ssaoPassList.renderPass;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.clearValueCount = 2;
        
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.pClearValues = clearValues.data();
        renderPassInfo.framebuffer = ssaoPassList.ssaoPass.frameBuffer;
        renderPassInfo.renderArea.extent.width = width;
        renderPassInfo.renderArea.extent.height = height;

        viewport = createViewPort(width, height, 0.0f, 1.0f);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        scissor = createScissor(width, height, 0, 0);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.ssao);
        // Bind scene descriptor set
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &(ssaoPassList.ssaoPass.descriptorSets[currentFrame]), 0, nullptr);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
    }

    /* SSAO Blur
    */
    {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = ssaoPassList.renderPass;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.clearValueCount = 2;
        
        clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.pClearValues = clearValues.data();
        renderPassInfo.framebuffer = ssaoPassList.ssaoBlurPass.frameBuffer;
        renderPassInfo.renderArea.extent.width = width;
        renderPassInfo.renderArea.extent.height = height;

        viewport = createViewPort(width, height, 0.0f, 1.0f);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        scissor = createScissor(width, height, 0, 0);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.ssaoBlur);
        // Bind scene descriptor set
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &(ssaoPassList.ssaoBlurPass.descriptorSets[0]), 0, nullptr);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
    }
    
    /*
        Reference https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmapping/shadowmapping.cpp
        Generate shadow map for spot light by rendering the scene from light's POV
    */
    if (shadowMapPassList.shadowMapPassesSpot.size() > 0) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = shadowMapPassList.renderPassSpot;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.clearValueCount = 1;
        
        clearValues[0].depthStencil = {1.0f, 0};
        renderPassInfo.pClearValues = clearValues.data();

        // Set depth bias (aka "Polygon offset")
        // Required to avoid shadow mapping artifacts
        vkCmdSetDepthBias(
            commandBuffer,
            DEPTH_BIAS_CONSTANT,
            0.0f,
            DEPTH_BIAS_SLOPE);
        for(auto& shadowMapPass: shadowMapPassList.shadowMapPassesSpot) {
            renderPassInfo.framebuffer = shadowMapPass.frameBuffer;
            
            renderPassInfo.renderArea.extent.width = shadowMapPass.shadow_res;
            renderPassInfo.renderArea.extent.height = shadowMapPass.shadow_res;

            viewport = createViewPort((float)shadowMapPass.shadow_res, (float)shadowMapPass.shadow_res, 0.0f, 1.0f);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            scissor = createScissor(shadowMapPass.shadow_res, shadowMapPass.shadow_res, 0, 0);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
                
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadow);

            model_list.renderForShadowMap(commandBuffer, pipelineLayout, shadowMapPass.pcShadow);

            vkCmdEndRenderPass(commandBuffer);
        }
    }

    /* Generate shadow cubemap for sphere lights
    */
    if(shadowMapPassList.shadowMapPassesSphere.size()>0) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = shadowMapPassList.renderPassSphere;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.clearValueCount = 2;
        
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.pClearValues = clearValues.data();
        
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadowCube);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &shadowMapPassList.sphereDescriptorSet, 0, nullptr);

        for(auto& shadowMapPass: shadowMapPassList.shadowMapPassesSphere) {
            renderPassInfo.renderArea.extent.width = shadowMapPass.shadow_res;
            renderPassInfo.renderArea.extent.height = shadowMapPass.shadow_res;

            viewport = createViewPort((float)shadowMapPass.shadow_res, (float)shadowMapPass.shadow_res, 0.0f, 1.0f);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            scissor = createScissor(shadowMapPass.shadow_res, shadowMapPass.shadow_res, 0, 0);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            for(uint32_t i=0; i<6; i++){
                renderPassInfo.framebuffer = shadowMapPass.frameBuffers[i];
                vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
                
                model_list.renderForShadowMap(commandBuffer, pipelineLayout, shadowMapPass.pcCubeShadow[i]);

                vkCmdEndRenderPass(commandBuffer);
            }
            
        }
    }


    /*
        Scene rendering with applied shadow map and SSAO
    */
    {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        

        if(DISPLAY_SHADOW_MAP_SPOT) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.debug);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &shadowMapPassList.debugDescriptorSet, 0, nullptr);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        } else if(DISPLAY_SHADOW_MAP_SPHERE) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.debugCube);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &shadowMapPassList.debugCubeDescriptorSet, 0, nullptr);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        } else {
            // Bind scene descriptor set
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetsScene[currentFrame], 0, nullptr);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        }
        

        vkCmdEndRenderPass(commandBuffer);
    }
    

    VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer), "failed to record command buffer for render pass!")
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
        resizeGBufferAttachment();
        resizeSSAOPassListAttachment();
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
            resizeGBufferAttachment();
            resizeSSAOPassListAttachment();
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
    // int width = 0, height = 0;
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
    //create uniform buffer scene
    VkDeviceSize bufferSize = sizeof(UniformBufferObjectScene);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkHelper.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i].buffer, uniformBuffers[i].bufferMemory);

        vkMapMemory(device, uniformBuffers[i].bufferMemory, 0, bufferSize, 0, &uniformBuffers[i].bufferMapped);
    }

    // Create uniform buffer light
    lightUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkDeviceSize lightBufferSize = sizeof(UniformBufferObjectLight);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkHelper.createBuffer(lightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightUniformBuffers[i].buffer, lightUniformBuffers[i].bufferMemory);

        vkMapMemory(device, lightUniformBuffers[i].bufferMemory, 0, lightBufferSize, 0, &lightUniformBuffers[i].bufferMapped);
    }

    // Create uniform buffer shadow
    shadowMapPassList.createShadowUniformBuffer();
    shadowMapPassList.createSphereUniformBuffer();

    // Create uniform buffer SSAO
    ssaoPassList.createUniformBuffer();
}

void ViewerApplication::updateUniformBuffer(uint32_t currentImage) {
    // Update scene
    uboScene.proj = camera_controller->getPerspective();
    uboScene.proj[1][1] *= -1;
    uboScene.view = camera_controller->getView();
    uboScene.light = environment_lighting_info.transform->worldToLocal(); // transform from world space to environment space
    uboScene.eye = camera_controller->getEyePos();

    memcpy(uniformBuffers[currentImage].bufferMapped, &uboScene, sizeof(uboScene));

    // Update light
    light_info_list.update();
    
    for(size_t i=0; i<uboLight.sphereLightCount; i++){
        uboLight.sphereLights[i] = light_info_list.sphere_lights[i];
    }
    for(size_t i=0; i<uboLight.spotLightCount; i++){
        uboLight.spotLights[i] = light_info_list.spot_lights[i];
    }
    for(size_t i=0; i<uboLight.directionalLightCount; i++){
        uboLight.directionalLights[i] = light_info_list.directional_lights[i];
    }

    // update shadow
    for(auto& shadowMapPass: shadowMapPassList.shadowMapPassesSpot)  {
        shadowMapPass.updatePushConstant();
        uboLight.spotLights[shadowMapPass.light_idx].lightVP = shadowMapPass.pcShadow.lightVP;
    }

    memcpy(lightUniformBuffers[currentImage].bufferMapped, &uboLight, sizeof(uboLight));

    // Update UniformBufferObjectSphereLight for sphere light shadow map rendering
    if(shadowMapPassList.shadowMapPassesSphere.size()>0) {
        for(auto& shadowMapPass: shadowMapPassList.shadowMapPassesSphere)  {
            shadowMapPass.updateSphereShadowData();
        }
        for(size_t i=0; i<uboLight.sphereLightCount; i++){
            shadowMapPassList.uboSphere.sphereLights[i] = light_info_list.sphere_lights[i];
        } 
        shadowMapPassList.copySphereUniformBuffer();
    }
    
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
    // Descriptor layout for scene

    std::vector<VkDescriptorSetLayoutBinding> sceneBindings = {
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,  /*binding=*/0, 1),//uboSceneLayoutBinding
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,  /*binding=*/1, 1),//uboLightLayoutBinding
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/2, 1),//shadow map sampler
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/3, MAX_LIGHT_COUNT),//spot shadow maps
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/4, 1),//sphere map sampler
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/5, MAX_LIGHT_COUNT),//sphere shadow maps
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/6, 1), //debug shadow maps OR Position map
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/7, 1), // Normal map
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/8, 1), // Albedo map
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/9, 1), // Roughness map
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/10, 1), // Metalness map
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/11, 1), // environment map
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/12, 1), // pbr prefiltered map
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/13, 1), // brdf lut
        createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/14, 1), //ssao blur
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(sceneBindings.size());
    layoutInfo.pBindings = sceneBindings.data();

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayoutScene), "failed to create descriptor set layout!");

    std::vector<VkDescriptorSetLayoutBinding> materialBindings = {
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/0, 1), //normal map
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/1, 1),//displacement map
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/2, 1), //albedo map
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/3, 1), // metalness map
    createDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, /*binding=*/4, 1), //roughness map
    };

    layoutInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
    layoutInfo.pBindings = materialBindings.data();

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayoutMaterial), "failed to create descriptor set layout!");
}

void ViewerApplication::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 1000;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 1000; //?
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 100; //?
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[3].descriptorCount = 100;

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
    descriptorWrite.descriptorType = type;
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
    descriptorWrite.descriptorType = type;
    descriptorWrite.descriptorCount = descriptorCount;
    descriptorWrite.pImageInfo = imageInfo;
    return descriptorWrite;
}

void ViewerApplication::allocateDescriptorSet(std::vector<VkDescriptorSet>& descriptorSets, int descriptorSetCount, VkDescriptorSetLayout descriptorSetLayout){
    std::vector<VkDescriptorSetLayout> layouts(descriptorSetCount, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetCount);
    allocInfo.pSetLayouts = layouts.data();
    
    descriptorSets.resize(descriptorSetCount);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
}

void ViewerApplication::allocateSingleDescriptorSet(VkDescriptorSet& descriptorSet, VkDescriptorSetLayout descriptorSetLayout){
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
}

void ViewerApplication::VkTexture::updateDescriptorImageInfo(){
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo.imageView = textureImageView;
    descriptorImageInfo.sampler = textureSampler;
}

void ViewerApplication::createModelDescriptorSets(std::shared_ptr<VkModel> model) {
    allocateDescriptorSet(model->descriptorSets, MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutMaterial);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // Add normal map and displacement map is common for all material
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/0, &(model->material.normalMap->descriptorImageInfo), 1),
            writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/1, &(model->material.displacementMap->descriptorImageInfo), 1)
        };
        
        if(model->material.type == Material::Type::ENVIRONMENT || 
        model->material.type == Material::Type::MIRROR ||
        model->material.type == Material::Type::SIMPLE) {
            // No other texture to add
            
        } else if(model->material.type == Material::Type::LAMBERTIAN) {
            // Add albedo
            
            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/2, &(model->material.albedo->descriptorImageInfo), 1)
            );
            // Fill in roughness and metalness descriptor but will not be used
            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/3, &(model->material.metalness->descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/4, &(model->material.roughness->descriptorImageInfo), 1)
            );
            
        } else if(model->material.type == Material::Type::PBR) {
            // Add albedo, metalness, roughness

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/2, &(model->material.albedo->descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/3, &(model->material.metalness->descriptorImageInfo), 1)
            );

            writeDescriptorSets.push_back(writeDescriptorSet(model->descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/4, &(model->material.roughness->descriptorImageInfo), 1)
            );  
        }

        
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }
}

void ViewerApplication::createDescriptorSets() {
    // create scene descriptor set
    allocateDescriptorSet(descriptorSetsScene, MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutScene);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo sceneBufferInfo{};
        sceneBufferInfo.buffer = uniformBuffers[i].buffer;
        sceneBufferInfo.offset = 0;
        sceneBufferInfo.range = sizeof(UniformBufferObjectScene);

        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = lightUniformBuffers[i].buffer;
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = sizeof(UniformBufferObjectLight);

        VkDescriptorImageInfo depthSamplerInfo = {};
        depthSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthSamplerInfo.sampler = shadowMapPassList.defaultShadowMapPassSpot.shadowMapTexture.textureSampler;

        VkDescriptorImageInfo samplerInfo = {};
        samplerInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        samplerInfo.sampler = shadowMapPassList.defaultShadowMapPassSphere.shadowMapTexture.textureSampler;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/0, &sceneBufferInfo, 1), //ubo scene
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/1, &lightBufferInfo, 1), //ubo light
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_SAMPLER, /*binding=*/2, &depthSamplerInfo, 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, /*binding=*/3, shadowMapPassList.descriptorImageInfosSpot.data(), MAX_LIGHT_COUNT),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_SAMPLER, /*binding=*/4, &samplerInfo, 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, /*binding=*/5, shadowMapPassList.descriptorImageInfosSphere.data(), MAX_LIGHT_COUNT),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &(gBufferPass.positionAttachment.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/7, &(gBufferPass.normalAttachment.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/8, &(gBufferPass.albedoAttachment.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/9, &(gBufferPass.roughnessAttachment.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/10, &(gBufferPass.metalnessAttachment.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/11, &(lambertianEnvironmentMap.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/12, &(pbrEnvironmentMap.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/13, &(lut.descriptorImageInfo), 1),
            writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/14, &(ssaoPassList.ssaoBlurPass.colorAttachment.descriptorImageInfo), 1) // SSAO Blur
        };

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }

    for(auto model : model_list.getAllModels()) {
        createModelDescriptorSets(model);
    }
    createShadowMapSphereDescriptorSet();
    if(DISPLAY_SHADOW_MAP_SPOT) {
        createShadowMapDebugDescriptorSet();
    }
    if(DISPLAY_SHADOW_MAP_SPHERE) {
        createShadowMapDebugCubeDescriptorSet();
    }
    
    createSSAOPassDescriptorSet();
    createSSAOBlurPassDescriptorSet();
}












/* ------------ Texture  ------------ */

ViewerApplication::TextureInfo ViewerApplication::VkTexture::loadFromFile(const char* texture_file_path, int desired_channels = STBI_rgb_alpha) {
    TextureInfo info = {};
    // load texture in top-left mode
    info.pixels = stbi_load(texture_file_path, &info.texWidth, &info.texHeight, &info.texChannels, desired_channels);
    if (!info.pixels) {
        throw std::runtime_error("failed to load texture image at: "+std::string(texture_file_path));
    }

    return info;
}

void ViewerApplication::VkTexture::createTextureImage(TextureInfo info, VkFormat format, int pixelSize) {
    VkDeviceSize imageSize = info.texWidth * info.texHeight * info.texChannels * pixelSize;
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vkHelper.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    
    void* data;
    VK_CHECK_RESULT(vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data), "failed to map memory");
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
    createTextureSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, 1, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
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
        if(isRgbe){
            createTextureSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, 1, VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
        } else {
            createTextureSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER);
        }
        
    } else if (type == "lambertian"){
        // load prefiltered environment map for lambertian diffuse
        std::string common_file_path = texture_file_path.substr(0, texture_file_path.find_last_of("."));
        std::vector<TextureInfo> infos = {loadFromFile((common_file_path+".lambertian.png").c_str(), STBI_rgb_alpha)};
        std::cout<<"Load prefiltered environment map for lambertian diffuse "<<(common_file_path+".lambertian.png")<<"\n"; 
        createCubeTextureImage(infos, format);
        createCubeTextureImageView(format);
        if(isRgbe){
            createTextureSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, 1, VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
        } else {
            createTextureSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER);
        }
    } else if (type == "pbr") {
        // load prefiltered environment maps for PBR
        std::vector<TextureInfo> infos;
        std::string common_file_path = texture_file_path.substr(0, texture_file_path.find_last_of("."));
        for(int i=0; i<ENVIRONMENT_MIP_LEVEL; ++i) {
            std::string mip_file_path = common_file_path+".ggx."+std::to_string(i)+".png";
            infos.push_back(loadFromFile(mip_file_path.c_str(), STBI_rgb_alpha)); 
            std::cout<<"Load mipmap "<<mip_file_path<<": "<<infos.back().texWidth<<","<<infos.back().texHeight<<"\n";
        }
        
        createCubeTextureImage(infos, format);
        createCubeTextureImageView(format, ENVIRONMENT_MIP_LEVEL);
        if(isRgbe){
            createTextureSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, ENVIRONMENT_MIP_LEVEL, VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, 4.0);
        } else {
            createTextureSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_COMPARE_OP_NEVER, ENVIRONMENT_MIP_LEVEL, VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, 4.0);
        }
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
    if(environment_lighting_info.exist) {
        environmentMap.load(environment_lighting_info.texture.src, VK_FORMAT_R8G8B8A8_UNORM, "", true);
        if(!model_info_list.lamber_models.empty() || !model_info_list.pbr_models.empty()) {
            lambertianEnvironmentMap.load(environment_lighting_info.texture.src, VK_FORMAT_R8G8B8A8_UNORM, "lambertian", true);
        }
        // if(!model_info_list.pbr_models.empty()) {
            std::string src = environment_lighting_info.texture.src;
            pbrEnvironmentMap.load(src, VK_FORMAT_R8G8B8A8_UNORM, "pbr", true);
            
            std::string lut_file_path = src.substr(0, src.find_last_of("."))+".lut.png";
            // lut.load(lut_file_path, VK_FORMAT_R16G16_SFLOAT);
            lut.load(lut_file_path, VK_FORMAT_R8G8B8A8_UNORM);
        // }
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
    vkHelper.createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    vkHelper.createImageView(depthImageView, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
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
    VkImage dstImage;
    VkDeviceMemory dstImageMemory;
    // vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
    // memAllocInfo.allocationSize = memRequirements.size;
    // // Memory must be host visible to copy from
    // memAllocInfo.memoryTypeIndex = vkHelper.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    // if(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory) != VK_SUCCESS) {
    //     throw std::runtime_error("failed to allocate memory for screenshot!");
    // }
    // vkBindImageMemory(device, dstImage, dstImageMemory, 0);
    vkHelper.createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, dstImage, dstImageMemory);

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

/* ------------------------- Shadow map ------------------------- */

void ViewerApplication::createShadowMapSphereDescriptorSet() {
    // for sphere shadow map rendering pipeline
    allocateSingleDescriptorSet(shadowMapPassList.sphereDescriptorSet, descriptorSetLayoutScene);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = shadowMapPassList.sphereUniformBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObjectShadow);
    
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(shadowMapPassList.sphereDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/0, &bufferInfo, 1)};

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

}

void ViewerApplication::createShadowMapDebugDescriptorSet() {
    // for shadow map debug pipeline
    allocateSingleDescriptorSet(shadowMapPassList.debugDescriptorSet, descriptorSetLayoutScene);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = shadowMapPassList.shadowUniformBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObjectShadow);

    VkDescriptorImageInfo samplerInfo = {};
    samplerInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    
    if(shadowMapPassList.shadowMapPassesSpot.empty()) {
        samplerInfo.sampler = shadowMapPassList.defaultShadowMapPassSpot.shadowMapTexture.textureSampler;
        samplerInfo.imageView = shadowMapPassList.defaultShadowMapPassSpot.shadowMapTexture.textureImageView;
    } else {
        auto& shadowMapPass = shadowMapPassList.shadowMapPassesSpot[DISPLAY_SHADOW_MAP_IDX];
        samplerInfo.sampler = shadowMapPass.shadowMapTexture.textureSampler;
        samplerInfo.imageView = shadowMapPass.shadowMapTexture.textureImageView;
        shadowMapPassList.copyShadowUniformBuffer(shadowMapPass.uboShadow);
    }
    
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(shadowMapPassList.debugDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/0, &bufferInfo, 1),
        writeDescriptorSet(shadowMapPassList.debugDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &samplerInfo, 1)};

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void ViewerApplication::createShadowMapDebugCubeDescriptorSet() {
    // for cube shadow map debug pipeline
    allocateSingleDescriptorSet(shadowMapPassList.debugCubeDescriptorSet, descriptorSetLayoutScene);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = shadowMapPassList.shadowUniformBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObjectShadow);

    VkDescriptorImageInfo samplerInfo = {};
    samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    if(shadowMapPassList.shadowMapPassesSphere.empty()) {
        samplerInfo.sampler = shadowMapPassList.defaultShadowMapPassSphere.shadowMapTexture.textureSampler;
        samplerInfo.imageView = shadowMapPassList.defaultShadowMapPassSphere.shadowMapTexture.textureImageView;
    } else {
        auto& shadowMapPass = shadowMapPassList.shadowMapPassesSphere[DISPLAY_SHADOW_MAP_IDX];
        samplerInfo.sampler = shadowMapPass.shadowMapTexture.textureSampler;
        samplerInfo.imageView = shadowMapPass.shadowMapTexture.textureImageView;
        shadowMapPassList.copyShadowUniformBuffer(shadowMapPass.uboShadow);
    }
    
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(shadowMapPassList.debugCubeDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/0, &bufferInfo, 1),
        writeDescriptorSet(shadowMapPassList.debugCubeDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &samplerInfo, 1)};

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

}

void ViewerApplication::createShadowMapPasses() {
    int count = 0;
    for(uint32_t i=0; i<light_info_list.spot_lights.size(); i++) {
        int shadow_res = light_info_list.spot_light_infos[i]->shadow_res;
        if(shadow_res>0) {
            ShadowMapPass shadowMapPass = {};
            shadowMapPass.light_idx = i;
            light_info_list.spot_lights[i].shadow[1] = count++;
            float fov = light_info_list.spot_lights[i].others[2] * 2;
            float radius = light_info_list.spot_lights[i].others[0];
            float limit = light_info_list.spot_lights[i].others[1];
            shadowMapPass.init2D(fov, shadow_res, radius, limit, light_info_list.spot_light_infos[i]->transform, shadowMapPassList.renderPassSpot, depthFormat); 
            shadowMapPassList.shadowMapPassesSpot.push_back(shadowMapPass);
            shadowMapPassList.descriptorImageInfosSpot.push_back(shadowMapPass.shadowMapTexture.descriptorImageInfo);
        }
    }
    shadowMapPassList.defaultShadowMapPassSpot.initDefault(degToRad(45), 1, nullptr, shadowMapPassList.renderPassSpot, depthFormat);
    if(count == 0) {
        // If no light needs a shadow map, fill the descriptorImageInfosSpot with default info
        shadowMapPassList.descriptorImageInfosSpot.push_back(shadowMapPassList.defaultShadowMapPassSpot.shadowMapTexture.descriptorImageInfo);
    } 
    if(count < MAX_LIGHT_COUNT) {
        std::cout<<"Shadow map required for spot lights: "<<count<<"\n";
        for(;count<MAX_LIGHT_COUNT; count++) {
            shadowMapPassList.descriptorImageInfosSpot.push_back(shadowMapPassList.defaultShadowMapPassSpot.shadowMapTexture.descriptorImageInfo);
    
        }
        
    }
}

void ViewerApplication::createShadowMapPassesSphere() {
    int count = 0;
    for(uint32_t i=0; i<light_info_list.sphere_lights.size(); i++) {
        int shadow_res = light_info_list.sphere_light_infos[i]->shadow_res;
        if(shadow_res>0) {
            ShadowMapPass shadowMapPass = {};
            shadowMapPass.light_idx = i;
            light_info_list.sphere_lights[i].shadow[1] = count++;
            float radius = light_info_list.sphere_lights[i].others[0];
            float limit = light_info_list.sphere_lights[i].others[1];
            shadowMapPass.initCube(&(shadowMapPassList.uboSphere), shadow_res, radius, limit, light_info_list.sphere_light_infos[i]->transform, shadowMapPassList.renderPassSphere, depthFormat); 
            shadowMapPassList.shadowMapPassesSphere.push_back(shadowMapPass);
            shadowMapPassList.descriptorImageInfosSphere.push_back(shadowMapPass.shadowMapTexture.descriptorImageInfo);
        }
    }
    shadowMapPassList.defaultShadowMapPassSphere.initCube(nullptr, 1, 1, 10, nullptr, shadowMapPassList.renderPassSphere, depthFormat);
    if(count == 0) {
        // If no light needs a shadow map, fill the descriptorImageInfosSpot with default info
        shadowMapPassList.descriptorImageInfosSphere.push_back(shadowMapPassList.defaultShadowMapPassSphere.shadowMapTexture.descriptorImageInfo);
    } 
    if(count < MAX_LIGHT_COUNT) {
        std::cout<<"Shadow map required for sphere lights: "<<count<<"\n";
        for(;count<MAX_LIGHT_COUNT; count++) {
            shadowMapPassList.descriptorImageInfosSphere.push_back(shadowMapPassList.defaultShadowMapPassSphere.shadowMapTexture.descriptorImageInfo);
    
        }
        
    }
}


/* ------------------- Deferred shading ------------------- */
// Reference: https://github.com/SaschaWillems/Vulkan/blob/master/examples/deferred/deferred.cpp

void ViewerApplication::GBufferPass::createAttachments(){
    // (World space) Positions
    createAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    positionAttachment, width, height);

    // (World space) Normals
    createAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    normalAttachment, width, height);

    // Albedo (color)
    createAttachment(
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    albedoAttachment, width, height);

    // roughness
    createAttachment(
    VK_FORMAT_R8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    roughnessAttachment, width, height);

    // metalness
    createAttachment(
    VK_FORMAT_R8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    metalnessAttachment, width, height);

    // Depth attachment
    createAttachment(
    depthFormat,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    depthAttachment, width, height);
}

void ViewerApplication::GBufferPass::createRenderPass(VkFormat depthFormat_){
    depthFormat = depthFormat_;

    createAttachments();

    std::array<VkAttachmentDescription, 6> attachmentDescs;
    // Set initial attachment properties
    for (uint32_t i = 0; i < 6; ++i)
    {
        attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachmentDescs[i].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachmentDescs[i].flags = 0;
    }
    attachmentDescs[5].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescs[5].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescs[5].format = depthFormat;

    attachmentDescs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachmentDescs[3].format = VK_FORMAT_R8_UNORM;
    attachmentDescs[4].format = VK_FORMAT_R8_UNORM;
    

    std::vector<VkAttachmentReference> colorReferences;
    colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 5;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pColorAttachments = colorReferences.data();
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
    subpass.pDepthStencilAttachment = &depthReference;

    // Use subpass dependencies for attachment layout transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachmentDescs.data();
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass), "failed to create render pass for GBufferPass");

    createFrameBuffer();
}

void ViewerApplication::GBufferPass::createFrameBuffer(){
    std::array<VkImageView,6> attachments;
    attachments[0] = positionAttachment.textureImageView;
    attachments[1] = normalAttachment.textureImageView;
    attachments[2] = albedoAttachment.textureImageView;
    attachments[3] = roughnessAttachment.textureImageView;
    attachments[4] = metalnessAttachment.textureImageView;
    attachments[5] = depthAttachment.textureImageView;

    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext = NULL;
    fbufCreateInfo.renderPass = renderPass;
    fbufCreateInfo.pAttachments = attachments.data();
    fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbufCreateInfo.width = width;
    fbufCreateInfo.height = height;
    fbufCreateInfo.layers = 1;
    VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffer), "failed to create frame buffer for GBufferPass");
}

void ViewerApplication::resizeGBufferAttachment() {
    gBufferPass.recreateAttachments();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &(gBufferPass.positionAttachment.descriptorImageInfo), 1),
        writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/7, &(gBufferPass.normalAttachment.descriptorImageInfo), 1),
        writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/8, &(gBufferPass.albedoAttachment.descriptorImageInfo), 1),
        writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/9, &(gBufferPass.roughnessAttachment.descriptorImageInfo), 1),
        writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/10, &(gBufferPass.metalnessAttachment.descriptorImageInfo), 1)
        };

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }
}

void ViewerApplication::GBufferPass::recreateAttachments() {
    // On window size change
    vkDeviceWaitIdle(device);

    vkDestroyFramebuffer(device, frameBuffer, nullptr);
    positionAttachment.destroy();
    normalAttachment.destroy();
    albedoAttachment.destroy();
    depthAttachment.destroy();
    metalnessAttachment.destroy();
    roughnessAttachment.destroy();

    createAttachments();

    createFrameBuffer();
}

/* -------------------- SSAO --------------------- */
void ViewerApplication::SSAOBasePass::init(VkRenderPass renderPass){
    createAttachment(
    VK_FORMAT_R8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    colorAttachment, width, height);

    createFrameBuffer(renderPass);
}

void ViewerApplication::SSAOPassList::createRenderPass(){
    VkAttachmentDescription attachmentDescription{};
    attachmentDescription.format = VK_FORMAT_R8_UNORM;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pColorAttachments = &colorReference;
    subpass.colorAttachmentCount = 1;

    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = &attachmentDescription;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass), "failed to create render pass for SSAOPass");

    ssaoPass.init(renderPass);//create attachment and frame buffer
    ssaoBlurPass.init(renderPass);
}

void ViewerApplication::SSAOBasePass::createFrameBuffer(VkRenderPass renderPass){
    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext = NULL;
    fbufCreateInfo.renderPass = renderPass;
    fbufCreateInfo.pAttachments = &(colorAttachment.textureImageView);
    fbufCreateInfo.attachmentCount = 1;
    fbufCreateInfo.width = width;
    fbufCreateInfo.height = height;
    fbufCreateInfo.layers = 1;
    VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffer), "failed to create frame buffer for SSAOPass");
}

void ViewerApplication::SSAOBasePass::recreateAttachment(VkRenderPass renderPass) {
    // On window size change
    vkDeviceWaitIdle(device);

    vkDestroyFramebuffer(device, frameBuffer, nullptr);
    colorAttachment.destroy();

    createAttachment(
    VK_FORMAT_R8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    colorAttachment, width, height);

    createFrameBuffer(renderPass);
}

void ViewerApplication::createSSAOPassList(){
    ssaoPassList.init();
}

void ViewerApplication::resizeSSAOPassListAttachment() {
    ssaoPassList.ssaoPass.recreateAttachment(ssaoPassList.renderPass);
    ssaoPassList.ssaoBlurPass.recreateAttachment(ssaoPassList.renderPass);

    for(uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(ssaoPassList.ssaoPass.descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &(gBufferPass.positionAttachment.descriptorImageInfo), 1),
        writeDescriptorSet(ssaoPassList.ssaoPass.descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/7, &(gBufferPass.normalAttachment.descriptorImageInfo), 1)
        };
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(ssaoPassList.ssaoBlurPass.descriptorSets[0], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &(ssaoPassList.ssaoPass.colorAttachment.descriptorImageInfo), 1)
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(descriptorSetsScene[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/14, &(ssaoPassList.ssaoBlurPass.colorAttachment.descriptorImageInfo), 1)
        };

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }
}

void ViewerApplication::createSSAOPassDescriptorSet(){
    // for cube shadow map debug pipeline
    allocateDescriptorSet(ssaoPassList.ssaoPass.descriptorSets, MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutScene);

    for(uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObjectScene);

        VkDescriptorBufferInfo ssaoBufferInfo{};
        ssaoBufferInfo.buffer = ssaoPassList.ssaoUniformBuffer.buffer;
        ssaoBufferInfo.offset = 0;
        ssaoBufferInfo.range = sizeof(UniformBufferObjectSSAO);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(ssaoPassList.ssaoPass.descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/0, &bufferInfo, 1),
        writeDescriptorSet(ssaoPassList.ssaoPass.descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, /*binding=*/1, &ssaoBufferInfo, 1),
        writeDescriptorSet(ssaoPassList.ssaoPass.descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &(gBufferPass.positionAttachment.descriptorImageInfo), 1),
        writeDescriptorSet(ssaoPassList.ssaoPass.descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/7, &(gBufferPass.normalAttachment.descriptorImageInfo), 1),
        writeDescriptorSet(ssaoPassList.ssaoPass.descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/8, &(ssaoPassList.ssaoNoise.descriptorImageInfo), 1)};

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }
}

void ViewerApplication::createSSAOBlurPassDescriptorSet(){
    allocateDescriptorSet(ssaoPassList.ssaoBlurPass.descriptorSets, 1, descriptorSetLayoutScene);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        writeDescriptorSet(ssaoPassList.ssaoBlurPass.descriptorSets[0], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, /*binding=*/6, &(ssaoPassList.ssaoPass.colorAttachment.descriptorImageInfo), 1)
    };

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}