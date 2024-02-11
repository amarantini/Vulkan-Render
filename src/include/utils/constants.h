#pragma once

#include <string>

// Define arguments
const std::string SCENE = "--scene";
const std::string CAMERA = "--camera";
const std::string PHYSICAL_DEVICE = "--physical-device";
const std::string LIST_PHYSICAL_DEVICE = "--list-physical-devices";
const std::string DRAWING_SIZE = "--window-size";
const std::string CULLING = "--culling";
const std::string HEADLESS = "--headless";
const std::string ANIMATION_LOOP = "--animation-loop";

// culling mode
const std::string CULLING_NONE = "None";
const std::string CULLING_FRUSTUM = "Frustum";

// animation chanels
const std::string CHANEL_TRANSLATION = "translation"; //3d
const std::string CHANEL_ROTATION = "rotation"; //4d
const std::string CHANEL_SCALE = "scale"; //3d
// animation interpolation 
const std::string INTERP_LINEAR = "LINEAR";

const std::string SCENE_PATH = "./scene/";
const std::string IMG_STORAGE_PATH = "./images/";


const int FRAME_RATE = 30;
const float FRAME_TIME = (1.0f / 30.0f); // in seconds