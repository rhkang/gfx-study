#include "core/Core.hpp"

#include "gfx/vulkan/Renderer.hpp"
#include "gfx/vulkan/Resource.hpp"
#include "gfx/vulkan/Device.hpp"
#include "gfx/vulkan/Pipeline.hpp"
#include "gfx/vulkan/Utils.hpp"

#include "Model.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui_impl_vulkan.h>

using namespace Engine;

struct BufferIndex {
    uint32_t startIndex;
    uint32_t count;
};

struct ModelInfo {
    uint32_t id;

    BufferIndex vertex;
    BufferIndex index;

    glm::mat4 model;
};

struct SceneData {
    std::vector<ModelInfo> modelInfos;

    Buffer vertexBuffer;
    Buffer indexBuffer;
    Buffer uniformBuffer;

    vk::DescriptorSetLayout descriptorSetLayout;
    vk::DescriptorSet descriptorSet;

    ModelInfo &getModelInfoById(uint32_t id) {
        for (auto &modelInfo : modelInfos) {
            if (modelInfo.id == id) {
                return modelInfo;
            }
        }

        throw std::runtime_error("Model Info not found");
    }
};

struct UBO {
    glm::mat4 view;
    glm::mat4 proj;

    glm::mat4 lightView;
    glm::mat4 lightProj;
    glm::vec3 lightPos;

    float shadowBias;
    float enablePCF;
};

struct PerObjectData {
    glm::mat4 model;
};

class ShadowPassRenderer : public Renderer {
   public:
   private:
    SceneData sceneData;

    Texture shadowTexture;
    Texture depthTexture;

    vk::PipelineLayout shadowPipelineLayout;
    vk::Pipeline shadowPipeline;

    vk::PipelineLayout finalImagePipelineLayout;
    vk::Pipeline finalImagePipeline;

    vk::DescriptorSetLayout shadowDescriptorSetLayout;
    vk::DescriptorSet shadowDescriptorSet;

    vk::Extent2D shadowMapExtent = {4096, 4096};

    Model cube;
    Model plane;

    UBO ubo;
    PerObjectData perObjectData;

    float shadowBound = 2.0f;
    float rotation = 30.0f;
    float distance = 2.0f;

    VkDescriptorSet shadowSetForImGui;

    float shadowBias = 0.000061035f;
    bool enablePCF = true;

    struct {
        glm::vec3 pos;
        float radius = 20.0f;
    } light;

    void onDestroy() override {
        sceneData.uniformBuffer.destroy();
        sceneData.vertexBuffer.destroy();
        sceneData.indexBuffer.destroy();

        shadowTexture.destroy();
        depthTexture.destroy();

        auto logicalDevice = device->getLogicalDevice();
        logicalDevice.destroyDescriptorSetLayout(sceneData.descriptorSetLayout);

        logicalDevice.destroyPipelineLayout(shadowPipelineLayout);
        logicalDevice.destroyPipeline(shadowPipeline);
        logicalDevice.destroyPipelineLayout(finalImagePipelineLayout);
        logicalDevice.destroyPipeline(finalImagePipeline);
        logicalDevice.destroyDescriptorSetLayout(shadowDescriptorSetLayout);
    }

    void onResize() override {
        depthTexture.destroy();
        depthTexture.allocate(
            swapchain->getExtent(), 1, vk::Format::eD32Sfloat,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageAspectFlagBits::eDepth
        );
    }

    void onPrepare() override {
        msaaSamples = vk::SampleCountFlagBits::e1;

        depthTexture = device->createTexture();
        depthTexture.allocate(
            swapchain->getExtent(), 1, vk::Format::eD32Sfloat,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageAspectFlagBits::eDepth
        );

        sceneData.vertexBuffer = device->createBuffer();
        sceneData.indexBuffer = device->createBuffer();
        sceneData.uniformBuffer = device->createBuffer();

        sceneData.vertexBuffer.allocate(
            100000, vk::BufferUsageFlagBits::eVertexBuffer |
                        vk::BufferUsageFlagBits::eTransferDst
        );
        sceneData.indexBuffer.allocate(
            1000000, vk::BufferUsageFlagBits::eIndexBuffer
        );
        sceneData.uniformBuffer.allocate(
            sizeof(UBO), vk::BufferUsageFlagBits::eUniformBuffer, true
        );

        vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex |
                          vk::ShaderStageFlagBits::eFragment,
        };

        auto logicalDevice = device->getLogicalDevice();
        sceneData.descriptorSetLayout = logicalDevice.createDescriptorSetLayout(
            {.bindingCount = 1, .pBindings = &descriptorSetLayoutBinding}
        );

        sceneData.descriptorSet =
            logicalDevice
                .allocateDescriptorSets(
                    {.descriptorPool = device->getDescriptorPool(),
                     .descriptorSetCount = 1,
                     .pSetLayouts = &sceneData.descriptorSetLayout}
                )
                .front();

        shadowTexture = device->createTexture();

        vk::DescriptorSetLayoutBinding shadowDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };

        shadowDescriptorSetLayout = logicalDevice.createDescriptorSetLayout(
            {.bindingCount = 1, .pBindings = &shadowDescriptorSetLayoutBinding}
        );

        shadowDescriptorSet =
            logicalDevice
                .allocateDescriptorSets(
                    {.descriptorPool = device->getDescriptorPool(),
                     .descriptorSetCount = 1,
                     .pSetLayouts = &shadowDescriptorSetLayout}
                )
                .front();

        shadowTexture.allocate(
            shadowMapExtent, 1, vk::Format::eD32Sfloat,
            vk::ImageUsageFlagBits::eDepthStencilAttachment |
                vk::ImageUsageFlagBits::eSampled,
            vk::ImageAspectFlagBits::eDepth
        );
        shadowTexture.addressMode = vk::SamplerAddressMode::eClampToBorder;
        shadowTexture.createSampler();

        vk::DescriptorBufferInfo bufferInfo{
            .buffer = sceneData.uniformBuffer.buffer,
            .offset = 0,
            .range = sizeof(UBO),
        };

        vk::WriteDescriptorSet uboWrite{
            .dstSet = sceneData.descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo,
        };

        logicalDevice.updateDescriptorSets(uboWrite, {});

        vk::DescriptorImageInfo imageInfo{
            .sampler = shadowTexture.sampler,
            .imageView = shadowTexture.imageView,
            .imageLayout = vk::ImageLayout::eDepthReadOnlyOptimal,
        };

        vk::WriteDescriptorSet shadowSetWrite{
            .dstSet = shadowDescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo,
        };

        logicalDevice.updateDescriptorSets(shadowSetWrite, {});

        cube.id = 1;
        cube.createCube();
        plane.id = 2;
        plane.createPlane();

        buildPipeline();

        AddModel(cube);
        AddModel(plane);

        auto &cubeInfo = sceneData.getModelInfoById(cube.id);
        auto &planeInfo = sceneData.getModelInfoById(plane.id);

        cubeInfo.model = glm::translate(
            glm::scale(cubeInfo.model, glm::vec3(0.5f, 0.5f, 0.5f)),
            glm::vec3(0.0f, 0.0f, 0.8f)
        );
        planeInfo.model =
            glm::scale(planeInfo.model, glm::vec3(100.0, 100.0, 0.0));
        perObjectData.model = glm::mat4(1.0f);
        light.pos.z = 20.0f;

        shadowSetForImGui = ImGui_ImplVulkan_AddTexture(
            shadowTexture.sampler, shadowTexture.imageView,
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
        );
    }

    void onUpdate() override {
        ubo.view = glm::lookAt(
            glm::vec3(distance), glm::vec3(0.0f, 0.0f, 0.5f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.proj = glm::perspective(
            glm::radians(45.0f),
            swapchain->getExtent().width / (float)swapchain->getExtent().height,
            0.1f, 1000.0f
        );
        ubo.proj[1][1] *= -1;

        ubo.lightView = glm::lookAt(
            light.pos, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.lightProj = glm::ortho(
            -shadowBound, shadowBound, -shadowBound, shadowBound, 0.1f, 100.0f
        );
        ubo.lightProj[1][1] *= -1;

        float angle = glm::radians(rotation);
        float x = light.radius * cos(angle);
        float y = light.radius * sin(angle);
        light.pos.x = x;
        light.pos.y = y;
        ubo.lightPos = light.pos;
        ubo.shadowBias = shadowBias;
        ubo.enablePCF = (enablePCF) ? 1.0f : 0.0f;

        std::memcpy(
            sceneData.uniformBuffer.allocationInfo.pMappedData, &ubo,
            sizeof(UBO)
        );
    }

    void draw() override {
        auto &cmdBuffer = getCurrentDrawCmdBuffer();

        imageLayoutTransition(
            cmdBuffer, vk::ImageAspectFlagBits::eDepth,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eEarlyFragmentTests,
            vk::AccessFlagBits::eNone,
            vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthStencilAttachmentOptimal, shadowTexture.image
        );

        vk::RenderingAttachmentInfo shadowAttachmentInfo{
            .imageView = shadowTexture.imageView,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue =
                vk::ClearValue{
                    .depthStencil =
                        {
                            .depth = 1.0f,
                            .stencil = 0,
                        }},
        };

        vk::RenderingInfo shadowpassInfo{
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent = shadowMapExtent,
                },
            .layerCount = 1,
            .colorAttachmentCount = 0,
            .pDepthAttachment = &shadowAttachmentInfo,
        };

        cmdBuffer.beginRendering(shadowpassInfo);
        cmdBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, shadowPipeline
        );
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, shadowPipelineLayout, 0,
            {sceneData.descriptorSet}, {}
        );
        cmdBuffer.bindVertexBuffers(0, {sceneData.vertexBuffer.buffer}, {0});
        cmdBuffer.bindIndexBuffer(
            sceneData.indexBuffer.buffer, 0, vk::IndexType::eUint16
        );

        cmdBuffer.setViewport(0, getDefaultViewport(shadowMapExtent));
        cmdBuffer.setScissor(0, getDefaultScissor(shadowMapExtent));

        for (auto &model : sceneData.modelInfos) {
            if (model.id == plane.id) {
                continue;
            }

            perObjectData.model = model.model;
            cmdBuffer.pushConstants(
                finalImagePipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                sizeof(PerObjectData), &perObjectData
            );
            cmdBuffer.drawIndexed(
                model.index.count, 1, model.index.startIndex,
                model.vertex.startIndex, 0
            );
        }

        cmdBuffer.endRendering();

        imageLayoutTransition(
            cmdBuffer, vk::ImageAspectFlagBits::eDepth,
            vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::ImageLayout::eDepthReadOnlyOptimal, shadowTexture.image
        );

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
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue =
                vk::ClearValue{
                    .depthStencil =
                        {
                            .depth = 1.0f,
                            .stencil = 0,
                        }},
        };

        vk::RenderingInfo writeToSwapchain{
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent = swapchain->extent,
                },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo,
        };

        cmdBuffer.beginRendering(writeToSwapchain);
        cmdBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, finalImagePipeline
        );
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, finalImagePipelineLayout, 1,
            {shadowDescriptorSet}, {}
        );

        auto extent = swapchain->getExtent();
        cmdBuffer.setViewport(0, getDefaultViewport(extent));
        cmdBuffer.setScissor(0, getDefaultScissor(extent));

        for (auto &model : sceneData.modelInfos) {
            perObjectData.model = model.model;
            cmdBuffer.pushConstants(
                finalImagePipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                sizeof(PerObjectData), &perObjectData
            );
            cmdBuffer.drawIndexed(
                model.index.count, 1, model.index.startIndex,
                model.vertex.startIndex, 0
            );
        }

        cmdBuffer.endRendering();
    }

    void drawUi() override {
        auto fontScale = device->getUiLayout()->fontScale;
        auto boxWidth = fontScale * 12;
        auto boxHeight = fontScale * 17;

        ImGui::SetNextWindowSize(ImVec2(boxWidth, boxHeight), ImGuiCond_Once);
        ImGui::SetNextWindowPos(
            ImVec2(
                ImGui::GetIO().DisplaySize.x - boxWidth - fontScale, fontScale
            ),
            ImGuiCond_Always
        );

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("Control", nullptr, flags);

        ImGui::Text("Shadow Bound");
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::SliderFloat("##ShadowBound", &shadowBound, 0.1f, 10.0f);

        ImGui::Text("# divide for bias");
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );

        const int maxDivide = 40;
        static int numDivide = 14;
        if (ImGui::SliderInt("##bias", &numDivide, 0, maxDivide)) {
            shadowBias = 1.0f;
            for (int i = 0; i < numDivide; i++) {
                shadowBias /= 2.0f;
            }
        }

        ImGui::Text("Camera Distance");
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::SliderFloat("##Distance", &distance, 1.0f, 10.0f);

        ImGui::Text("Light Rotation");
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::SliderFloat("##rotation", &rotation, -180.0f, 540.0f);

        ImGui::Text("Light Z");
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::SliderFloat("##pz", &light.pos.z, 3.0f, 100.0f);

        ImGui::Checkbox("Enable PCF", &enablePCF);
        ImGui::End();

        float texturePrintSize = 240.0f;
        ImGui::SetNextWindowPos(
            ImVec2(
                ImGui::GetIO().DisplaySize.x - boxWidth - texturePrintSize -
                    fontScale * 3,
                fontScale
            ),
            ImGuiCond_Always
        );
        ImGui::Begin("Shadow Map", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image(
            (ImTextureID)shadowSetForImGui,
            ImVec2(texturePrintSize, texturePrintSize)
        );
        ImGui::End();
    }

    void buildPipeline() {
        auto pushConstantRange = vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(PerObjectData),
        };

        auto logicalDevice = device->getLogicalDevice();
        PipelineBuilder shadowPassBuilder(logicalDevice);

        auto shadowVertShader =
            device->createShaderModule("test/shadow_gen.vert.spv");

        auto vertexAttribute = Vertex::getAttributeDescriptions();
        auto vertexDescription = Vertex::getBindingDescription();
        shadowPassBuilder.vertexInputCI.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(vertexAttribute.size());
        shadowPassBuilder.vertexInputCI.pVertexAttributeDescriptions =
            vertexAttribute.data();
        shadowPassBuilder.vertexInputCI.vertexBindingDescriptionCount = 1;
        shadowPassBuilder.vertexInputCI.pVertexBindingDescriptions =
            &vertexDescription;

        shadowPassBuilder.depthAttachmentFormat = vk::Format::eD32Sfloat;

        shadowPassBuilder.shaderStages.push_back({
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shadowVertShader,
            .pName = "vert",
        });

        shadowPipelineLayout = logicalDevice.createPipelineLayout({
            .setLayoutCount = 1,
            .pSetLayouts = &sceneData.descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        });

        shadowPassBuilder.setLayout(shadowPipelineLayout);
        shadowPipeline = shadowPassBuilder.build();
        logicalDevice.destroyShaderModule(shadowVertShader);

        PipelineBuilder finalImageBuilder(logicalDevice);
        auto finalImageVertShader =
            device->createShaderModule("test/shadow.vert.spv");
        auto finalImageFragShader =
            device->createShaderModule("test/shadow.frag.spv");

        finalImageBuilder.vertexInputCI.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(vertexAttribute.size());
        finalImageBuilder.vertexInputCI.pVertexAttributeDescriptions =
            vertexAttribute.data();
        finalImageBuilder.vertexInputCI.vertexBindingDescriptionCount = 1;
        finalImageBuilder.vertexInputCI.pVertexBindingDescriptions =
            &vertexDescription;

        finalImageBuilder.shaderStages.push_back({
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = finalImageVertShader,
            .pName = "vert",
        });

        finalImageBuilder.shaderStages.push_back({
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = finalImageFragShader,
            .pName = "frag",
        });

        finalImageBuilder.addColorAttachment(swapchain->format);
        finalImageBuilder.depthAttachmentFormat = vk::Format::eD32Sfloat;

        vk::DescriptorSetLayout descriptorSetLayouts[2] = {
            sceneData.descriptorSetLayout, shadowDescriptorSetLayout};
        finalImagePipelineLayout = logicalDevice.createPipelineLayout({
            .setLayoutCount = 2,
            .pSetLayouts = descriptorSetLayouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        });

        finalImageBuilder.setLayout(finalImagePipelineLayout);
        finalImagePipeline = finalImageBuilder.build();

        logicalDevice.destroyShaderModule(finalImageVertShader);
        logicalDevice.destroyShaderModule(finalImageFragShader);
    }

    void AddModel(Model &model) {
        uint32_t vbSize = model.getTotalVerticesSize();
        uint32_t ibSize = model.getTotalIndicesSize();

        auto stagingBuffer = device->createBuffer();
        stagingBuffer.allocate(
            vbSize, vk::BufferUsageFlagBits::eTransferSrc, true
        );
        std::memcpy(
            stagingBuffer.allocationInfo.pMappedData,
            model.mesh.vertices.data(), vbSize
        );

        auto cmdBuffer = device->allocateCommandBuffer();
        uint32_t vDstOffset = 0;

        if (!sceneData.modelInfos.empty()) {
            auto &vTail = sceneData.modelInfos.back().vertex;
            vDstOffset = vTail.startIndex + vTail.count;
        }
        cmdBuffer.copyBuffer(
            stagingBuffer.buffer, sceneData.vertexBuffer.buffer,
            vk::BufferCopy{0, vDstOffset * sizeof(Vertex), vbSize}
        );
        device->flushCommandBuffer(cmdBuffer);

        ModelInfo modelInfo{
            .id = model.id,
            .model = glm::mat4(1.0f),
        };

        modelInfo.vertex.count =
            static_cast<uint32_t>(model.mesh.vertices.size());
        modelInfo.vertex.startIndex = vDstOffset;

        stagingBuffer.destroy();

        auto &indexBuffer = sceneData.indexBuffer;

        uint32_t iDstOffset = 0;
        if (!sceneData.modelInfos.empty()) {
            auto &iTail = sceneData.modelInfos.back().index;
            iDstOffset = iTail.startIndex + iTail.count;
        }
        void *data = indexBuffer.map();
        std::memcpy(
            static_cast<uint8_t *>(data) + iDstOffset * sizeof(uint16_t),
            model.mesh.indices.data(), ibSize
        );
        indexBuffer.unmap();

        modelInfo.index.count =
            static_cast<uint32_t>(model.mesh.indices.size());
        modelInfo.index.startIndex = iDstOffset;

        sceneData.modelInfos.push_back(modelInfo);
    }
};

class App : public Application {
   public:
    void onInit() override { appName = "Simple Shadow"; }
};

int main() {
    auto app = std::make_unique<App>();
    app->renderer = std::make_unique<ShadowPassRenderer>();
    app->init();
    app->mainLoop();
    app->destroy();

    return 0;
}