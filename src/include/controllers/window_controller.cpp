#include "window_controller.h"
#include <memory>
#include <chrono>

bool WindowController::shouldClose() { 
    return glfwWindowShouldClose(window); 
}

bool WindowController::wasResized() { 
    return framebuffer_resized; 
}

void WindowController::resetResized() { 
    framebuffer_resized = false; 
}

void WindowController::initWindow(float width, float height){
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width,height,"Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void WindowController::createSurface(VkInstance& instance, VkSurfaceKHR* surface){
    if(glfwCreateWindowSurface(instance, window, nullptr, surface)!=VK_SUCCESS){
        throw std::runtime_error("failed to create window surface!");
    }
}

void WindowController::destroy() {
    glfwTerminate();
    glfwDestroyWindow(window);
}

void WindowController::getFramebufferSize(int* width, int* height){
    glfwGetFramebufferSize(window, width, height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, width, height);
        glfwWaitEvents();
    }
}

void WindowController::framebufferResizeCallback(GLFWwindow* window, int width, int height)  {
    auto window_controller = reinterpret_cast<WindowController *>(glfwGetWindowUserPointer(window));
    window_controller->framebuffer_resized = true;
    window_controller->width = width;
    window_controller->height = height;
}
