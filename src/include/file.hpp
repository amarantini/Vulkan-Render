//
//  file.hpp
//  VulkanTesting
//
//  Created by qiru hu on 1/19/24.
//

#ifndef file_hpp
#define file_hpp

#include <stdio.h>
#include <fstream>

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    
    file.close();

    return buffer;
}

#endif /* file_hpp */
