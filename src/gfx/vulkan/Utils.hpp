#pragma once

#include "gfx/vulkan/VulkanUsage.hpp"
#include "core/Log.hpp"

namespace Engine {
inline void vkCheckResult(VkResult result) {
    if (result != VK_SUCCESS) {
        LOG_ERROR(
            "Fatal : VkResult is {}",
            vk::to_string(static_cast<vk::Result>(result))
        );
        assert(result == VK_SUCCESS);
    }
}

void imageLayoutTransition(
    vk::CommandBuffer cmdBuffer, vk::ImageAspectFlags imageAspectFlags,
    vk::PipelineStageFlagBits srcStageMask,
    vk::PipelineStageFlagBits dstStageMask, vk::AccessFlags srcAccessMask,
    vk::AccessFlags dstAccessMask, vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout, vk::Image image, uint32_t mipLevels = 1
);

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
    vk::DebugUtilsMessengerCallbackDataEXT const *pCallbackData,
    void * /*pUserData*/
);
}  // namespace Engine