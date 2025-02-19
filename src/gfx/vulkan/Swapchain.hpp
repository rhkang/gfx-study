#pragma once

#include "gfx/vulkan/VulkanUsage.hpp"

namespace Engine {
class Swapchain {
   public:
    Swapchain() = default;
    ~Swapchain();

    void create(
        vk::Device device, vk::SurfaceKHR surface, uint32_t queueFamilyIndex
    );
    void recreate(vk::Extent2D extent);
    void recreate();
    vk::Image getImage(uint32_t index) { return images[index]; }
    vk::ImageView getImageView(uint32_t index) { return imageViews[index]; }
    vk::SwapchainKHR getSwapchain() const { return swapchain; }
    vk::Extent2D getExtent() const { return extent; }

    uint32_t acquireNextImage(vk::Semaphore semaphore);

    uint32_t imageCount;
    vk::Format format;
    vk::ColorSpaceKHR colorSpace;
    vk::PresentModeKHR presentMode;
    vk::Extent2D extent;

   private:
    vk::Device device;
    uint32_t queueFamilyIndex;
    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;

    vk::SwapchainCreateInfoKHR createInfo;

    std::vector<vk::Image> images;
    std::vector<vk::ImageView> imageViews;
};
}  // namespace Engine