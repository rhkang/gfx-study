#include "core/Core.hpp"

#include "gfx/vulkan/Renderer.hpp"
#include "gfx/vulkan/Resource.hpp"
#include "gfx/vulkan/Device.hpp"
#include "gfx/vulkan/Pipeline.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <imgui.h>

using namespace Engine;

class ModelRenderer : public Renderer
{
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
    Texture msaaTexture;

	float rotation = 0.0f;
    float minLod = 0.0f;
	bool needRecreateSampler = false;

	bool msaaResetFlag = false;
    int sampleValueIndex = 0;
    int maxSampleValueIndex = 3;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec4 color;

        static vk::VertexInputBindingDescription getBindingDescription()
        {
            return vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
        }

        static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
        {
            return {
                vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)),
                vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, color)),
            };
        }
    };

    struct UBO
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    } ubo;

    void onPrepare() override
    {
		sampleValueIndex = std::floor(std::log2((double)msaaSamples));
        maxSampleValueIndex = sampleValueIndex;

		depthTexture = device->createTexture();
		depthTexture.sampleCount = msaaSamples;
		depthTexture.allocate(swapchain->getExtent(), 1, vk::Format::eD32SfloatS8Uint, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth);
		msaaTexture = device->createTexture();
		msaaTexture.sampleCount = msaaSamples;
		msaaTexture.allocate(swapchain->getExtent(), 1, swapchain->format, vk::ImageUsageFlagBits::eColorAttachment, vk::ImageAspectFlagBits::eColor);
        msaaTexture.createSampler();

        prepareData();
        prepareTexture();
        prepareUBO();
        buildPipeline();
    }

	void onResize() override
	{
		depthTexture.destroy();
		depthTexture.allocate(swapchain->getExtent(), 1, vk::Format::eD32SfloatS8Uint, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth);
		msaaTexture.destroy();
		msaaTexture.allocate(swapchain->getExtent(), 1, swapchain->format, vk::ImageUsageFlagBits::eColorAttachment, vk::ImageAspectFlagBits::eColor);
		msaaTexture.createSampler();
	}

    void onUpdate() override
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double, std::milli>(currentTime - startTime).count();

        ubo.model = glm::rotate(glm::mat4(1.0f), (float)rotation / 90 * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapchain->getExtent().width / (float)swapchain->getExtent().height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        std::memcpy(uniformBuffer.allocationInfo.pMappedData, &ubo, sizeof(UBO));

		auto logicalDevice = device->getLogicalDevice();
        if (needRecreateSampler)
        {
            logicalDevice.waitIdle();
            logicalDevice.destroySampler(texture.sampler);
            texture.minLod = minLod;
			texture.createSampler();
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
			needRecreateSampler = false;
        }

        if (msaaResetFlag)
        {
            logicalDevice.waitIdle();

            swapchain->recreate();
            depthTexture.destroy();
            msaaTexture.destroy();

			depthTexture.sampleCount = msaaSamples;
			depthTexture.allocate(swapchain->getExtent(), 1, vk::Format::eD32SfloatS8Uint, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth);
			msaaTexture.sampleCount = msaaSamples;
			msaaTexture.allocate(swapchain->getExtent(), 1, swapchain->format, vk::ImageUsageFlagBits::eColorAttachment, vk::ImageAspectFlagBits::eColor);
            msaaTexture.createSampler();

            logicalDevice.destroyPipeline(pipeline);
            logicalDevice.destroyPipelineLayout(pipelineLayout);

            buildPipeline();
			msaaResetFlag = false;
        }
    }

    void onDestroy() override
    {
		uniformBuffer.destroy();
        vertexBuffer.destroy();
		indexBuffer.destroy();
		texture.destroy();
		depthTexture.destroy();
		msaaTexture.destroy();

		auto logicalDevice = device->getLogicalDevice();
		logicalDevice.destroyDescriptorSetLayout(uboLayout);
		logicalDevice.destroyDescriptorSetLayout(textureLayout);
		logicalDevice.destroyPipeline(pipeline);
		logicalDevice.destroyPipelineLayout(pipelineLayout);
    }

    void draw() override
    {
        auto &cmdBuffer = getCurrentDrawCmdBuffer();

        vk::RenderingAttachmentInfo depthAttachmentInfo{
                .imageView = depthTexture.imageView,
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .depthStencil = {
                        .depth = 1.0f,
                        .stencil = 0,
                    }
                },
        };

        vk::RenderingInfo renderingInfo{
                .renderArea = vk::Rect2D{
                    .offset = vk::Offset2D{0, 0},
                    .extent = swapchain->extent,
                },
                .layerCount = 1,
                .pDepthAttachment = &depthAttachmentInfo,
        };

        vk::RenderingAttachmentInfo renderingAttachmentInfo{
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}},
        };

        bool useMultiSampling = (msaaSamples | vk::SampleCountFlagBits::e1) != vk::SampleCountFlagBits::e1;
        if (useMultiSampling)
        {
            renderingAttachmentInfo.imageView = msaaTexture.imageView;
			renderingAttachmentInfo.resolveMode = vk::ResolveModeFlagBits::eAverage;
			renderingAttachmentInfo.resolveImageView = swapchain->getImageView(imageIndex);
			renderingAttachmentInfo.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }
        else
        {
			renderingAttachmentInfo.imageView = swapchain->getImageView(imageIndex);
        }

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &renderingAttachmentInfo;

        cmdBuffer.beginRendering(renderingInfo);

        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmdBuffer.bindVertexBuffers(0, {vertexBuffer.buffer}, {0});
        cmdBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint16);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, {uboSet, textureSet}, {});

        auto extent = swapchain->getExtent();
        cmdBuffer.setViewport(0, getDefaultViewport(extent));
        cmdBuffer.setScissor(0, getDefaultScissor(extent));

        cmdBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
        cmdBuffer.endRendering();
    }

    void drawUi() override
    {
        auto fontScale = device->getUiLayout()->fontScale;
		auto boxWidth = fontScale * 12;
		auto boxHeight = fontScale * 10;

		ImGui::SetNextWindowSize(ImVec2(boxWidth, boxHeight), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - boxWidth - 10, 10), ImGuiCond_Always);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		ImGui::Begin("Control", nullptr, flags);
		ImGui::Text("Model Rotate");
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
		ImGui::SliderFloat("##", &rotation, 0.0f, 360.0f);

        ImGui::Separator();

		ImGui::Text("Min LOD");
		ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
        if (ImGui::SliderFloat("##minlod", &minLod, 0.0f, 12.0f))
        {
			needRecreateSampler = true;
        }

        ImGui::Separator();

		ImGui::Text("MSAA Samples"); 
		ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
		const char* sampleCountOptions[] = { "1", "2", "4", "8", "16", "32", "64"};
        if (ImGui::Combo("##msaa", &sampleValueIndex, sampleCountOptions, IM_ARRAYSIZE(sampleCountOptions)))
        {
			if (sampleValueIndex <= maxSampleValueIndex)
			{
			    msaaSamples = (vk::SampleCountFlagBits)(1 << sampleValueIndex);
			    msaaResetFlag = true;
			}
            else
            {
				sampleValueIndex = maxSampleValueIndex;
            }
        }

		ImGui::Separator();

		ImGui::End();
    }

    void prepareUBO()
    {
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

        uboSet = logicalDevice.allocateDescriptorSets({
                                                   .descriptorPool = device->getDescriptorPool(),
                                                   .descriptorSetCount = 1,
                                                   .pSetLayouts = &uboLayout,
                                               })
                     .front();

        uniformBuffer = device->createBuffer();
        uniformBuffer.allocate(sizeof(UBO), vk::BufferUsageFlagBits::eUniformBuffer, true);

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

    void prepareTexture()
    {
        auto logicalDevice = device->getLogicalDevice();

        texture = device->createTexture();
        texture.loadFromFile("viking_room.png");

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

        textureSet = logicalDevice.allocateDescriptorSets({
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

    void prepareData()
    {
        auto importer = Assimp::Importer();
        const aiScene* scene = importer.ReadFile(RESOURCE_DIR + std::string("viking_room.obj"), aiProcess_Triangulate);

        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;

        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[i];
            for (unsigned int j = 0; j < mesh->mNumVertices; j++)
            {
                Vertex vertex;
                vertex.pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
                vertex.uv = mesh->mTextureCoords[0] ? glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y) : glm::vec2(0.0f, 0.0f);
                vertex.color = mesh->mColors[0] ? glm::vec4(mesh->mColors[0][j].r, mesh->mColors[0][j].g, mesh->mColors[0][j].b, mesh->mColors[0][j].a) : glm::vec4(1.0f);
				vertex.uv.y = 1.0f - vertex.uv.y;   // obj format assume coord 0 is the bottom
                vertices.push_back(vertex);
            }

            for (unsigned int j = 0; j < mesh->mNumFaces; j++)
            {
                aiFace face = mesh->mFaces[j];
                for (unsigned int k = 0; k < face.mNumIndices; k++)
                {
                    indices.push_back(face.mIndices[k]);
                }
            }
        }

        auto vbSize = sizeof(Vertex) * vertices.size();
        auto ibSize = sizeof(uint16_t) * indices.size();

        vertexBuffer = device->createBuffer();
        vertexBuffer.allocate(vbSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

        auto stagingBuffer = device->createBuffer();
        stagingBuffer.allocate(vbSize, vk::BufferUsageFlagBits::eTransferSrc, true);
        std::memcpy(stagingBuffer.allocationInfo.pMappedData, vertices.data(), vbSize);

        auto cmdBuffer = device->allocateCommandBuffer();
        cmdBuffer.copyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, {vk::BufferCopy{0, 0, vbSize}});
        device->flushCommandBuffer(cmdBuffer);

        stagingBuffer.destroy();

        indexBuffer = device->createBuffer();
        indexBuffer.allocate(sizeof(uint16_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);
        indexCount = static_cast<uint32_t>(indices.size());

        void* data = indexBuffer.map();
        std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());
        indexBuffer.unmap();
    }

    void buildPipeline()
    {
		auto logicalDevice = device->getLogicalDevice();
        PipelineBuilder pipelineBuilder(logicalDevice);

        auto vertShader = device->createShaderModule("texture.vert.spv");
        auto fragShader = device->createShaderModule("texture.frag.spv");

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
        pipelineBuilder.vertexInputCI.pVertexBindingDescriptions = &bindingDescription;
        pipelineBuilder.vertexInputCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        pipelineBuilder.vertexInputCI.pVertexAttributeDescriptions = attributeDescriptions.data();

        pipelineBuilder.rasterizationCI.frontFace = vk::FrontFace::eCounterClockwise;

		pipelineBuilder.multisampleCI.rasterizationSamples = msaaSamples;

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

class App : public Application
{
public:
    void onInit() override
    {
        appName = "Model";
    }
};

int main()
{
    auto app = std::make_unique<App>();
	app->renderer = std::make_unique<ModelRenderer>();
    app->init();
    app->mainLoop();
    app->destroy();

    return 0;
}