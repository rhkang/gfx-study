#include "Base.hpp"

#include <SDL2/SDL_vulkan.h>

void Application::init()
{
	onInit();
	setupSDLWindow();

	VULKAN_HPP_DEFAULT_DISPATCHER.init();

	vk::ApplicationInfo appInfo{
		.pApplicationName = appName.c_str(),
		.apiVersion = vk::ApiVersion13,
	};

	uint32_t sdlExtensionCount = 0;
	std::vector<const char *> sdlExtensions;
	SDL_Vulkan_GetInstanceExtensions(window.sdlWindow, &sdlExtensionCount, nullptr);
	sdlExtensions.resize(sdlExtensionCount);
	SDL_Vulkan_GetInstanceExtensions(window.sdlWindow, &sdlExtensionCount, sdlExtensions.data());

	instanceExtensions.insert(instanceExtensions.end(), sdlExtensions.begin(), sdlExtensions.end());

	vk::InstanceCreateInfo ici{
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
		.ppEnabledLayerNames = instanceLayers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
		.ppEnabledExtensionNames = instanceExtensions.data(),
	};

	instance = vk::createInstance(ici);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

	vk::DebugUtilsMessengerCreateInfoEXT dumci{
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
		.pfnUserCallback = debugMessageFunc,
	};
	debugMessenger = instance.createDebugUtilsMessengerEXT(dumci);

	auto physicalDevices = instance.enumeratePhysicalDevices();
	assert(!physicalDevices.empty());
	physicalDevice = physicalDevices[0];

	auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
	auto queueFamilyIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](const vk::QueueFamilyProperties &qfp)
																											{ return qfp.queueFlags & vk::QueueFlagBits::eGraphics; })));

	float queuePriority = 1.0f;
	vk::DeviceQueueCreateInfo dqci{
		.queueFamilyIndex = queueFamilyIndex,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};

	auto physicalDevicefeatures = physicalDevice.getFeatures();
	if (!physicalDevicefeatures.samplerAnisotropy)
	{
		LOG_ERROR("Physical device does not support sampler anisotropy");
	}

	auto physicalDeviceProperties = physicalDevice.getProperties();
	vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts | physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & vk::SampleCountFlagBits::e64)
		msaaSamples = vk::SampleCountFlagBits::e64;
	else if (counts & vk::SampleCountFlagBits::e32)
		msaaSamples = vk::SampleCountFlagBits::e32;
	else if (counts & vk::SampleCountFlagBits::e16)
		msaaSamples = vk::SampleCountFlagBits::e16;
	else if (counts & vk::SampleCountFlagBits::e8)
		msaaSamples = vk::SampleCountFlagBits::e8;
	else if (counts & vk::SampleCountFlagBits::e4)
		msaaSamples = vk::SampleCountFlagBits::e4;
	else if (counts & vk::SampleCountFlagBits::e2)
		msaaSamples = vk::SampleCountFlagBits::e2;
	else
		msaaSamples = vk::SampleCountFlagBits::e1;

	vk::PhysicalDeviceDynamicRenderingFeatures drFeatures{
		.dynamicRendering = vk::True,
	};
	vk::PhysicalDeviceFeatures2 features2{
		.pNext = &drFeatures,
		.features = {
			.sampleRateShading = vk::True,
			.samplerAnisotropy = vk::True,
		},
	};

	vk::DeviceCreateInfo dci{
		.pNext = &features2,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &dqci,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = VK_NULL_HANDLE,
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = VK_NULL_HANDLE,
	};

	this->queueFamilyIndex = queueFamilyIndex;
	device = physicalDevice.createDevice(dci);
	queue = device.getQueue(queueFamilyIndex, 0);

	queryPool = device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = 2,
	});
	timestampPeriod = physicalDevice.getProperties().limits.timestampPeriod;

	std::vector<vk::DescriptorPoolSize> poolSizes{
		{
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 10,
		},
		{
			.type = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 10,
		},
	};
	descriptorPool = device.createDescriptorPool({
		.maxSets = 10,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	});

	cmdPool = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queueFamilyIndex,
	});

	semaphores.imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
	semaphores.renderFinished.resize(MAX_FRAMES_IN_FLIGHT);
	fences.inFlight.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		semaphores.imageAvailable[i] = device.createSemaphore({});
		semaphores.renderFinished[i] = device.createSemaphore({});
		fences.inFlight[i] = device.createFence({
			.flags = vk::FenceCreateFlagBits::eSignaled,
		});
	}

	VmaAllocatorCreateInfo allocatorCI{
		.physicalDevice = physicalDevice,
		.device = device,
		.instance = instance,
	};
	vmaCreateAllocator(&allocatorCI, &allocator);

	vulkanDevice = std::make_unique<VulkanDevice>();
	vulkanDevice->instance = instance;
	vulkanDevice->physicalDevice = physicalDevice;
	vulkanDevice->device = device;
	vulkanDevice->allocator = allocator;
	vulkanDevice->queue = queue;
	vulkanDevice->queueFamilyIndex = queueFamilyIndex;
	vulkanDevice->cmdPool = cmdPool;

	uiLayout = std::make_unique<UiLayout>(vulkanDevice.get(), window.sdlWindow);

	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window.sdlWindow, instance, &surface);

	auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
	auto surfacePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

	uint32_t minImageCount = (surfaceCapabilities.minImageCount < 2) ? 2 : surfaceCapabilities.minImageCount;
	assert(surfaceCapabilities.maxImageCount >= 3);

	auto presentMode = vk::PresentModeKHR::eFifo;
	swapchain = std::make_unique<VulkanSwapchain>();
	swapchain->extent = surfaceCapabilities.currentExtent;
	swapchain->imageCount = minImageCount + 1;
	swapchain->format = vk::Format::eB8G8R8A8Srgb;
	swapchain->colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	swapchain->presentMode = presentMode;

	this->surface = surface;
	uiLayout->minImageCount = minImageCount;
	uiLayout->imageCount = swapchain->imageCount;
	uiLayout->colorFormat = swapchain->format;
	uiLayout->init();

	swapchain->create(device, surface, queueFamilyIndex);

	drawCmdBuffers = device.allocateCommandBuffers({
		.commandPool = cmdPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	});
}

void Application::prepare()
{
	onPrepare();

	depthStencilTexture.vulkanDevice = vulkanDevice.get();
	msaaTexture.vulkanDevice = vulkanDevice.get();
	allocateDepthStencilTexture();
	allocateMsaaTexture();
}

void Application::mainLoop()
{
	prepare();

	bool running = true;
	bool minimized = false;
	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			uiLayout->handleSDLEvent(event);
			if (event.type == SDL_QUIT)
			{
				running = false;
			}

			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_RESIZED:
				windowResized = true;
				break;
			case SDL_WINDOWEVENT_MINIMIZED:
				minimized = true;
				break;
			case SDL_WINDOWEVENT_RESTORED:
				minimized = false;
				break;
			}
		}

		update();

		if (!minimized)
		{
			render();
		}
	}
}

void Application::update()
{
	onUpdate();
}

void Application::prepareFrame()
{
	auto result = device.waitForFences(fences.inFlight[currentFrame], vk::True, UINT64_MAX);
	assert(result == vk::Result::eSuccess);

	imageIndex = swapchain->acquireNextImage(semaphores.imageAvailable[currentFrame]);
	if (imageIndex == UINT32_MAX)
	{
		handleWindowResize();
		return;
	}

	device.resetFences(fences.inFlight[currentFrame]);
	uiLayout->prepareFrame();
}

void Application::drawFrame()
{
	auto &cmdBuffer = getCurrentDrawCmdBuffer();
	cmdBuffer.reset();
	cmdBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	cmdBuffer.resetQueryPool(queryPool, 0, 2);
	cmdBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, queryPool, 0);

	imageLayoutTransition(
		cmdBuffer,
		vk::PipelineStageFlagBits::eTopOfPipe,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::AccessFlagBits::eNone,
		vk::AccessFlagBits::eColorAttachmentWrite,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		swapchain->getImage(imageIndex));

	draw();

	vk::RenderingAttachmentInfo finalColorAttachmentInfo{
		.imageView = swapchain->getImageView(imageIndex),
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eLoad,
		.storeOp = vk::AttachmentStoreOp::eStore,
	};

	vk::RenderingInfo uiRenderingInfo{
		.renderArea = vk::Rect2D{
			.offset = vk::Offset2D{0, 0},
			.extent = swapchain->extent,
		},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &finalColorAttachmentInfo,
	};

	cmdBuffer.beginRendering(uiRenderingInfo);
	drawUi();
	uiLayout->draw(cmdBuffer);
	cmdBuffer.endRendering();

	imageLayoutTransition(
		cmdBuffer,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eBottomOfPipe,
		vk::AccessFlagBits::eColorAttachmentWrite,
		vk::AccessFlagBits::eNone,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		swapchain->getImage(imageIndex));

	cmdBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, queryPool, 1);
	cmdBuffer.end();
}

void Application::submitFrame()
{
	vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
	vk::Semaphore waitSemaphores[] = {semaphores.imageAvailable[currentFrame]};
	vk::Semaphore signalSemaphores[] = {semaphores.renderFinished[currentFrame]};

	queue.submit({vk::SubmitInfo{
					 .waitSemaphoreCount = 1,
					 .pWaitSemaphores = waitSemaphores,
					 .pWaitDstStageMask = waitStages,
					 .commandBufferCount = 1,
					 .pCommandBuffers = &drawCmdBuffers[currentFrame],
					 .signalSemaphoreCount = 1,
					 .pSignalSemaphores = signalSemaphores,
				 }},
				 fences.inFlight[currentFrame]);

	vk::SwapchainKHR swapchains[] = {swapchain->getSwapchain()};
	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = reinterpret_cast<VkSemaphore *>(signalSemaphores),
		.swapchainCount = 1,
		.pSwapchains = reinterpret_cast<VkSwapchainKHR *>(swapchains),
		.pImageIndices = &imageIndex,
	};

	auto result = vkQueuePresentKHR(queue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowResized)
	{
		handleWindowResize();
	}

	vk::Result queryResult = device.getQueryPoolResults(queryPool, 0, 2, sizeof(uint64_t) * 2, &timestamps, sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
	if (queryResult == vk::Result::eSuccess)
	{
		uint64_t startTimestamp = timestamps[0];
		uint64_t endTimestamp = timestamps[1];
		double gpuTime = static_cast<double>(endTimestamp - startTimestamp) * timestampPeriod / 1000000.0;
		uiLayout->renderTime = gpuTime;
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::handleWindowResize()
{
	vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	vk::Extent2D extent = surfaceCapabilities.currentExtent;
	if (extent.width == 0 && extent.height == 0)
	{
		return;
	}

	swapchain->recreate(extent);
	depthStencilTexture.destroy();
	allocateDepthStencilTexture();
	msaaTexture.destroy();
	allocateMsaaTexture();

	windowResized = false;
}

void Application::render()
{
	prepareFrame();
	drawFrame();
	submitFrame();
}

void Application::destroy()
{
	device.waitIdle();

	onDestroy();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		device.destroySemaphore(semaphores.imageAvailable[i]);
		device.destroySemaphore(semaphores.renderFinished[i]);
		device.destroyFence(fences.inFlight[i]);
	}

	depthStencilTexture.destroy();
	msaaTexture.destroy();

	uiLayout.reset();
	swapchain.reset();

	device.destroyCommandPool(cmdPool);
	device.destroyDescriptorPool(descriptorPool);
	device.destroyQueryPool(queryPool);

	vmaDestroyAllocator(allocator);

	device.destroy();
	instance.destroySurfaceKHR(surface);
	instance.destroyDebugUtilsMessengerEXT(debugMessenger);
	instance.destroy();

	SDL_DestroyWindow(window.sdlWindow);
	SDL_Quit();
}

void Application::allocateDepthStencilTexture()
{
	depthStencilTexture.sampleCount = msaaSamples;
	depthStencilTexture.allocate(swapchain->extent, 1, vk::Format::eD32SfloatS8Uint, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
	depthStencilTexture.createSampler();

	auto cmdBuffer = vulkanDevice->allocateCommandBuffer();
	imageLayoutTransition(cmdBuffer,
						  vk::PipelineStageFlagBits::eTopOfPipe,
						  vk::PipelineStageFlagBits::eEarlyFragmentTests,
						  vk::AccessFlagBits::eNone,
						  vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
						  vk::ImageLayout::eUndefined,
						  vk::ImageLayout::eDepthStencilAttachmentOptimal,
						  depthStencilTexture.image);
	vulkanDevice->flushCommandBuffer(cmdBuffer);
}

void Application::allocateMsaaTexture()
{
	msaaTexture.sampleCount = msaaSamples;
	msaaTexture.allocate(swapchain->extent, 1, swapchain->format, vk::ImageUsageFlagBits::eColorAttachment, vk::ImageAspectFlagBits::eColor);
	msaaTexture.createSampler();

	auto cmdBuffer = vulkanDevice->allocateCommandBuffer();
	imageLayoutTransition(cmdBuffer,
						  vk::PipelineStageFlagBits::eTopOfPipe,
						  vk::PipelineStageFlagBits::eColorAttachmentOutput,
						  vk::AccessFlagBits::eNone,
						  vk::AccessFlagBits::eColorAttachmentWrite,
						  vk::ImageLayout::eUndefined,
						  vk::ImageLayout::eColorAttachmentOptimal,
						  msaaTexture.image);
	vulkanDevice->flushCommandBuffer(cmdBuffer);
}

void Application::setupSDLWindow()
{
	SDL_Init(SDL_INIT_VIDEO);

#if defined(_WIN32)
	window.w = GetSystemMetrics(SM_CXSCREEN) / 2;
	window.h = GetSystemMetrics(SM_CYSCREEN) / 2;
#endif

	auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
	auto sdlWindow = SDL_CreateWindow(appName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window.w, window.h, flags);
	assert(sdlWindow != nullptr);

	window.sdlWindow = sdlWindow;
}

vk::ShaderModule createShaderModule(vk::Device device, const char *filename)
{
	std::string fullPath = SHADER_DIR + std::string(filename);

	auto fileBinary = readFile(fullPath);

	vk::ShaderModuleCreateInfo shaderModuleCI{
		.codeSize = fileBinary.size(),
		.pCode = reinterpret_cast<uint32_t *>(fileBinary.data()),
	};

	return device.createShaderModule(shaderModuleCI);
}

std::vector<char> readFile(const std::string &filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}