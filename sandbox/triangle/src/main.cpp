#include "Base.hpp"

class App : public Application
{
public:
private:
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	uint32_t indexCount;

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

	void onInit() override
	{
		appName = "Simple Triangle";
	}

	void onPrepare() override
	{
		prepareData();
		buildPipeline();
	}

	void onDestroy() override
	{
		vertexBuffer.destroy();
		indexBuffer.destroy();

		device.destroyPipeline(pipeline);
		device.destroyPipelineLayout(pipelineLayout);
	}

	void draw() override
	{
		auto& cmdBuffer = getCurrentDrawCmdBuffer();

		vk::RenderingAttachmentInfo colorAttachmentInfo{
			.imageView = swapchain->getImageView(imageIndex),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}},
		};

		vk::RenderingInfo renderingInfo{
			.renderArea = vk::Rect2D{
				.offset = vk::Offset2D{0, 0},
				.extent = swapchain->extent,
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
		};

		cmdBuffer.beginRendering(renderingInfo);
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		cmdBuffer.bindVertexBuffers(0, { vertexBuffer.buffer }, { 0 });
		cmdBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint16);

		auto extent = swapchain->getExtent();
		cmdBuffer.setViewport(0, getDefaultViewport(extent));
		cmdBuffer.setScissor(0, getDefaultScissor(extent));

		cmdBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
		cmdBuffer.endRendering();
	}

	void prepareData()
	{
		std::vector<Vertex> vertices = {
			{{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f, 1.0f }},
			{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
		};

		std::vector<uint16_t> indices = { 0, 1, 2 };

		auto vbSize = sizeof(Vertex) * vertices.size();
		vertexBuffer = vulkanDevice->createBuffer();
		vertexBuffer.allocate(vbSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

		auto stagingBuffer = vulkanDevice->createBuffer();
		stagingBuffer.allocate(vbSize, vk::BufferUsageFlagBits::eTransferSrc);

		void* data;
		vmaMapMemory(allocator, stagingBuffer.allocation, &data);
		std::memcpy(data, vertices.data(), vbSize);
		vmaUnmapMemory(allocator, stagingBuffer.allocation);

		auto cmdBuffer = vulkanDevice->allocateCommandBuffer();
		cmdBuffer.copyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, { vk::BufferCopy{0, 0, vbSize} });
		vulkanDevice->flushCommandBuffer(cmdBuffer);

		stagingBuffer.destroy();

		indexBuffer = vulkanDevice->createBuffer();
		indexBuffer.allocate(sizeof(uint16_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);
		indexCount = static_cast<uint32_t>(indices.size());

		vmaMapMemory(allocator, indexBuffer.allocation, &data);
		std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());
		vmaUnmapMemory(allocator, indexBuffer.allocation);
	}

	void buildPipeline()
	{
		VulkanPipelineBuilder pipelineBuilder(device);

		auto vertShader = createShaderModule(device, "triangle.vert.spv");
		auto fragShader = createShaderModule(device, "triangle.frag.spv");

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

		pipelineBuilder.addColorAttachment(swapchain->format);

		pipelineLayout = device.createPipelineLayout({
			.setLayoutCount = 0,
			.pSetLayouts = nullptr,
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