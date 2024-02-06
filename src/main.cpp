//
//  main.cpp
//  VulkanTesting
//
//  Created by qiru hu on 1/16/24.
//
#include <iostream>
#include <cassert>
#include <cstdint>

#include "arg_parser.h"
#include "viewer.h"


// Define arguments
const std::string SCENE = "--scene";
const std::string CAMERA = "--camera";
const std::string PHYSICAL_DEVICE = "--physical-device";
const std::string LIST_PHYSICAL_DEVICE = "--list-physical-devices";
const std::string DRAWING_SIZE = "--drawing-size";
const std::string CULLING = "--culling";
const std::string HEADLESS = "--headless";


int main(int argc, char ** argv) {
    // Parse arguments
    ArgParser arg_parser;

    // pecifies the scene (in .s72 format) to view
    arg_parser.add_option(SCENE, true);
    // view the scene through the camera named name. 
    //If such a camera doesn't exist in the scene, abort
    arg_parser.add_option(CAMERA, false);
    //use the physical device whose VkPhysicalDeviceProperties::deviceName matches name. 
    //If such a device does not exist, abort
    arg_parser.add_option(PHYSICAL_DEVICE, false);
    arg_parser.add_option(LIST_PHYSICAL_DEVICE, false , "", 0);
    //set the initial size of the drawable part of the window in physical pixels. 
    //If the resulting swapchain extent does not match the requested size 
    //(e.g., because the window manager won't allow a window this requested size), abort. 
    //If this flag is not specified, do something reasonable, like creating a moderately-sized window.
    arg_parser.add_option(DRAWING_SIZE, false, "", 2);
    //sets the culling mode. You may add additional culling modes when tackling extra goals
    arg_parser.add_option(CULLING, false, "none", 1, {"none", "frustrum"});
    arg_parser.add_option(HEADLESS, false);

    arg_parser.parse(argc, argv);

    std::string scene_file_path = SCENE_PATH + (*arg_parser.get_option(SCENE))[0];
    std::string camera;
    std::string physical_device;
    int w, h;
    [[maybe_unused]] std::string culling_mode = "none";
    [[maybe_unused]] bool headless = false;
    [[maybe_unused]] std::string event_file_name;

    ViewerApplication app;

    const std::vector<std::string>* pt= nullptr;
    // if required to list physical device, print physical device names and exit
    pt = arg_parser.get_option(LIST_PHYSICAL_DEVICE);
    if(pt) {
        app.listPhysicalDevice();
        return 0;
    }

    app.setUpScene(scene_file_path);

    pt = arg_parser.get_option(CAMERA);
    if(pt) {
        camera = (*pt)[0];
        app.setCamera(camera);
    }   
    pt = arg_parser.get_option(PHYSICAL_DEVICE);
    if(pt) {
        physical_device = (*pt)[0];
        app.setPhysicalDevice(physical_device);
    }
    pt = arg_parser.get_option(DRAWING_SIZE);
    if(pt){
        w = stoi((*pt)[0]);
        h = stoi((*pt)[1]);
        app.setDrawingSize(w,h);
    }
    pt = arg_parser.get_option(CULLING);
    if(pt) {
        culling_mode = (*pt)[0];
    }
    pt = arg_parser.get_option(HEADLESS);
    if(pt) {
        headless = true;
        event_file_name = (*pt)[0];
        app.setHeadless(headless);
    }
    
    try {
        app.run();
    } catch (const std::exception& e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;



    // glm::mat4 glm_view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)); //eye position, center position and up axis
    // std::cout<<"View: ";
    // for(int i=0 ; i<4; i++){
    //     for(int j=0; j<4; j++) {
    //         std::cout<<glm_view[i][j]<<",";
    //     }
    //     std::cout<<"\n";
    // }
    // mat4 view = lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f));
    // std::cout<<"My view: "<<view<<"\n";

    // glm::mat4 glm_proj = glm::perspective(0.47109f, 800 / (float)450, 0.1f, 1000.0f); //45 degree vertical field-of-view, aspect ratio, near and far view planes
    // glm_proj[1][1] *= -1;

    // mat4 proj = perspective(0.47109f, 800 / (float) 450, 0.1f, 1000.0f);
    // proj[1][1] *= -1;
    // std::cout<<"Proj: ";
    // for(int i=0 ; i<4; i++){
    //     for(int j=0; j<4; j++) {
    //         std::cout<<glm_proj[i][j]<<",";
    //     }
    //     std::cout<<"\n";
    // }
    // std::cout<<"My proj: "<<proj<<"\n";

    // glm::mat4 glm_model = glm::rotate(glm::mat4(1.0f), 1.0f * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)); //existing transformation, rotation angle and rotation axis
    // mat4 model = mat4(-0.0f,1,0,0,
    //                 -1,-0.0f,0,0,
    //                 0,0,1,0,
    //                 0,0,0,1);
    
    // std::cout<<"Model: ";
    // for(int i=0 ; i<4; i++){
    //     for(int j=0; j<4; j++) {
    //         std::cout<<glm_model[i][j]<<",";
    //     }
    //     std::cout<<"\n";
    // }
    // std::cout<<"My model: "<<model<<"\n";
    // glm::vec4 result = glm_proj * glm_view * glm_model * glm::vec4(0.5f, 0.5f, 0.0f, 1.0);
    // std::cout<<"Result: ";
    // for(int j=0; j<4; j++) {
    //     std::cout<<result[j]<<",";
    // }
    // std::cout<<"\n";

    // vec4 myResult = proj * view * model * vec4(0.5f, 0.5f, 0.0f, 1.0);
    // std::cout<<"My result: "<< myResult <<"\n";

    // glm::mat4 a = glm_proj * glm_view;
    // std::cout<<"proj * view: ";
    // for(int i=0 ; i<4; i++){
    //     for(int j=0; j<4; j++) {
    //         std::cout<<a[i][j]<<",";
    //     }
    //     std::cout<<"\n";
    // }
    // mat4 my_a = proj * view;
    // std::cout<<"My proj * view: "<< my_a <<"\n";

    // glm::mat4 b = glm_proj * glm_view * glm_model;
    // std::cout<<"proj * view * model: ";
    // for(int i=0 ; i<4; i++){
    //     for(int j=0; j<4; j++) {
    //         std::cout<<b[i][j]<<",";
    //     }
    //     std::cout<<"\n";
    // }
    // mat4 my_b = proj * view *  model;
    // std::cout<<"My proj * view * model: "<< my_b <<"\n";
    
    // std::cout<<"Size: \n";
    // std::cout<<sizeof(glm::mat4)<<"\n";
    // std::cout<<sizeof(mat4)<<"\n";
    // std::cout<<sizeof(std::array<float,4>)<<"\n";
    // std::cout<<sizeof(vec4)<<"\n";
}
