#include "core/Core.hpp"

#include "gfx/vulkan/Renderer.hpp"
#include "gfx/vulkan/Resource.hpp"
#include "gfx/vulkan/Pipeline.hpp"

#include <glm/glm.hpp>

using namespace Engine;

class SimpleRenderer : public Renderer {
   public:
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount;

    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;

    struct Vertex {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec4 color;

        static vk::VertexInputBindingDescription getBindingDescription() {
            return vk::VertexInputBindingDescription(
                0, sizeof(Vertex), vk::VertexInputRate::eVertex);
        }

        static std::array<vk::VertexInputAttributeDescription, 3>
        getAttributeDescriptions() {
            return {
                vk::VertexInputAttributeDescription(
                    0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(
                    1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)),
                vk::VertexInputAttributeDescription(
                    2, 0, vk::Format::eR32G32B32A32Sfloat,
                    offsetof(Vertex, color)),
            };
        }
    };

   private:
    void onInit() override {}

    void onPrepare() override {
        prepareData();
        buildPipeline();
    }

    void prepareData() {
        std::vector<Vertex> vertices = {
            {{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = {0, 1, 2};

        auto vbSize = sizeof(Vertex) * vertices.size();
        vertexBuffer = device->createBuffer();
        vertexBuffer.allocate(vbSize,
                              vk::BufferUsageFlagBits::eVertexBuffer |
                                  vk::BufferUsageFlagBits::eTransferDst);

        auto stagingBuffer = device->createBuffer();
        stagingBuffer.allocate(vbSize, vk::BufferUsageFlagBits::eTransferSrc);

        void *data = stagingBuffer.map();
        std::memcpy(data, vertices.data(), vbSize);
        stagingBuffer.unmap();

        auto cmdBuffer = device->allocateCommandBuffer();
        cmdBuffer.copyBuffer(stagingBuffer.buffer, vertexBuffer.buffer,
                             {vk::BufferCopy{0, 0, vbSize}});
        device->flushCommandBuffer(cmdBuffer);

        stagingBuffer.destroy();

        indexBuffer = device->createBuffer();
        indexBuffer.allocate(sizeof(uint16_t) * indices.size(),
                             vk::BufferUsageFlagBits::eIndexBuffer);
        indexCount = static_cast<uint32_t>(indices.size());

        data = indexBuffer.map();
        std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());
        indexBuffer.unmap();
    }

    void buildPipeline() {
        auto logicalDevice = device->getLogicalDevice();
        PipelineBuilder pipelineBuilder(logicalDevice);

        auto vertShader = device->createShaderModule("test/triangle.vert.spv");
        auto fragShader = device->createShaderModule("test/triangle.frag.spv");

        vk::PipelineShaderStageCreateInfo vertexShaderStageCI{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShader,
            .pName = "vert",
        };
        vk::PipelineShaderStageCreateInfo fragmentShaderStageCI{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragShader,
            .pName = "frag",
        };

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        pipelineBuilder.vertexInputCI.vertexBindingDescriptionCount = 1;
        pipelineBuilder.vertexInputCI.pVertexBindingDescriptions =
            &bindingDescription;
        pipelineBuilder.vertexInputCI.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size());
        pipelineBuilder.vertexInputCI.pVertexAttributeDescriptions =
            attributeDescriptions.data();

        pipelineBuilder.addColorAttachment(swapchain->format);

        pipelineLayout = logicalDevice.createPipelineLayout({
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
        });

        pipelineBuilder.setLayout(pipelineLayout);
        pipelineBuilder.shaderStages.push_back(vertexShaderStageCI);
        pipelineBuilder.shaderStages.push_back(fragmentShaderStageCI);
        pipeline = pipelineBuilder.build();

        logicalDevice.destroyShaderModule(vertShader);
        logicalDevice.destroyShaderModule(fragShader);
    }

    void onDestroy() override {
        vertexBuffer.destroy();
        indexBuffer.destroy();

        device->getLogicalDevice().destroyPipeline(pipeline);
        device->getLogicalDevice().destroyPipelineLayout(pipelineLayout);
    }

    void draw() override {
        auto &cmdBuffer = getCurrentDrawCmdBuffer();

        auto extent = getFinalExtent();

        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = getFinalColorTexture().imageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f,
                                                                   0.0f, 1.0f}},
        };

        vk::RenderingInfo renderingInfo{
            .renderArea =
                vk::Rect2D{
                    .offset = vk::Offset2D{0, 0},
                    .extent = extent,
                },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
        };

        cmdBuffer.beginRendering(renderingInfo);
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmdBuffer.bindVertexBuffers(0, {vertexBuffer.buffer}, {0});
        cmdBuffer.bindIndexBuffer(indexBuffer.buffer, 0,
                                  vk::IndexType::eUint16);

        cmdBuffer.setViewport(0, getDefaultViewport(extent));
        cmdBuffer.setScissor(0, getDefaultScissor(extent));

        cmdBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
        cmdBuffer.endRendering();
    }
};

class App : public Application {
   public:
   private:
    void onInit() override { appName = "Simple Triangle"; }
};

int main() {
    auto app = std::make_unique<App>();
    app->renderer = std::make_unique<SimpleRenderer>();
    app->init();
    app->mainLoop();
    app->destroy();

    return 0;
}