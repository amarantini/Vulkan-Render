#pragma once

#include "camera_controller.h"
#include "animation_controller.h"
#include <GLFW/glfw3.h>
#include <memory>

// Referenced from https://stackoverflow.com/questions/21799746/how-to-glfwsetkeycallback-for-different-classes
// To solve the problem that glfwSetKeyCallback does not accept non-static member function
class StateBase
{
public:
    virtual void keyCallback(
        GLFWwindow *window,
        int key,
        int scancode,
        int action,
        int mods) = 0; /* purely abstract function */

    virtual void mouseButtonCallback(
        GLFWwindow* window, 
        int button, 
        int action, 
        int mods) = 0; /* purely abstract function */

    static StateBase *event_handling_instance;
    // technically setEventHandling should be finalized so that it doesn't
    // get overwritten by a descendant class.
    virtual void setEventHandling();

    static void keyCallback_dispatch(
        GLFWwindow *window,
        int key,
        int scancode,
        int action,
        int mods)
    {
        if(event_handling_instance)
            event_handling_instance->keyCallback(window,key,scancode,action,mods);
    }  

    static void mouseButtonCallback_dispatch(
        GLFWwindow* window, 
        int button, 
        int action, 
        int mods)
    {
        if(event_handling_instance)
            event_handling_instance->mouseButtonCallback(window,button,action,mods);
    }    
};



class InputController : StateBase {
public:
    InputController() = default;
    virtual ~InputController() = default;

    void setCameraController(std::shared_ptr<CameraController> cameraController_){
        cameraController = cameraController_;
    }

    void setAnimationController(std::shared_ptr<AnimationController> animationController_){
        animationController = animationController_;
    }

    virtual void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        // if(curr_camera->movable) {
        //     if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) 
        //     {
        //         //getting cursor position
        //         glfwGetCursorPos(window, &xpos, &ypos);
        //     } 
        //     if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        //         double new_xpos,new_ypos;
        //         glfwGetCursorPos(window, &new_xpos, &new_ypos);
        //         rotateCamera(new_xpos-xpos, new_ypos-ypos);
        //     }
        // }
        
    }

    void setKeyCallback(GLFWwindow* window_){
        window = window_;
        setEventHandling();
        glfwSetKeyCallback(window, StateBase::keyCallback_dispatch);
        // glfwSetMouseButtonCallback(window, StateBase::mouseButtonCallback_dispatch);
    }

    virtual void keyCallback(
        GLFWwindow *window,
        int key,
        int scancode,
        int action,
        int mods) {
        if (key == GLFW_KEY_C && action == GLFW_PRESS) {
            cameraController->switchCamera();
        }
        if (key == GLFW_KEY_B && action == GLFW_PRESS) {
            cameraController->turnOnDebugCamera();
        } 
        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            // pause animation and resume
            animationController->pauseOrResume();
        } 
        if (key == GLFW_KEY_R && action == GLFW_PRESS) {
            // restart animation
            animationController->restart();
        } 
    }
private:
    std::shared_ptr<CameraController> cameraController;
    std::shared_ptr<AnimationController> animationController;
    GLFWwindow *window;
};