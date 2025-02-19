#include "core/Core.hpp"

#include "gfx/vulkan/Renderer.hpp"
#include "gfx/vulkan/Resource.hpp"
#include "gfx/vulkan/Device.hpp"
#include "gfx/vulkan/Pipeline.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

using namespace Engine;

class SimpleRenderer : public Renderer {
   public:
   private:
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount;

    vk::DescriptorSetLayout uboLayout;
    vk::DescriptorSet uboSet;
    Buffer uniformBuffer;

    Texture texture;
    vk::DescriptorSetLayout textureLayout;
    vk::DescriptorSet textureSet;

    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;

    Texture depthTexture;

    struct Vertex {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec4 color;

        static vk::VertexInputBindingDescription getBindingDescription() {
            return vk::VertexInputBindingDescription(
                0, sizeof(Vertex), vk::VertexInputRate::eVertex
            );
        }

        static std::array<vk::VertexInputAttributeDescription, 3>
        getAttributeDescriptions() {
            return {
                vk::VertexInputAttributeDescription(
                    0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)
                ),
                vk::VertexInputAttributeDescription(
                    1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)
                ),
                vk::VertexInputAttributeDescription(
                    2, 0, vk::Format::eR32G32B32A32Sfloat,
                    offsetof(Vertex, color)
                ),
            };
        }
    };

    struct UBO {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    } ubo;

    void onPrepare() override {
        msaaSamples = vk::SampleCountFlagBits::e1;

        depthTexture = device->createTexture();
        depthTexture.allocate(
            swapchain->getExtent(), 1, vk::Format::eD32SfloatS8Uint,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageAspectFlagBits::eDepth
        );

        prepareData();
        prepareTexture();
        prepareUBO();
        buildPipeline();
    }

    void onResize() override {
        depthTexture.destroy();
        depthTexture.allocate(
            swapchain->getExtent(), 1, vk::Format::eD32SfloatS8Uint,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageAspectFlagBits::eDepth
        );
    }

    void onUpdate() override {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        double time =
            std::chrono::duration<double, std::milli>(currentTime - startTime)
                .count();

        ubo.model = glm::rotate(
            glm::mat4(1.0f), (float)time / 1000 * glm::radians(90.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.proj = glm::perspective(
            glm::radians(45.0f),
            swapchain->getExtent().width / (float)swapchain->getExtent().height,
            0.1f, 10.0f
        );
        ubo.proj[1][1] *= -1;

        std::memcpy(
            uniformBuffer.allocationInfo.pMappedData, &ubo, sizeof(UBO)
        );
    }

    void onDestroy() override {
        uniformBuffer.destroy();
        vertexBuffer.destroy();
        indexBuffer.destroy();
        texture.destroy();
        depthTexture.destroy();

        auto logicalDevice = device->getLogicalDevice();
        logicalDevice.destroyDescriptorSetLayout(uboLayout);
        logicalDevice.destroyDescriptorSetLayout(textureLayout);
        logicalDevice.destroyPipeline(pipeline);
        logicalDevice.destroyPipelineLayout(pipelineLayout);
    }

    void draw() override {
        auto& cmdBuffer = getCurrentDrawCmdBuffer();

        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = swapchain->getImageView(imageIndex),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearColorValue{std::array<float, 4>{
                0.0f, 0.0f, 0.0f, 1.0f}},
        };

        vk::RenderingAttachmentInfo depthAttachmentInfo{
            .imageView = depthTexture.imageView,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue =
                vk::ClearValue{
                    .depthStencil =
                        {
                            .depth = 1.0f,
                            .stencil = 0,
                        },
                },
        };

        vk::RenderingInfo renderingInfo{
            .renderArea =
                vk::Rect2D{
                    .offset = vk::Offset2D{0, 0},
                    .extent = swapchain->getExtent(),
                },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo,
        };

        cmdBuffer.beginRendering(renderingInfo);
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmdBuffer.bindVertexBuffers(0, {vertexBuffer.buffer}, {0});
        cmdBuffer.bindIndexBuffer(
            indexBuffer.buffer, 0, vk::IndexType::eUint16
        );
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
            {uboSet, textureSet}, {}
        );

        auto extent = swapchain->getExtent();
        cmdBuffer.setViewport(0, getDefaultViewport(extent));
        cmdBuffer.setScissor(0, getDefaultScissor(extent));

        cmdBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
        cmdBuffer.endRendering();
    }

    void prepareUBO() {
        auto logicalDevice = device->getLogicalDevice();

        auto binding = vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
        };

        uboLayout = logicalDevice.createDescriptorSetLayout({
            .bindingCount = 1,
            .pBindings = &binding,
        });

        uboSet = logicalDevice
                     .allocateDescriptorSets({
                         .descriptorPool = device->getDescriptorPool(),
                         .descriptorSetCount = 1,
                         .pSetLayouts = &uboLayout,
                     })
                     .front();

        uniformBuffer = device->createBuffer();
        uniformBuffer.allocate(
            sizeof(UBO), vk::BufferUsageFlagBits::eUniformBuffer, true
        );

        vk::DescriptorBufferInfo bufferInfo{
            .buffer = uniformBuffer.buffer,
            .offset = 0,
            .range = sizeof(UBO),
        };

        vk::WriteDescriptorSet descriptorWrite{
            .dstSet = uboSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo,
        };
        logicalDevice.updateDescriptorSets(descriptorWrite, {});
    }

    void prepareTexture() {
        auto logicalDevice = device->getLogicalDevice();
        texture = device->createTexture();
        texture.loadFromFile("nikke.jpg");

        auto binding = vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };

        textureLayout = logicalDevice.createDescriptorSetLayout({
            .bindingCount = 1,
            .pBindings = &binding,
        });

        textureSet = logicalDevice
                         .allocateDescriptorSets({
                             .descriptorPool = device->getDescriptorPool(),
                             .descriptorSetCount = 1,
                             .pSetLayouts = &textureLayout,
                         })
                         .front();

        vk::DescriptorImageInfo imageInfo{
            .sampler = texture.sampler,
            .imageView = texture.imageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        vk::WriteDescriptorSet descriptorWrite{
            .dstSet = textureSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo,
        };
        logicalDevice.updateDescriptorSets(descriptorWrite, {});
    }

    void prepareData() {
        std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.2f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.2f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.2f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.2f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},

            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

        auto vbSize = sizeof(Vertex) * vertices.size();
        vertexBuffer = device->createBuffer();
        vertexBuffer.allocate(
            vbSize, vk::BufferUsageFlagBits::eVertexBuffer |
                        vk::BufferUsageFlagBits::eTransferDst
        );

        auto stagingBuffer = device->createBuffer();
        stagingBuffer.allocate(
            vbSize, vk::BufferUsageFlagBits::eTransferSrc, true
        );
        std::memcpy(
            stagingBuffer.allocationInfo.pMappedData, vertices.data(), vbSize
        );

        auto cmdBuffer = device->allocateCommandBuffer();
        cmdBuffer.copyBuffer(
            stagingBuffer.buffer, vertexBuffer.buffer,
            {vk::BufferCopy{0, 0, vbSize}}
        );
        device->flushCommandBuffer(cmdBuffer);

        stagingBuffer.destroy();

        indexBuffer = device->createBuffer();
        indexBuffer.allocate(
            sizeof(uint16_t) * indices.size(),
            vk::BufferUsageFlagBits::eIndexBuffer
        );
        indexCount = static_cast<uint32_t>(indices.size());

        void* data = indexBuffer.map();
        std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());
        indexBuffer.unmap();
    }

    void buildPipeline() {
        auto logicalDevice = device->getLogicalDevice();
        PipelineBuilder pipelineBuilder(logicalDevice);

        auto vertShader = device->createShaderModule("test/texture.vert.spv");
        auto fragShader = device->createShaderModule("test/texture.frag.spv");

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

        pipelineBuilder.rasterizationCI.frontFace =
            vk::FrontFace::eCounterClockwise;

        pipelineBuilder.addColorAttachment(swapchain->format);

        vk::DescriptorSetLayout setLayouts[2] = {uboLayout, textureLayout};
        pipelineLayout = logicalDevice.createPipelineLayout({
            .setLayoutCount = 2,
            .pSetLayouts = setLayouts,
        });

        pipelineBuilder.setLayout(pipelineLayout);
        pipelineBuilder.shaderStages.push_back(vertexShaderStageCI);
        pipelineBuilder.shaderStages.push_back(fragmentShaderStageCI);
        pipeline = pipelineBuilder.build();

        logicalDevice.destroyShaderModule(vertShader);
        logicalDevice.destroyShaderModule(fragShader);
    }
};

class App : public Application {
   public:
    void onInit() override { appName = "Simple Texture"; }
};

int main() {
    auto app = std::make_unique<App>();
    app->renderer = std::make_unique<SimpleRenderer>();
    app->init();
    app->mainLoop();
    app->destroy();

    return 0;
}