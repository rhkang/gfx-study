#include "gfx/vulkan/UiLayout.hpp"
#include "gfx/vulkan/Utils.hpp"
#include "gfx/vulkan/Device.hpp"

#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

namespace Engine
{
	UiLayout::UiLayout(Device* device)
	{
		this->device = device;
	}

	UiLayout::~UiLayout()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		device->getLogicalDevice().destroyDescriptorPool(descriptorPool);
	}

	void UiLayout::init()
	{
		std::vector<vk::DescriptorPoolSize> poolSizes = {
			{vk::DescriptorType::eCombinedImageSampler, 20},
		};

		uint32_t maxSets = 0;
		for (auto& poolSize : poolSizes)
		{
			maxSets += poolSize.descriptorCount;
		}

		vk::DescriptorPoolCreateInfo poolInfo = {
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = maxSets,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};
		descriptorPool = device->getLogicalDevice().createDescriptorPool(poolInfo);

		initImGui();
	}

	void UiLayout::initImGui()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		io.Fonts->AddFontFromFileTTF((std::string(RESOURCE_DIR) + "/fonts/" + fontName).c_str(), fontScale);

		vk::PipelineRenderingCreateInfo prci{
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &colorFormat,
			.depthAttachmentFormat = depthStencilFormat,
			.stencilAttachmentFormat = depthStencilFormat,
		};

		GLFWwindow* window = (GLFWwindow*)device->getWindowHandle();
		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = device->getInstance();
		init_info.PhysicalDevice = device->getPhysicalDevice();
		init_info.Device = device->getLogicalDevice();
		init_info.QueueFamily = device->getQueueFamilyIndex();
		init_info.Queue = device->getQueue();
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = descriptorPool;
		init_info.Allocator = nullptr;
		init_info.MinImageCount = minImageCount;
		init_info.ImageCount = imageCount;
		init_info.CheckVkResultFn = vkCheckResult;
		init_info.UseDynamicRendering = true;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.PipelineRenderingCreateInfo = prci;
		ImGui_ImplVulkan_Init(&init_info);
	}

	void UiLayout::prepareFrame()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void UiLayout::draw(vk::CommandBuffer& cmdBuffer)
	{
		showMetrics();
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
	}

	void UiLayout::showMetrics()
	{
		static bool open = true;
		static bool showExtraDebug = true;
		static float fpsHistory[100] = {};
		static int fpsIndex = 0;
		static float renderTimeHistory[100] = {};
		static int renderTimeIndex = 0;

		if (ImGui::IsKeyPressed(ImGuiKey_F1))
		{
			open = !open;
		}

		if (!open)
			return;

		int boxWidth = fontScale * 15;
		int boxHeight = fontScale * 25;
		ImGui::SetNextWindowSize(ImVec2(boxWidth, boxHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(fontScale, fontScale), ImGuiCond_FirstUseEver);

		ImGui::SetNextWindowSizeConstraints(ImVec2(boxWidth, boxHeight), ImVec2(boxWidth, fontScale * 1000));

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove;

		if (ImGui::Begin("Debug Info", &open, flags))
		{
			static float fps = 0.0f;
			static float lastTime = 0.0f;
			float elapsedTime = ImGui::GetTime();
			float deltaTime = elapsedTime - lastTime;
			lastTime = elapsedTime;

			if (deltaTime > 0.0f)
				fps = 1.0f / deltaTime;

			fpsHistory[fpsIndex] = fps;
			fpsIndex = (fpsIndex + 1) % IM_ARRAYSIZE(fpsHistory);

			renderTimeHistory[renderTimeIndex] = renderTime;
			renderTimeIndex = (renderTimeIndex + 1) % IM_ARRAYSIZE(renderTimeHistory);

			ImVec4 fpsColor = (fps >= 55.0f) ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)	 // Green
				: (fps >= 30.0f) ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f)	 // Yellow
				: ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red (Below 30 FPS)

			ImGui::Text("FPS: ");
			ImGui::SameLine();
			ImGui::TextColored(fpsColor, "%.1f", fps);

			ImGui::Separator();

			ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);
			ImGui::Text("Application Time: %.2f sec", elapsedTime);
			ImGui::Text("Render Time: %.3f ms", renderTime);
			ImGui::Separator();

			static int frameTimePlotMax = 70;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
			ImGui::Text("FPS History");
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
			ImGui::PlotLines("##FPS History", fpsHistory, IM_ARRAYSIZE(fpsHistory),
				fpsIndex, nullptr, 0.0f, frameTimePlotMax, ImVec2(0, 50));
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
			ImGui::SliderInt("##Fps Plot Scale", &frameTimePlotMax, 1, 120);

			static float renderTimePlotMax = 5.0f;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
			ImGui::Text("Render Time History");
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
			ImGui::PlotLines("##Render Time History", renderTimeHistory, IM_ARRAYSIZE(renderTimeHistory),
				renderTimeIndex, nullptr, 0.0f, renderTimePlotMax, ImVec2(0, 50));
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
			ImGui::SliderFloat("##Render Time Plot Scale", &renderTimePlotMax, 0.001f, 17.0f);

			ImGui::Checkbox("Show Extra Debug", &showExtraDebug);
			if (showExtraDebug)
			{
				ImGui::Text("Mouse Position: (%.1f, %.1f)", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
				ImGui::Text("Window Size: (%.0f, %.0f)", ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
				ImGui::Text("Delta Time: %.6f", deltaTime);
			}
		}
		ImGui::End();
	}
}
