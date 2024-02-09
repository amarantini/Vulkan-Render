#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class WindowController {
    friend class ViewerApplication;
private:
    GLFWwindow* window;
    bool framebufferResized = false;
    int width, height;

public:
    WindowController() = default;

    GLFWwindow* getWindow(){
        return window;
    }

    bool shouldClose();
    bool wasResized();
    void resetResized();

    void initWindow(float width, float height);
    void createSurface(VkInstance& instance, VkSurfaceKHR* surface);
    
    void destroy();

    void loop();

    void getFramebufferSize(int* width, int* height);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};