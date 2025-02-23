#pragma once

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include "gfx/vulkan/VulkanUsage.hpp"

namespace Engine {
class Device;

class UiLayout {
   public:
    UiLayout(Device* device);
    ~UiLayout();

    void init();
    void prepareFrame();
    void draw();
    void record(vk::CommandBuffer& cmdBuffer);

    uint32_t minImageCount = 2;
    uint32_t imageCount = 3;
    vk::Format colorFormat = vk::Format::eB8G8R8A8Srgb;
    vk::Format depthStencilFormat = vk::Format::eD32SfloatS8Uint;

    double renderTime = 0.0;
    float fontScale = 24.0f;

    void addOffscreenTextureForImGui(VkSampler sampler, VkImageView imageView,
                                     VkImageLayout layout);
    void removeOffscreenTextureForImGui();

    inline bool isOffscreenRenderable() const {
        return offscreenInfo.width != 0 && offscreenInfo.height != 0 &&
               offscreenInfo.doRender;
    }
    inline vk::Extent2D getOffscreenExtent() const {
        return {offscreenInfo.width, offscreenInfo.height};
    }

    inline bool getOffscreenSizeChanged() const {
        return offscreenInfo.sizeChanged;
    }

    inline void setOffscreenSizeChanged(bool value) {
        offscreenInfo.sizeChanged = value;
    }

   private:
    bool isShowScene = true;
    bool isShowMetrics = false;
    bool isShowDemo = false;

    void showMenuBar();
    void showScene();
    void showMetrics();

    void initImGui();

    struct {
        uint32_t width = 1200;
        uint32_t height = 720;
        bool sizeChanged = false;
        bool doRender = true;

        VkDescriptorSet descriptorSet;
    } offscreenInfo;

    Device* device;

    std::string fontName = "jetbrains_mono_bold.ttf";

    vk::DescriptorPool descriptorPool;
};
}  // namespace Engine