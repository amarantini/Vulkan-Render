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
// const std::string ANIMATION_LOOP = "--animation-loop";
const std::string ANIMATION_NO_LOOP = "--animation-no-loop";
const std::string MEASURE = "--measure";

// culling mode
const std::string CULLING_NONE = "None";
const std::string CULLING_FRUSTUM = "Frustum";

// animation chanels
const std::string CHANEL_TRANSLATION = "translation"; //3d
const std::string CHANEL_ROTATION = "rotation"; //4d
const std::string CHANEL_SCALE = "scale"; //3d
// animation interpolation 
const std::string INTERP_LINEAR = "LINEAR";
const std::string INTERP_SLERP = "SLERP";
const std::string INTERP_STEP = "STEP";

const std::string SCENE_PATH = "../scene/";
const std::string IMG_STORAGE_PATH = "../images/";


const int FRAME_RATE = 30;
const float FRAME_TIME = (1.0f / 30.0f); // in seconds

// Performance testing
// static bool ENABLE_INDEX_BUFFER = true;
const int MAX_FRAME_COUNT = 800;

// Shader paths
const std::string SHADER_PATH = "./src/shaders/bin/";

const std::string SIMPLE_VSHADER = SHADER_PATH+"simple.vert.spv";
const std::string SIMPLE_FSHADER = SHADER_PATH+"simple.frag.spv";

const std::string ENV_VSHADER = SHADER_PATH+"env.vert.spv";
const std::string ENV_FSHADER = SHADER_PATH+"env.frag.spv";

const std::string MIRROR_VSHADER = SHADER_PATH+"mirror.vert.spv";
const std::string MIRROR_FSHADER = SHADER_PATH+"mirror.frag.spv";

const std::string LAMBER_VSHADER = SHADER_PATH+"lamber.vert.spv";
const std::string LAMBER_FSHADER = SHADER_PATH+"lamber.frag.spv";

const std::string PBR_VSHADER = SHADER_PATH+"pbr.vert.spv";
const std::string PBR_FSHADER = SHADER_PATH+"pbr.frag.spv";

const std::string SHADOW_VSHADER = SHADER_PATH+"depth.vert.spv";
const std::string SHADOW_FSHADER = SHADER_PATH+"depth.frag.spv";

const std::string DEBUG_SHADOW_VSHADER = SHADER_PATH+"shadow.debug.vert.spv";
const std::string DEBUG_SHADOW_FSHADER = SHADER_PATH+"shadow.debug.frag.spv";

const std::string SHADOW_CUBE_VSHADER = SHADER_PATH+"depth.cube.vert.spv";
const std::string SHADOW_CUBE_FSHADER = SHADER_PATH+"depth.cube.frag.spv";

const std::string DEBUG_SHADOW_CUBE_FSHADER = SHADER_PATH+"shadow.debug.cube.frag.spv";

const int MAX_DESCRIPTOR_COUNT = 6; //maximum number of texture sampler descriptor

// Cube arguments
const std::string LAMBERTIAN = "--lambertian";
const std::string GGX = "--ggx";
const std::string LUT = "--lut";

const int ENVIRONMENT_MIP_LEVEL = 5;

// Light
const int MAX_LIGHT_COUNT = 10;

// Shader map constants
const float SHADOW_ZNEAR = 0.1f;
const float SHADOW_ZFAR = 20.0f;
const float SHADOW_FOV = 45.0f;
const int SHADOW_RES = 1024;

// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
const float DEPTH_BIAS_CONSTANT = 1.25f;
// Slope depth bias factor, applied depending on polygon's slope
const float DEPTH_BIAS_SLOPE = 1.75f;
// Debug shadow map by displying shadow map
const bool DISPLAY_SHADOW_MAP_SPHERE = false;
const bool DISPLAY_SHADOW_MAP_SPOT = false;
const int DISPLAY_SHADOW_MAP_IDX = 1;