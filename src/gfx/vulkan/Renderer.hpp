#pragma once

#include "gfx/vulkan/Device.hpp"
#include "gfx/vulkan/UiLayout.hpp"
#include "gfx/vulkan/Swapchain.hpp"

namespace Engine {
class Renderer {
   public:
    void init(Device *device);
    void prepare();
    void update();
    void destroy();
    void render();

    void handleWindowResize();
    bool windowResized = false;

   protected:
    inline vk::CommandBuffer &getCurrentDrawCmdBuffer() {
        return drawCmdBuffers[currentFrame];
    }
    inline vk::Viewport getDefaultViewport(vk::Extent2D extent) {
        return vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                            static_cast<float>(extent.height), 0.0f, 1.0f);
    }
    inline vk::Rect2D getDefaultScissor(vk::Extent2D extent) {
        return vk::Rect2D(vk::Offset2D(0, 0), extent);
    }

    inline Texture &getFinalColorTexture() {
        return offscreenResources.colorTexture;
    }

    inline vk::Extent2D getFinalExtent() {
        return device->getUiLayout()->getOffscreenExtent();
    }

    virtual void onInit() {}
    virtual void onPrepare() {}
    virtual void onUpdate() {}
    virtual void onDestroy() {}
    virtual void onWindowResize() {}
    virtual void onSceneResize() {}

    virtual void draw() = 0;
    virtual void drawUi() {}

    void prepareFrame();
    void drawFrame();
    void submitFrame();

    Device *device;

    vk::QueryPool queryPool;
    uint64_t timestamps[2];
    uint64_t timestampPeriod;

    uint32_t imageIndex;
    vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;

    std::vector<vk::CommandBuffer> drawCmdBuffers;
    std::unique_ptr<Swapchain> swapchain;

    vk::AttachmentLoadOp swapchainLoadOp = vk::AttachmentLoadOp::eClear;

    struct {
        Texture colorTexture;
    } offscreenResources;

    uint32_t currentFrame = 0;

    struct {
        std::vector<vk::Semaphore> imageAvailable;
        std::vector<vk::Semaphore> renderFinished;
    } semaphores;

    struct {
        std::vector<vk::Fence> inFlight;
    } fences;

   private:
};
}  // namespace Engine