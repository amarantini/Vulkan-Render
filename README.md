# Vulkan-Render
This is a Vulkan render built using C++ with a JavaScript compiler. It supports loading scene, moving the camera and rendering meshes with various materials including PBR.

### Compile and run
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
- animation-no-loop -- not required -- disable animation loop. The animation loops in the default setting
- measure -- not required -- print the elapsed time between 2 consecutive frames and the total time at the end. Users can set the max number of frames they want to measure in constants.h for window mode

### Controls
- A Rotate camera left
- D Rotate camera right
- W Rotate camera upward
- S Rotate camera downward
- Q Move camera forward
- E Move camera backward
- C Switch camera in order (including cameras in scene and movable user and debug camera)
- B Switch to debug camera. The culling test is performed through the previously active camera.
- P Pause/resume animation
- R Restart animation
- LMB+Drag Resize the window by dragging the corner of the window
- LMB Close the window by clicking the close button ("x") on the window
- LMB Hide the window by clicking the hide button ("-") on the window

### Supported Scene Format
Please refer to https://github.com/15-472/s72 for more information.
There are some example scenes in [scenes](examples/scenes) folder.

### Implemented Features
- Load scene and mesh data
- Interactive camera and debug camera
- Frustum culling
- Animation
- Headless mode
- Environment lighting
- Simple, environment and mirror materials
- Tone mapping
- Lambertian material with prefiltered environment cubemap 
- PBR material with GGX prefiltered environment cubemap
- Normal map
- Displacement map
- Spot light, sphere light, sun light (use closest point estimation for PBR)
- Shadow map for spot light and sphere light
- Screen Space Ambient Occlusion (SSAO)
