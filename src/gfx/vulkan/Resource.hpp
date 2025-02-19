#pragma once

#include "gfx/vulkan/VulkanUsage.hpp"

namespace Engine {
class Device;

struct Buffer {
    Device* device;

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    void allocate(
        vk::DeviceSize size, vk::BufferUsageFlags usage,
        bool isPersistentMap = false
    );
    void* map();
    void unmap();
    void destroy();
};

struct Texture {
    Device* device;

    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    vk::ImageView imageView;
    vk::Sampler sampler;

    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
    vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat;

    uint32_t mipLevels = 1;
    float minLod = 0.0f;

    void loadFromFile(const char* filename);
    void allocate(
        vk::Extent2D extent, uint32_t mipLevels, vk::Format format,
        vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags
    );
    void createSampler();
    void destroy();
};
}  // namespace Engine