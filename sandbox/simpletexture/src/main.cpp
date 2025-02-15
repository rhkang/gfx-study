#include "Base.hpp"

class App : public Application
{
public:
private:
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	uint32_t indexCount;

	vk::DescriptorSetLayout uboLayout;
	vk::DescriptorSet uboSet;
	VulkanBuffer uniformBuffer;

	VulkanTexture texture;
	vk::DescriptorSetLayout textureLayout;
	vk::DescriptorSet textureSet;

	vk::PipelineLayout pipelineLayout;
	vk::Pipeline pipeline;

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

	void onInit() override
	{
		appName = "Simple Texture";
	}

	void onPrepare() override
	{
		msaaSamples = vk::SampleCountFlagBits::e1;

		prepareData();
		prepareTexture();
		prepareUBO();
		buildPipeline();
	}

	void onUpdate() override
	{
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		double time = std::chrono::duration<double, std::milli>(currentTime - startTime).count();

		ubo.model = glm::rotate(glm::mat4(1.0f), (float)time / 1000 * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), swapchain->getExtent().width / (float)swapchain->getExtent().height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		std::memcpy(uniformBuffer.allocationInfo.pMappedData, &ubo, sizeof(UBO));
	}

	void onDestroy() override
	{
		uniformBuffer.destroy();
		vertexBuffer.destroy();
		indexBuffer.destroy();
		texture.destroy();
		device.destroyDescriptorSetLayout(uboLayout);
		device.destroyDescriptorSetLayout(textureLayout);
		device.destroyPipeline(pipeline);
		device.destroyPipelineLayout(pipelineLayout);
	}

	void draw() override
	{
		auto &cmdBuffer = getCurrentDrawCmdBuffer();

		vk::RenderingAttachmentInfo colorAttachmentInfo{
			.imageView = swapchain->getImageView(imageIndex),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}},
		};

		vk::RenderingAttachmentInfo depthStencilAttachmentInfo{
			.imageView = depthStencilTexture.imageView,
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = vk::ClearValue{
				.depthStencil = {
					.depth = 1.0f,
					.stencil = 0,
				},
			},
		};

		vk::RenderingInfo renderingInfo{
			.renderArea = vk::Rect2D{
				.offset = vk::Offset2D{0, 0},
				.extent = swapchain->getExtent(),
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthStencilAttachmentInfo,
		};

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

	void prepareUBO()
	{
		auto binding = vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex,
		};

		uboLayout = device.createDescriptorSetLayout({
			.bindingCount = 1,
			.pBindings = &binding,
		});

		uboSet = device.allocateDescriptorSets({
												   .descriptorPool = descriptorPool,
												   .descriptorSetCount = 1,
												   .pSetLayouts = &uboLayout,
											   })
					 .front();

		uniformBuffer = vulkanDevice->createBuffer();
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
		device.updateDescriptorSets(descriptorWrite, {});
	}

	void prepareTexture()
	{
		texture = vulkanDevice->createTexture();
		texture.loadFromFile("nikke.jpg");

		auto binding = vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment,
		};

		textureLayout = device.createDescriptorSetLayout({
			.bindingCount = 1,
			.pBindings = &binding,
		});

		textureSet = device.allocateDescriptorSets({
													   .descriptorPool = descriptorPool,
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
		device.updateDescriptorSets(descriptorWrite, {});
	}

	void prepareData()
	{
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
		vertexBuffer = vulkanDevice->createBuffer();
		vertexBuffer.allocate(vbSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

		auto stagingBuffer = vulkanDevice->createBuffer();
		stagingBuffer.allocate(vbSize, vk::BufferUsageFlagBits::eTransferSrc, true);
		std::memcpy(stagingBuffer.allocationInfo.pMappedData, vertices.data(), vbSize);

		auto cmdBuffer = vulkanDevice->allocateCommandBuffer();
		cmdBuffer.copyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, {vk::BufferCopy{0, 0, vbSize}});
		vulkanDevice->flushCommandBuffer(cmdBuffer);

		stagingBuffer.destroy();

		indexBuffer = vulkanDevice->createBuffer();
		indexBuffer.allocate(sizeof(uint16_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);
		indexCount = static_cast<uint32_t>(indices.size());

		void *data;
		vmaMapMemory(allocator, indexBuffer.allocation, &data);
		std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());
		vmaUnmapMemory(allocator, indexBuffer.allocation);
	}

	void buildPipeline()
	{
		VulkanPipelineBuilder pipelineBuilder(device);

		auto vertShader = createShaderModule(device, "texture.vert.spv");
		auto fragShader = createShaderModule(device, "texture.frag.spv");

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

		pipelineBuilder.addColorAttachment(swapchain->format);

		vk::DescriptorSetLayout setLayouts[2] = {uboLayout, textureLayout};
		pipelineLayout = device.createPipelineLayout({
			.setLayoutCount = 2,
			.pSetLayouts = setLayouts,
		});

		pipelineBuilder.setLayout(pipelineLayout);
		pipelineBuilder.shaderStages.push_back(vertexShaderStageCI);
		pipelineBuilder.shaderStages.push_back(fragmentShaderStageCI);
		pipeline = pipelineBuilder.build();

		device.destroyShaderModule(vertShader);
		device.destroyShaderModule(fragShader);
	}
};

int main()
{
	auto app = std::make_unique<App>();
	app->init();
	app->mainLoop();
	app->destroy();

	return 0;
}