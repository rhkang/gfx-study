#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;

    static std::array<vk::VertexInputAttributeDescription, 4>
    getAttributeDescriptions() {
        return {
            vk::VertexInputAttributeDescription(
                0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(
                1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat,
                                                offsetof(Vertex, uv)),
            vk::VertexInputAttributeDescription(
                3, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, color)),
        };
    }

    static vk::VertexInputBindingDescription getBindingDescription() {
        return vk::VertexInputBindingDescription(0, sizeof(Vertex),
                                                 vk::VertexInputRate::eVertex);
    }
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

struct Model {
    uint32_t id;

    Mesh mesh;

    void loadFromFile(std::string fileName);
    void createPlane();
    void createCube();

    inline uint32_t getTotalVerticesSize() const {
        return sizeof(Vertex) * mesh.vertices.size();
    }
    inline uint32_t getTotalIndicesSize() const {
        return sizeof(uint16_t) * mesh.indices.size();
    }
};