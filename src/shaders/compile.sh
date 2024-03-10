#!/bin/sh

#  compile.sh
#  VulkanTesting
#
#  Created by qiru hu on 1/19/24.
#  
/Users/qiruhu/VulkanSDK/1.3.268.1/macOS/bin/glslc simple.shader.vert -o simple.vert.spv
/Users/qiruhu/VulkanSDK/1.3.268.1/macOS/bin/glslc simple.shader.frag -o simple.frag.spv

/Users/qiruhu/VulkanSDK/1.3.268.1/macOS/bin/glslc pbr.shader.vert -o pbr.vert.spv
/Users/qiruhu/VulkanSDK/1.3.268.1/macOS/bin/glslc pbr.shader.frag -o pbr.frag.spv

/Users/qiruhu/VulkanSDK/1.3.268.1/macOS/bin/glslc skybox.shader.vert -o skybox.vert.spv
/Users/qiruhu/VulkanSDK/1.3.268.1/macOS/bin/glslc skybox.shader.frag -o skybox.frag.spv
