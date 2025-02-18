#pragma once

#include "gfx/vulkan/VulkanUsage.hpp"
#include "gfx/vulkan/Resource.hpp"
#include "gfx/vulkan/UiLayout.hpp"

const int MAX_FRAMES_IN_FLIGHT = 2;

namespace Engine
{
	class Buffer;
	class Texture;

	class Device
	{
	public:
		void init(void* window);
		void destroy();

		void waitIdle();

		vk::Instance getInstance() const { return instance; }
		vk::PhysicalDevice getPhysicalDevice() const { return physicalDevice; }
		vk::Device getLogicalDevice() const { return device; }
		VmaAllocator getAllocator() const { return allocator; }
		vk::SurfaceKHR getSurface() const { return surface; }
		vk::Queue getQueue() const { return queue; }
		uint32_t getQueueFamilyIndex() const { return queueFamilyIndex; }
		vk::CommandPool getCommandPool() const { return cmdPool; }
		vk::DescriptorPool getDescriptorPool() const { return descriptorPool; }
		UiLayout* getUiLayout() const { return uiLayout.get(); }

		void* getWindowHandle() const { return windowHandle; }

		vk::CommandBuffer allocateCommandBuffer(bool begin = true);
		void flushCommandBuffer(vk::CommandBuffer cmdBuffer);

		vk::ShaderModule createShaderModule(const char* filename);
		Buffer createBuffer();
		Texture createTexture();
	private:
		vk::Instance instance;
		vk::PhysicalDevice physicalDevice;
		vk::Device device;
		VmaAllocator allocator;
		vk::DescriptorPool descriptorPool;
		vk::CommandPool cmdPool;
		vk::Queue queue;
		uint32_t queueFamilyIndex;

		std::unique_ptr<UiLayout> uiLayout;

		vk::DebugUtilsMessengerEXT debugMessenger;
		vk::SurfaceKHR surface;

		std::vector<const char*> instanceExtensions = {
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_KHR_SURFACE_EXTENSION_NAME,
		};
		std::vector<const char*> instanceLayers = {
			"VK_LAYER_KHRONOS_validation",
		};

		std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		};

		void* windowHandle;
	};

	std::vector<char> readFile(const std::string& filename);
}