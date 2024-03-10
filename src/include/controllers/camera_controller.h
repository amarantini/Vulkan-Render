#pragma once

#include <cfloat>
#include <stdexcept>
#include <algorithm>
#include <GLFW/glfw3.h>
#include "mathlib.h"
#include "camera.h"

// Key mappings
const int ROTATE_LEFT = GLFW_KEY_A;
const int ROTATE_RIGHT = GLFW_KEY_D;
const int ROTATE_UP = GLFW_KEY_W;
const int ROTATE_DOWN = GLFW_KEY_S;
const int MOVE_FORWARD = GLFW_KEY_Q;
const int MOVE_BACKWARD = GLFW_KEY_E;


class CameraController  {
public: 
    CameraController(std::unordered_map<std::string, std::shared_ptr<Camera> > cameras_, int width_, int height_)  {
        width = width_;
        height = height_;

        cameras = cameras_;
        vec3 translation = vec3();
        qua rotation = qua();
        if(!cameras_.empty()) {
            translation = cameras.begin()->second->transform->translation;
            rotation = cameras.begin()->second->transform->rotation;
        }   
        
        std::shared_ptr<Transform> userCamTransform = std::make_shared<Transform>("User-Camera-Transform", translation, rotation, vec3(1));
        user_camera = std::make_shared<Camera>(width / (float) height, degToRad(45.0f), 0.1, 1000.0f);
        user_camera->movable = true;
        user_camera->transform = userCamTransform;
        user_camera->euler = user_camera->transform->rotation.toEuler();
        cameras["User-Camera"] = user_camera;

        std::shared_ptr<Transform> debugCamTransform = std::make_shared<Transform>("Debug-Camera-Transform", translation, rotation, vec3(1));
        debug_camera = std::make_shared<Camera>(width / (float) height, degToRad(45.0f), 0.1, 1000.0f);
        debug_camera->debug = true;
        debug_camera->movable = true;
        debug_camera->transform = debugCamTransform;
        debug_camera->euler = debug_camera->transform->rotation.toEuler();
        cameras["Debug-Camera"] = debug_camera;

        camera_itr = cameras.find("User-Camera");
        curr_camera = user_camera;
        prev_camera = curr_camera;
    }

    virtual ~CameraController() = default;

    void setWindow(GLFWwindow* window_){
        window = window_;
    }

    void setCamera(const std::string& camera_name){
        camera_itr = cameras.find(camera_name);
        if(camera_itr==cameras.end()){
            throw std::runtime_error("Specified camera not found");
        }
        curr_camera = camera_itr->second;
    }

    void setHeightWdith(float height_, float width_) {
        height = height_;
        width = width_;
    }

    mat4 getPerspective() {
        return curr_camera->getPerspective(width / (float) height);
    }

    mat4 getView(){
        return curr_camera->getView();
    }

    bool isDebug(){
        return curr_camera->debug;
    }

    mat4 getPrevPerspective() {
        mat4 persp = prev_camera->getPerspective(width / (float) height);
        persp[1][1] *= -1;
        return persp;
    }

    mat4 getPrevView(){
        return prev_camera->getView();
    }

    vec4 getEyePos(){
        return curr_camera->getEyePos();
    }

    // Move camera up, down, left, right, forward, backward, and rotat camera left and right
    void moveCamera(float deltaTime){
        if(curr_camera->movable){
            float deltaRoll = .0f;
            float deltaPitch = .0f;
            if (glfwGetKey(window, ROTATE_LEFT) == GLFW_PRESS) deltaRoll += 1.f;
            if (glfwGetKey(window, ROTATE_RIGHT) == GLFW_PRESS) deltaRoll -= 1.f;
            if (glfwGetKey(window, ROTATE_DOWN) == GLFW_PRESS) deltaPitch -= 1.f;
            if (glfwGetKey(window, ROTATE_UP) == GLFW_PRESS) deltaPitch += 1.f;
            float& pitch = curr_camera->euler[0];
            float& roll = curr_camera->euler[2];
            
            if(std::fabs(deltaRoll) > FLT_EPSILON || std::fabs(deltaPitch) > FLT_EPSILON) {
                // Use pitch, roll
                // std::cout<<"Before roll: "<<roll<<"\n";
                pitch += deltaTime * rotation_speed * deltaPitch;
                roll += deltaTime * rotation_speed * deltaRoll;
                // std::cout<<"After roll: "<<roll<<"\n\n";
                // roll = std::clamp(roll, -1.5f, 1.5f);
                // std::cout<<"Clamp roll: "<<roll<<"\n";
                curr_camera->transform->rotation = eulerToQua(curr_camera->euler);

                // Use Quaternion
                // deltaYaw = deltaTime * rotation_speed * deltaYaw;
                // deltaPitch = deltaTime * rotation_speed * deltaPitch;
                //  // deltaYaw = myClamp(deltaYaw, -1.5-yaw, 1.5-yaw);
                // // deltaPitch = myClamp(deltaPitch, -1.5-pitch, 1.5-pitch);
                // // vec4 qPitch = angleAxis(pitch, vec3(1, 0, 0));
                // // vec4 qYaw = angleAxis(yaw, vec3(0, 1, 0)); 
                // vec4 qPitch = angleAxis(deltaPitch, vec3(1, 0, 0));
                // vec4 qYaw = angleAxis(deltaYaw, vec3(0, 1, 0)); 
                // vec4& rotation = curr_camera->transform->rotation;
                // // rotation = quaMul(qPitch, qYaw);
                // rotation = quaMul(quaMul(qPitch.normalized(),rotation),qYaw.normalized());
            }
            
            // float yaw = curr_camera->yaw();
            // // float pitch = curr_camera->pitch();
            // float cos_yaw = cos(yaw);
            // float sin_yaw = sin(yaw);
            // float cos_pitch = cos(pitch);
            // float sin_pitch = sin(pitch);
            // vec3 forward(cos_pitch * sin_yaw, -sin_pitch, cos_pitch * cos_yaw);
            vec4 forward4 = rotationMat(curr_camera->transform->rotation) * vec4(0,0,-1,0);
            forward4.normalize();
            vec3 forward(forward4[0],forward4[1],forward4[2]);

            vec3 translation(0);
            if (glfwGetKey(window, MOVE_FORWARD) == GLFW_PRESS) translation += forward;
            if (glfwGetKey(window, MOVE_BACKWARD) == GLFW_PRESS) translation -= forward;

            if(translation.norm()>FLT_EPSILON) {
                curr_camera->transform->translation += move_speed * deltaTime * translation.normalized();
            }
        }
    }
    

    void turnOnDebugCamera(){
        // switch to debug camera, but render in previously active camera
        std::cout<<"Turning on Debug_Camera\n";
        prev_camera = curr_camera;
        camera_itr = cameras.find("Debug-Camera");
        curr_camera = debug_camera;
    }

    void switchCamera(){
        camera_itr++;
        if(camera_itr==cameras.end()){
            camera_itr = cameras.begin();
        }
        curr_camera = camera_itr->second;
        std::cout<<"Switch to camera: "<<camera_itr->first<<"\n";
    }

    bool isMovable(){
        return curr_camera->movable;
    }
    
private:
    float move_speed = 10.0f;
    float rotation_speed = 1.0f;
    // double xpos, ypos;
    float height, width;
    GLFWwindow* window;
    std::shared_ptr<Camera> user_camera;
    std::shared_ptr<Camera> prev_camera;
    std::shared_ptr<Camera> curr_camera;
    std::shared_ptr<Camera> debug_camera;
    std::unordered_map<std::string, std::shared_ptr<Camera> > cameras;
    std::unordered_map<std::string, std::shared_ptr<Camera> >::iterator camera_itr;
    
};