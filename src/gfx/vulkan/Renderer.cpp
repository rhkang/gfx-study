#include "gfx/vulkan/Renderer.hpp"
#include "gfx/vulkan/Utils.hpp"

namespace Engine
{
    void Renderer::init(Device *device)
    {
        this->device = device;

        auto logicalDevice = device->getLogicalDevice();
        auto physicalDevice = device->getPhysicalDevice();
        auto surface = device->getSurface();
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
        auto surfacePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

        uint32_t minImageCount = (surfaceCapabilities.minImageCount < 2) ? 2 : surfaceCapabilities.minImageCount;
        assert(surfaceCapabilities.maxImageCount >= 3);

        auto presentMode = vk::PresentModeKHR::eFifo;

        swapchain = std::make_unique<Swapchain>();
        swapchain->extent = surfaceCapabilities.currentExtent;
        swapchain->imageCount = minImageCount + 1;
        swapchain->format = vk::Format::eB8G8R8A8Srgb;
        swapchain->colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        swapchain->presentMode = presentMode;
        swapchain->create(logicalDevice, surface, device->getQueueFamilyIndex());

        semaphores.imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
        semaphores.renderFinished.resize(MAX_FRAMES_IN_FLIGHT);
        fences.inFlight.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            semaphores.imageAvailable[i] = logicalDevice.createSemaphore({});
            semaphores.renderFinished[i] = logicalDevice.createSemaphore({});
            fences.inFlight[i] = logicalDevice.createFence({
                .flags = vk::FenceCreateFlagBits::eSignaled,
            });
        }

        auto physicalDeviceProperties = physicalDevice.getProperties();
        vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts | physicalDeviceProperties.limits.framebufferDepthSampleCounts;

        if (counts & vk::SampleCountFlagBits::e64)
            msaaSamples = vk::SampleCountFlagBits::e64;
        else if (counts & vk::SampleCountFlagBits::e32)
            msaaSamples = vk::SampleCountFlagBits::e32;
        else if (counts & vk::SampleCountFlagBits::e16)
            msaaSamples = vk::SampleCountFlagBits::e16;
        else if (counts & vk::SampleCountFlagBits::e8)
            msaaSamples = vk::SampleCountFlagBits::e8;
        else if (counts & vk::SampleCountFlagBits::e4)
            msaaSamples = vk::SampleCountFlagBits::e4;
        else if (counts & vk::SampleCountFlagBits::e2)
            msaaSamples = vk::SampleCountFlagBits::e2;
        else
            msaaSamples = vk::SampleCountFlagBits::e1;

        queryPool = logicalDevice.createQueryPool({
            .queryType = vk::QueryType::eTimestamp,
            .queryCount = 2,
            });
        timestampPeriod = physicalDevice.getProperties().limits.timestampPeriod;

        drawCmdBuffers = logicalDevice.allocateCommandBuffers({
            .commandPool = device->getCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        });

        onInit();
    }

    void Renderer::destroy()
    {
        onDestroy();

		auto logicalDevice = device->getLogicalDevice();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            logicalDevice.destroySemaphore(semaphores.imageAvailable[i]);
            logicalDevice.destroySemaphore(semaphores.renderFinished[i]);
            logicalDevice.destroyFence(fences.inFlight[i]);
        }

        swapchain.reset();
        logicalDevice.destroyQueryPool(queryPool);
    }

    void Renderer::prepare()
    {
		onPrepare();
    }

    void Renderer::update()
    {
		onUpdate();
    }

    void Renderer::render()
    {
        prepareFrame();
        drawFrame();
		submitFrame();
    }

    void Renderer::prepareFrame()
    {
        auto logicalDevice = device->getLogicalDevice();
        auto result = logicalDevice.waitForFences(fences.inFlight[currentFrame], vk::True, UINT64_MAX);
        assert(result == vk::Result::eSuccess);

        imageIndex = swapchain->acquireNextImage(semaphores.imageAvailable[currentFrame]);
        if (imageIndex == UINT32_MAX)
        {
            handleWindowResize();
            return;
        }

        logicalDevice.resetFences(fences.inFlight[currentFrame]);
        device->getUiLayout()->prepareFrame();
    }

    void Renderer::drawFrame()
    {
        auto& cmdBuffer = getCurrentDrawCmdBuffer();
        cmdBuffer.reset();
        cmdBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        cmdBuffer.resetQueryPool(queryPool, 0, 2);
        cmdBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, queryPool, 0);

        imageLayoutTransition(
            cmdBuffer, vk::ImageAspectFlagBits::eColor,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eNone,
            vk::AccessFlagBits::eColorAttachmentWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            swapchain->getImage(imageIndex));

        draw();

        vk::RenderingAttachmentInfo finalColorAttachmentInfo{
            .imageView = swapchain->getImageView(imageIndex),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        vk::RenderingInfo uiRenderingInfo{
            .renderArea = vk::Rect2D{
                .offset = vk::Offset2D{0, 0},
                .extent = swapchain->extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &finalColorAttachmentInfo,
        };

        cmdBuffer.beginRendering(uiRenderingInfo);
        drawUi();
        device->getUiLayout()->draw(cmdBuffer);
        cmdBuffer.endRendering();

        imageLayoutTransition(
            cmdBuffer, vk::ImageAspectFlagBits::eColor,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::AccessFlagBits::eColorAttachmentWrite,
            vk::AccessFlagBits::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            swapchain->getImage(imageIndex));

        cmdBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, queryPool, 1);
        cmdBuffer.end();
    }

    void Renderer::submitFrame()
    {
        auto queue = device->getQueue();
		auto logicalDevice = device->getLogicalDevice();

        vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::Semaphore waitSemaphores[] = { semaphores.imageAvailable[currentFrame] };
        vk::Semaphore signalSemaphores[] = { semaphores.renderFinished[currentFrame] };

        queue.submit({ vk::SubmitInfo{
                         .waitSemaphoreCount = 1,
                         .pWaitSemaphores = waitSemaphores,
                         .pWaitDstStageMask = waitStages,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &drawCmdBuffers[currentFrame],
                         .signalSemaphoreCount = 1,
                         .pSignalSemaphores = signalSemaphores,
                     } },
                     fences.inFlight[currentFrame]);

        vk::SwapchainKHR swapchains[] = { swapchain->getSwapchain() };
        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = reinterpret_cast<VkSemaphore*>(signalSemaphores),
            .swapchainCount = 1,
            .pSwapchains = reinterpret_cast<VkSwapchainKHR*>(swapchains),
            .pImageIndices = &imageIndex,
        };

        auto result = vkQueuePresentKHR(queue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowResized)
        {
            handleWindowResize();
        }

        vk::Result queryResult = logicalDevice.getQueryPoolResults(queryPool, 0, 2, sizeof(uint64_t) * 2, &timestamps, sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
        if (queryResult == vk::Result::eSuccess)
        {
            uint64_t startTimestamp = timestamps[0];
            uint64_t endTimestamp = timestamps[1];
            double gpuTime = static_cast<double>(endTimestamp - startTimestamp) * timestampPeriod / 1000000.0;
            device->getUiLayout()->renderTime = gpuTime;
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Renderer::handleWindowResize()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = device->getPhysicalDevice().getSurfaceCapabilitiesKHR(device->getSurface());
        vk::Extent2D extent = surfaceCapabilities.currentExtent;
        if (extent.width == 0 && extent.height == 0)
        {
            return;
        }

        swapchain->recreate(extent);
        onResize();
    }
}