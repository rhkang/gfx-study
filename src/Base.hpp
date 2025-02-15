#pragma once

#include <spdlog/spdlog.h>
#include <SDL2/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>

#if defined(_WIN32)
#include <Windows.h>
#endif

#include "VulkanUtils.hpp"
#include "Log.hpp"
#include "UiLayout.hpp"

const int MAX_FRAMES_IN_FLIGHT = 2;

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

class Application
{
public:
	void init();
	void prepare();
	void mainLoop();
	void update();
	void render();
	void destroy();

	// @brief For Application configurations only. Use `onPrepare` to create gfx contexts
	virtual void onInit() {}
	virtual void onPrepare() {}
	virtual void onUpdate() {}
	virtual void onDestroy() {}

	virtual void draw() = 0;
	virtual void drawUi() {}

protected:
	inline vk::CommandBuffer &getCurrentDrawCmdBuffer()
	{
		return drawCmdBuffers[currentFrame];
	}
	inline vk::Viewport getDefaultViewport(vk::Extent2D extent)
	{
		return vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f);
	}
	inline vk::Rect2D getDefaultScissor(vk::Extent2D extent)
	{
		return vk::Rect2D(vk::Offset2D(0, 0), extent);
	}

	void allocateDepthStencilTexture();
	void allocateMsaaTexture();

	std::string appName = "Renderer";

	vk::Instance instance;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	VmaAllocator allocator;

	std::unique_ptr<VulkanDevice> vulkanDevice;
	std::unique_ptr<VulkanSwapchain> swapchain;
	std::unique_ptr<UiLayout> uiLayout;

	VulkanTexture depthStencilTexture;
	VulkanTexture msaaTexture;

	vk::DescriptorPool descriptorPool;
	vk::CommandPool cmdPool;
	vk::Queue queue;

	uint32_t imageIndex;
	vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;
private:
	void setupSDLWindow();

	void prepareFrame();
	void drawFrame();
	void submitFrame();

	bool windowResized = false;
	void handleWindowResize();

	vk::DebugUtilsMessengerEXT debugMessenger;
	vk::SurfaceKHR surface;
	vk::QueryPool queryPool;
	uint64_t timestamps[2];
	uint64_t timestampPeriod;

	uint32_t queueFamilyIndex;
	std::vector<vk::CommandBuffer> drawCmdBuffers;

	std::chrono::high_resolution_clock::time_point renderStartTime;

	struct
	{
		std::vector<vk::Semaphore> imageAvailable;
		std::vector<vk::Semaphore> renderFinished;
	} semaphores;

	struct
	{
		std::vector<vk::Fence> inFlight;
	} fences;

	uint32_t currentFrame = 0;

	std::vector<const char *> instanceExtensions = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
	};
	std::vector<const char *> instanceLayers = {
		"VK_LAYER_KHRONOS_validation",
	};

	std::vector<const char *> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	};

	struct
	{
		int w = 1920;
		int h = 1080;

		SDL_Window *sdlWindow;
	} window;
};

vk::ShaderModule createShaderModule(vk::Device device, const char *filename);
std::vector<char> readFile(const std::string &filename);