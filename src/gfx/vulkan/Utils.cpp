#include "gfx/vulkan/Utils.hpp"

namespace Engine {
void imageLayoutTransition(vk::CommandBuffer cmdBuffer,
                           vk::ImageAspectFlags imageAspectFlags,
                           vk::PipelineStageFlagBits srcStageMask,
                           vk::PipelineStageFlagBits dstStageMask,
                           vk::AccessFlags srcAccessMask,
                           vk::AccessFlags dstAccessMask,
                           vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                           vk::Image image, uint32_t mipLevels) {
    vk::ImageMemoryBarrier imageBarrier{
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            vk::ImageSubresourceRange{imageAspectFlags, 0, mipLevels, 0, 1}};

    cmdBuffer.pipelineBarrier(srcStageMask, dstStageMask, {}, nullptr, nullptr,
                              imageBarrier);
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debugMessageFunc(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                 vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
                 vk::DebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                 void* /*pUserData*/) {
    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        LOG_ERROR(pCallbackData->pMessage);
    } else if (messageSeverity &
               vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        LOG_WARN(pCallbackData->pMessage);
    } else {
        LOG(pCallbackData->pMessage);
    }

    return VK_FALSE;
}
}  // namespace Engine