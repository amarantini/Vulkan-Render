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
    arg_parser.add_option(CAMERA, false, "default_camera");
    //use the physical device whose VkPhysicalDeviceProperties::deviceName matches name. 
    //If such a device does not exist, abort
    arg_parser.add_option(PHYSICAL_DEVICE, false, "default_physical_device");
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

    [[maybe_unused]] std::string scene_name = (*arg_parser.get_option(SCENE))[0];
    [[maybe_unused]] std::string camera = "";
    [[maybe_unused]] std::string physical_device = "";
    [[maybe_unused]] int w, h;
    [[maybe_unused]] std::string culling_mode = "none";
    [[maybe_unused]] bool headless = false;
    [[maybe_unused]] std::string event_file_name;

    ViewerApplication app;

    const std::vector<std::string>* pt= nullptr;
    pt = arg_parser.get_option(LIST_PHYSICAL_DEVICE);
    if(pt) {
        app.listPhysicalDevice();
        return 0;
    }
    pt = arg_parser.get_option(CAMERA);
    if(pt) {
        scene_name = (*pt)[0];
    }   
    pt = arg_parser.get_option(PHYSICAL_DEVICE);
    if(pt) {
        physical_device = (*pt)[0];
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
}
