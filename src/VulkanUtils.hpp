#pragma once

#include "VulkanUsage.hpp"
#include "Log.hpp"

inline void vkCheckResult(VkResult result)
{
	if (result != VK_SUCCESS)
	{
		LOG_ERROR("Fatal : VkResult is {}", vk::to_string(static_cast<vk::Result>(result)));
		assert(result == VK_SUCCESS);
	}
}

struct VulkanBuffer;
struct VulkanTexture;

struct VulkanDevice
{
	vk::Instance instance;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	VmaAllocator allocator;

	uint32_t queueFamilyIndex;
	vk::Queue queue;
	vk::CommandPool cmdPool;

	VulkanBuffer createBuffer();
	VulkanTexture createTexture();

	vk::CommandBuffer allocateCommandBuffer(bool begin = true);
	void flushCommandBuffer(vk::CommandBuffer cmdBuffer);
};

struct VulkanBuffer
{
	VulkanDevice *vulkanDevice;

	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo allocationInfo;

	void allocate(vk::DeviceSize size, vk::BufferUsageFlags usage, bool isPersistentMap = false);
	void destroy();
};

struct VulkanTexture
{
	VulkanDevice *vulkanDevice;

	VkImage image;
	VmaAllocation allocation;
	VmaAllocationInfo allocationInfo;

	vk::ImageView imageView;
	vk::Sampler sampler;

	vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
	vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat;

	uint32_t mipLevels = 1;
	float minLod = 0.0f;

	void loadFromFile(const char *filename);
	void allocate(vk::Extent2D extent, uint32_t mipLevels, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags);
	void createSampler();
	void destroy();
};

class VulkanSwapchain
{
public:
	VulkanSwapchain() = default;
	~VulkanSwapchain();

	void create(vk::Device device, vk::SurfaceKHR surface, uint32_t queueFamilyIndex);
	void recreate(vk::Extent2D extent);
	void recreate();
	vk::Image getImage(uint32_t index) { return images[index]; }
	vk::ImageView getImageView(uint32_t index) { return imageViews[index]; }
	vk::SwapchainKHR getSwapchain() const { return swapchain; }
	vk::Extent2D getExtent() const { return extent; }

	uint32_t acquireNextImage(vk::Semaphore semaphore);

	uint32_t imageCount;
	vk::Format format;
	vk::ColorSpaceKHR colorSpace;
	vk::PresentModeKHR presentMode;
	vk::Extent2D extent;

private:
	vk::Device device;
	uint32_t queueFamilyIndex;
	vk::SurfaceKHR surface;
	vk::SwapchainKHR swapchain;

	vk::SwapchainCreateInfoKHR createInfo;

	std::vector<vk::Image> images;
	std::vector<vk::ImageView> imageViews;
};

class VulkanPipelineBuilder
{
public:
	VulkanPipelineBuilder(vk::Device device);

	vk::Pipeline build();
	inline void setLayout(vk::PipelineLayout layout) { this->layout = layout; }

	std::vector<vk::Format> colorAttachmentFormats;

	vk::Format colorFormat = vk::Format::eB8G8R8A8Srgb;
	vk::Format depthAttachmentFormat = vk::Format::eD32SfloatS8Uint;
	vk::Format stencilAttachmentFormat = vk::Format::eUndefined;

	vk::GraphicsPipelineCreateInfo graphicsPipelineCI{};

	vk::PipelineVertexInputStateCreateInfo vertexInputCI{};
	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
	vk::PipelineViewportStateCreateInfo viewportCI{};
	vk::PipelineRasterizationStateCreateInfo rasterizationCI{};
	vk::PipelineMultisampleStateCreateInfo multisampleCI{};
	vk::PipelineDepthStencilStateCreateInfo depthStencilCI{};
	vk::PipelineColorBlendStateCreateInfo colorBlendCI{};
	vk::PipelineDynamicStateCreateInfo dynamicStateCI{};

	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
	std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachmentStates{};
	std::vector<vk::DynamicState> dynamicStates{};

	void addColorAttachment(vk::Format format);
	void addColorAttachment(vk::Format format, vk::PipelineColorBlendAttachmentState colorBlendState);

private:
	vk::Device device;
	vk::PipelineLayout layout;
};

void imageLayoutTransition(vk::CommandBuffer cmdBuffer, vk::ImageAspectFlags imageAspectFlags, vk::PipelineStageFlagBits srcStageMask, vk::PipelineStageFlagBits dstStageMask, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::Image image, uint32_t mipLevels = 1);

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
												vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
												vk::DebugUtilsMessengerCallbackDataEXT const *pCallbackData,
												void * /*pUserData*/);