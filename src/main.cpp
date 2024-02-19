//
//  main.cpp
//  VulkanTesting
//
//  Created by qiru hu on 1/16/24.
//
#include <iostream>
#include <cassert>
#include <cstdint>

#include "constants.h"
#include "arg_parser.h"
#include "include/utils/constants.h"
#include "viewer.h"
#include <memory>




int main(int argc, char ** argv) {
    // Parse arguments
    /* add_option(const std::string& opt_name, 
        bool required,
        int nargs = 1,
        const std::string& default_val = "",
        const std::vector<std::string> accepted_vals = std::vector<std::string>())
    */
    ArgParser arg_parser;

    // pecifies the scene (in .s72 format) to view
    arg_parser.add_option(SCENE, true);
    // view the scene through the camera named name. 
    //If such a camera doesn't exist in the scene, abort
    arg_parser.add_option(CAMERA, false);
    //use the physical device whose VkPhysicalDeviceProperties::deviceName matches name. 
    //If such a device does not exist, abort
    arg_parser.add_option(PHYSICAL_DEVICE, false);
    arg_parser.add_option(LIST_PHYSICAL_DEVICE, false, 0);
    //set the initial size of the drawable part of the window in physical pixels. 
    //If the resulting swapchain extent does not match the requested size 
    //(e.g., because the window manager won't allow a window this requested size), abort. 
    //If this flag is not specified, do something reasonable, like creating a moderately-sized window.
    arg_parser.add_option(DRAWING_SIZE, false, 2);
    //sets the culling mode. You may add additional culling modes when tackling extra goals
    arg_parser.add_option(CULLING, false, 1, CULLING_NONE, {CULLING_NONE, CULLING_FRUSTUM});
    arg_parser.add_option(HEADLESS, false, 1);
    arg_parser.add_option(ANIMATION_NO_LOOP, false, 0);
    arg_parser.add_option(MEASURE, false, 0);
    arg_parser.parse(argc, argv);

    std::string scene_file_path = (*arg_parser.get_option(SCENE))[0];
    std::cout<<scene_file_path<<"\n";
    std::string camera;
    std::string physical_device;
    int w, h;
    std::string culling_mode = "none";
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
        app.setCulling(culling_mode);
    }
    pt = arg_parser.get_option(HEADLESS);
    if(pt) {
        event_file_name = (*pt)[0];
        app.setHeadless(event_file_name);
    } 
    pt = arg_parser.get_option(ANIMATION_NO_LOOP);
    if(pt) {
        app.disableAnimationLoop();
    } 
    pt = arg_parser.get_option(MEASURE);
    if(pt) {
        app.enableMeasure();
    } 

    
    try {
        app.run();
    } catch (const std::exception& e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
