#include "gfx/vulkan/Swapchain.hpp"

namespace Engine {
Swapchain::~Swapchain() {
    for (auto imageView : imageViews) {
        device.destroyImageView(imageView);
    }
    device.destroySwapchainKHR(swapchain);
}

void Swapchain::create(vk::Device device, vk::SurfaceKHR surface,
                       uint32_t queueFamilyIndex) {
    this->device = device;
    this->surface = surface;

    createInfo = {
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = format,
        .imageColorSpace = colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueFamilyIndex,
        .preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = vk::True,
    };

    swapchain = device.createSwapchainKHR(createInfo);

    images = device.getSwapchainImagesKHR(swapchain);
    for (auto& image : images) {
        auto imageView = device.createImageView({
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .components =
                {
                    .r = vk::ComponentSwizzle::eIdentity,
                    .g = vk::ComponentSwizzle::eIdentity,
                    .b = vk::ComponentSwizzle::eIdentity,
                    .a = vk::ComponentSwizzle::eIdentity,
                },
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        });
        imageViews.push_back(imageView);
    }
}

void Swapchain::recreate(vk::Extent2D extent) {
    device.waitIdle();
    device.destroySwapchainKHR(swapchain);
    for (auto imageView : imageViews) {
        device.destroyImageView(imageView);
    }
    imageViews.clear();

    this->extent = extent;
    create(device, surface, queueFamilyIndex);
}

void Swapchain::recreate() {
    device.waitIdle();
    device.destroySwapchainKHR(swapchain);
    for (auto imageView : imageViews) {
        device.destroyImageView(imageView);
    }
    imageViews.clear();

    create(device, surface, queueFamilyIndex);
}

uint32_t Swapchain::acquireNextImage(vk::Semaphore semaphore) {
    uint32_t imageIndex;
    auto result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                        semaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return UINT32_MAX;
    }

    return imageIndex;
}
}  // namespace Engine