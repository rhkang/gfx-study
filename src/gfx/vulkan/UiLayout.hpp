#pragma once

#include "gfx/vulkan/VulkanUsage.hpp"

namespace Engine
{
	class Device;

	class UiLayout
	{
	public:
		UiLayout(Device* device);
		~UiLayout();

		void init();
		void prepareFrame();
		void draw(vk::CommandBuffer& cmdBuffer);

		uint32_t minImageCount = 2;
		uint32_t imageCount = 3;
		vk::Format colorFormat = vk::Format::eB8G8R8A8Srgb;
		vk::Format depthStencilFormat = vk::Format::eD32SfloatS8Uint;
		
		double renderTime = 0.0;
		float fontScale = 24.0f;
	private:
		void showMetrics();
		void initImGui();

		Device* device;

		std::string fontName = "jetbrains_mono_bold.ttf";

		vk::DescriptorPool descriptorPool;
	};
}