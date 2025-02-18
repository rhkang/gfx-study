#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "gfx/vulkan/Resource.hpp"
#include "gfx/vulkan/Utils.hpp"
#include "gfx/vulkan/Device.hpp"

namespace Engine
{
	void Buffer::allocate(vk::DeviceSize size, vk::BufferUsageFlags usage, bool isPersistentMap)
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

		vkCheckResult(vmaCreateBuffer(device->getAllocator(), &bufferCI, &allocCI, &buffer, &allocation, &allocationInfo));
	}

	void* Buffer::map()
	{
		void* data;
		vmaMapMemory(device->getAllocator(), allocation, &data);
		return data;
	}

	void Buffer::unmap()
	{
		vmaUnmapMemory(device->getAllocator(), allocation);
	}

	void Buffer::destroy()
	{
		vmaDestroyBuffer(device->getAllocator(), buffer, allocation);
	}

	void Texture::loadFromFile(const char* filename)
	{
		std::string fullPath = RESOURCE_DIR + std::string(filename);

		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(fullPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pixels)
		{
			throw std::runtime_error("Failed to load texture image!");
		}

		vk::DeviceSize imageSize = texWidth * texHeight * 4;
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		auto stagingBuffer = device->createBuffer();
		stagingBuffer.allocate(imageSize, vk::BufferUsageFlagBits::eTransferSrc, true);
		std::memcpy(stagingBuffer.allocationInfo.pMappedData, pixels, static_cast<size_t>(imageSize));

		stbi_image_free(pixels);

		allocate({ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight) }, mipLevels, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor);
		createSampler();

		auto cmdBuffer = device->allocateCommandBuffer();
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
			{ vk::BufferImageCopy{
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
			} });

		device->flushCommandBuffer(cmdBuffer);
		stagingBuffer.destroy();

		// Generate Mipmaps
		cmdBuffer = device->allocateCommandBuffer();
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

		device->flushCommandBuffer(cmdBuffer);
	}

	void Texture::allocate(vk::Extent2D extent, uint32_t mipLevels, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags)
	{
		auto queueFamilyIndex = device->getQueueFamilyIndex();
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
			.pQueueFamilyIndices = &queueFamilyIndex,
			.initialLayout = vk::ImageLayout::eUndefined,
		};

		VmaAllocationCreateInfo allocCI{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};

		vkCheckResult(vmaCreateImage(device->getAllocator(), &imageCI, &allocCI, &image, &allocation, &allocationInfo));

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

		imageView = device->getLogicalDevice().createImageView(imageViewCI);
	}

	void Texture::createSampler()
	{
		auto properties = device->getPhysicalDevice().getProperties();

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

		sampler = device->getLogicalDevice().createSampler(samplerCI);
	}

	void Texture::destroy()
	{
		if (sampler != VK_NULL_HANDLE)
		{
			device->getLogicalDevice().destroySampler(sampler);
		}
		device->getLogicalDevice().destroyImageView(imageView);
		vmaDestroyImage(device->getAllocator(), image, allocation);
	}
}