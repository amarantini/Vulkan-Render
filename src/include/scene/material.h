#pragma once
#include <string>
#include <optional>
#include <variant>

#include "mathlib.h"


struct Texture {
    enum Type {TWO_D, CUBE};
    enum Format {LINEAR, RGBE};
    /*
        - "linear" -- map linearly. rgba' = rgba/255
        - rgbe" -- use shared-exponent RGBE as per radiance's HDR format. rgb = 2^(a-128) * (rgb+0.5) / 256
        All "type":"2D" textures have format "format":"linear".
        All "type":"cube" textures have format "format":"rgbe".
    */

    std::string src;
    Type type = TWO_D;
    Format format = LINEAR;
};

// PBR material
struct Pbr {
    std::optional<vec3> albedo;
    std::optional<Texture> albedo_texture;
    std::optional<float> roughness;
    std::optional<Texture> roughness_texture;
    std::optional<float> metalness;
    std::optional<Texture> metalness_texture;
};

// Lambertian material
struct Lambertian {
    std::optional<vec3> albedo;
    std::optional<Texture> albedo_texture;
};

// Mirror material: a perfect mirror BRDF
struct Mirror {
};

// Environment material: looks up the environment in the direction of the normal. (This is a BRDF with a Dirac delta along n)
struct Environment {
};

struct Simple {
};

// Simple material: uses a hemisphere light to shade a model based on its normals and vertex colors
struct Material {
    enum Type {PBR, LAMBERTIAN, MIRROR, ENVIRONMENT, SIMPLE};

    std::string name;
    std::optional<Texture> normal_map;
    std::optional<Texture> displacement_map;
    std::variant<Pbr, Lambertian,Mirror,Environment,Simple> material;
    Type type = Type::SIMPLE;

    Material() = default;

    Material(decltype(material) material_, Type type_) : material(std::move(material_)), type(type_) { }

    Pbr pbr(){
        return std::get<Pbr>(material);
    }

    Lambertian lambertian(){
        return std::get<Lambertian>(material);
    }
};



