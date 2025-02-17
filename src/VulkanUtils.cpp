#include "VulkanUtils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void VulkanBuffer::allocate(vk::DeviceSize size, vk::BufferUsageFlags usage, bool isPersistentMap)
{
	VkBufferCreateInfo bufferCI = vk::BufferCreateInfo{
		.size = size,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	VmaAllocationCreateInfo allocCI{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (isPersistentMap)
	{
		allocCI.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}

	vkCheckResult(vmaCreateBuffer(vulkanDevice->allocator, &bufferCI, &allocCI, &buffer, &allocation, &allocationInfo));
}

void VulkanBuffer::destroy()
{
	vmaDestroyBuffer(vulkanDevice->allocator, buffer, allocation);
}

VulkanSwapchain::~VulkanSwapchain()
{
	for (auto imageView : imageViews)
	{
		device.destroyImageView(imageView);
	}
	device.destroySwapchainKHR(swapchain);
}

void VulkanSwapchain::create(vk::Device device, vk::SurfaceKHR surface, uint32_t queueFamilyIndex)
{
	this->device = device;
	this->surface = surface;

	createInfo = {
		.surface = surface,
		.minImageCount = imageCount,
		.imageFormat = format,
		.imageColorSpace = colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &queueFamilyIndex,
		.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = presentMode,
		.clipped = vk::True,
	};

	swapchain = device.createSwapchainKHR(createInfo);

	images = device.getSwapchainImagesKHR(swapchain);
	for (auto &image : images)
	{
		auto imageView = device.createImageView({
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components = {
				.r = vk::ComponentSwizzle::eIdentity,
				.g = vk::ComponentSwizzle::eIdentity,
				.b = vk::ComponentSwizzle::eIdentity,
				.a = vk::ComponentSwizzle::eIdentity,
			},
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		});
		imageViews.push_back(imageView);
	}
}

void VulkanSwapchain::recreate(vk::Extent2D extent)
{
	device.waitIdle();
	device.destroySwapchainKHR(swapchain);
	for (auto imageView : imageViews)
	{
		device.destroyImageView(imageView);
	}
	imageViews.clear();

	this->extent = extent;
	create(device, surface, queueFamilyIndex);
}

void VulkanSwapchain::recreate()
{
	device.waitIdle();
	device.destroySwapchainKHR(swapchain);
	for (auto imageView : imageViews)
	{
		device.destroyImageView(imageView);
	}
	imageViews.clear();

	create(device, surface, queueFamilyIndex);
}

uint32_t VulkanSwapchain::acquireNextImage(vk::Semaphore semaphore)
{
	uint32_t imageIndex;
	auto result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return UINT32_MAX;
	}

	return imageIndex;
}

VulkanPipelineBuilder::VulkanPipelineBuilder(vk::Device device)
{
	this->device = device;

	inputAssemblyCI.topology = vk::PrimitiveTopology::eTriangleList;

	viewportCI.viewportCount = 1;
	viewportCI.scissorCount = 1;

	rasterizationCI.polygonMode = vk::PolygonMode::eFill;
	rasterizationCI.cullMode = vk::CullModeFlagBits::eBack;
	rasterizationCI.frontFace = vk::FrontFace::eClockwise;
	rasterizationCI.lineWidth = 1.0f;

	depthStencilCI.depthTestEnable = VK_TRUE;
	depthStencilCI.depthWriteEnable = VK_TRUE;
	depthStencilCI.depthCompareOp = vk::CompareOp::eLess;
	depthStencilCI.depthBoundsTestEnable = VK_FALSE;
	depthStencilCI.stencilTestEnable = VK_FALSE;

	multisampleCI.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampleCI.sampleShadingEnable = vk::True;
	multisampleCI.minSampleShading = 1.0f;

	dynamicStates.push_back(vk::DynamicState::eViewport);
	dynamicStates.push_back(vk::DynamicState::eScissor);
	dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCI.pDynamicStates = dynamicStates.data();
}

vk::Pipeline VulkanPipelineBuilder::build()
{
	colorBlendCI.attachmentCount = static_cast<uint32_t>(colorBlendAttachmentStates.size());
	colorBlendCI.pAttachments = colorBlendAttachmentStates.data();

	vk::PipelineRenderingCreateInfo pipelineRenderingCI{
		.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size()),
		.pColorAttachmentFormats = colorAttachmentFormats.data(),
	};

	if (depthAttachmentFormat != vk::Format::eUndefined)
	{
		pipelineRenderingCI.depthAttachmentFormat = depthAttachmentFormat;
	}
	if (stencilAttachmentFormat != vk::Format::eUndefined)
	{
		pipelineRenderingCI.stencilAttachmentFormat = stencilAttachmentFormat;
	}

	vk::GraphicsPipelineCreateInfo pipelineCI{
		.pNext = &pipelineRenderingCI,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputCI,
		.pInputAssemblyState = &inputAssemblyCI,
		.pTessellationState = VK_NULL_HANDLE,
		.pViewportState = &viewportCI,
		.pRasterizationState = &rasterizationCI,
		.pMultisampleState = &multisampleCI,
		.pDepthStencilState = &depthStencilCI,
		.pColorBlendState = &colorBlendCI,
		.pDynamicState = &dynamicStateCI,
		.layout = layout,
		.renderPass = VK_NULL_HANDLE,
		.subpass = 0,
	};

	auto resultValue = device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineCI);
	return resultValue.value;
}

void VulkanPipelineBuilder::addColorAttachment(vk::Format format)
{
	colorAttachmentFormats.push_back(format);

	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{
		.blendEnable = VK_FALSE,
		.colorWriteMask =
			vk::ColorComponentFlagBits::eR |
			vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB |
			vk::ColorComponentFlagBits::eA,
	};
	colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
}

void VulkanPipelineBuilder::addColorAttachment(vk::Format format, vk::PipelineColorBlendAttachmentState colorBlendState)
{
	colorAttachmentFormats.push_back(format);
	colorBlendAttachmentStates.push_back(colorBlendState);
}

void VulkanTexture::loadFromFile(const char *filename)
{
	std::string fullPath = RESOURCE_DIR + std::string(filename);

	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load(fullPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels)
	{
		throw std::runtime_error("Failed to load texture image!");
	}

	vk::DeviceSize imageSize = texWidth * texHeight * 4;
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	auto stagingBuffer = vulkanDevice->createBuffer();
	stagingBuffer.allocate(imageSize, vk::BufferUsageFlagBits::eTransferSrc, true);
	std::memcpy(stagingBuffer.allocationInfo.pMappedData, pixels, static_cast<size_t>(imageSize));

	stbi_image_free(pixels);

	allocate({static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight)}, mipLevels, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor);
	createSampler();

	auto cmdBuffer = vulkanDevice->allocateCommandBuffer();
	imageLayoutTransition(cmdBuffer, vk::ImageAspectFlagBits::eColor,
						  vk::PipelineStageFlagBits::eTopOfPipe,
						  vk::PipelineStageFlagBits::eTransfer,
						  vk::AccessFlagBits::eNone,
						  vk::AccessFlagBits::eTransferWrite,
						  vk::ImageLayout::eUndefined,
						  vk::ImageLayout::eTransferDstOptimal,
						  image, mipLevels);

	cmdBuffer.copyBufferToImage(stagingBuffer.buffer,
								image,
								vk::ImageLayout::eTransferDstOptimal,
								{vk::BufferImageCopy{
									.bufferOffset = 0,
									.bufferRowLength = 0,
									.bufferImageHeight = 0,
									.imageSubresource = {
										.aspectMask = vk::ImageAspectFlagBits::eColor,
										.mipLevel = 0,
										.baseArrayLayer = 0,
										.layerCount = 1,
									},
									.imageOffset = {0, 0, 0},
									.imageExtent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
								}});

	vulkanDevice->flushCommandBuffer(cmdBuffer);
	stagingBuffer.destroy();

	// Generate Mipmaps
	cmdBuffer = vulkanDevice->allocateCommandBuffer();
	vk::ImageMemoryBarrier barrier{
		.oldLayout = vk::ImageLayout::eTransferDstOptimal,
		.newLayout = vk::ImageLayout::eTransferSrcOptimal,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++)
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eTransfer,
			{},
			nullptr,
			nullptr,
			barrier);

		vk::ImageBlit blit{
			.srcSubresource = {vk::ImageAspectFlagBits::eColor, i - 1, 0, 1},
			.srcOffsets = std::array<vk::Offset3D, 2>{
				vk::Offset3D{ 0, 0, 0 },
				vk::Offset3D{ mipWidth, mipHeight, 1 },
			},
			.dstSubresource = {vk::ImageAspectFlagBits::eColor, i, 0, 1},
			.dstOffsets = std::array<vk::Offset3D, 2>{
				vk::Offset3D{0, 0, 0},
				vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1},
			},
		};

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;

		cmdBuffer.blitImage(
			image, vk::ImageLayout::eTransferSrcOptimal,
			image, vk::ImageLayout::eTransferDstOptimal,
			blit,
			vk::Filter::eLinear);

		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			nullptr,
			nullptr,
			barrier);
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	cmdBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eFragmentShader,
		{},
		nullptr,
		nullptr,
		barrier);

	vulkanDevice->flushCommandBuffer(cmdBuffer);
}

void VulkanTexture::allocate(vk::Extent2D extent, uint32_t mipLevels, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags)
{
	VkImageCreateInfo imageCI = vk::ImageCreateInfo{
		.imageType = vk::ImageType::e2D,
		.format = format,
		.extent = {extent.width, extent.height, 1},
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = sampleCount,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &vulkanDevice->queueFamilyIndex,
		.initialLayout = vk::ImageLayout::eUndefined,
	};

	VmaAllocationCreateInfo allocCI{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	vkCheckResult(vmaCreateImage(vulkanDevice->allocator, &imageCI, &allocCI, &image, &allocation, &allocationInfo));

	auto imageViewCI = vk::ImageViewCreateInfo{
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	imageView = vulkanDevice->device.createImageView(imageViewCI);
}

void VulkanTexture::createSampler()
{
	auto properties = vulkanDevice->physicalDevice.getProperties();

	vk::SamplerCreateInfo samplerCI{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = addressMode,
		.addressModeV = addressMode,
		.addressModeW = addressMode,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::True,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = std::clamp(minLod, 0.0f, (float)mipLevels),
		.maxLod = VK_LOD_CLAMP_NONE,
		.borderColor = vk::BorderColor::eFloatOpaqueWhite,
		.unnormalizedCoordinates = vk::False,
	};

	sampler = vulkanDevice->device.createSampler(samplerCI);
}

void VulkanTexture::destroy()
{
	vulkanDevice->device.destroySampler(sampler);
	vulkanDevice->device.destroyImageView(imageView);
	vmaDestroyImage(vulkanDevice->allocator, image, allocation);
}

void imageLayoutTransition(vk::CommandBuffer cmdBuffer, vk::ImageAspectFlags imageAspectFlags, vk::PipelineStageFlagBits srcStageMask, vk::PipelineStageFlagBits dstStageMask, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::Image image, uint32_t mipLevels)
{
	vk::ImageMemoryBarrier imageBarrier{
		.srcAccessMask = srcAccessMask,
		.dstAccessMask = dstAccessMask,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = vk::ImageSubresourceRange{
			imageAspectFlags, 0, mipLevels, 0, 1}};

	cmdBuffer.pipelineBarrier(
		srcStageMask,
		dstStageMask,
		{},
		nullptr,
		nullptr,
		imageBarrier);
}

VulkanBuffer VulkanDevice::createBuffer()
{
	VulkanBuffer buffer{};
	buffer.vulkanDevice = this;
	return buffer;
}

VulkanTexture VulkanDevice::createTexture()
{
	VulkanTexture texture{};
	texture.vulkanDevice = this;
	return texture;
}

vk::CommandBuffer VulkanDevice::allocateCommandBuffer(bool begin)
{
	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers({
																	.commandPool = cmdPool,
																	.level = vk::CommandBufferLevel::ePrimary,
																	.commandBufferCount = 1,
																})
									  .front();

	if (begin)
	{
		cmdBuffer.begin({vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}});
	}

	return cmdBuffer;
}

void VulkanDevice::flushCommandBuffer(vk::CommandBuffer cmdBuffer)
{
	cmdBuffer.end();

	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &cmdBuffer,
	};

	queue.submit(submitInfo);
	queue.waitIdle();

	device.freeCommandBuffers(cmdPool, cmdBuffer);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
												vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
												vk::DebugUtilsMessengerCallbackDataEXT const *pCallbackData,
												void * /*pUserData*/)
{
	if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
	{
		LOG_ERROR(pCallbackData->pMessage);
	}
	else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		LOG_WARN(pCallbackData->pMessage);
	}
	else
	{
		LOG(pCallbackData->pMessage);
	}

	return VK_FALSE;
}