# Vulkan-Render

To compile the Scene Viewer, run "node Maekfile.js" from the root directory. The executable is compiled into /bin folder.
To run the Scene Viewer, run the command ./bin/viewer --scene [folder]/scene.s72 ... from the command line. The scene file should be placed in /scene/[folder] directory outside the code's root directory. So the total relative path is "../scene/[folder]/scene.s72".

### Command-line Arguments
- scene [folder]/scene.s72 -- required -- load scene from scene.s72 under "/scene/[folder]" directory
- camera camera-name -- not required -- view the scene through the camera named name. If such a camera doesn't exist in the scene, abort
- physical-device device-name -- not required -- use the physical device whose VkPhysicalDeviceProperties::deviceName matches name. If such a device does not exist, abort
- list-physical-devices -- not required -- list all available physics devices
- window-size w h -- not required -- set the initial size of the drawable part of the window in physical pixels. If not specified, use default size 800 450
- culling cull-mode -- not required -- set the culling mode. Available choices are "None" and "Frustum". Default to None
- headless event_file_name -- not required -- enable headless mode. Execute event in events file specified by event_file_name
- animation-no-loop -- not required -- disable animation loop. The animation loops in default setting
- measure -- not required -- print the elapse time between 2 consequtive frames and the total time at the end. User can set the max number of frames they want to measure in constants.h for window mode
