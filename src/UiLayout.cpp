#include "UiLayout.hpp"
#include <imgui_impl_vulkan.cpp>

UiLayout::UiLayout(VulkanDevice* vulkanDevice, SDL_Window* window)
{
	this->vulkanDevice = vulkanDevice;
	this->window = window;
}

UiLayout::~UiLayout()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	vulkanDevice->device.destroyDescriptorPool(descriptorPool);
}

void UiLayout::init()
{
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		{ vk::DescriptorType::eCombinedImageSampler, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE }
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
	descriptorPool = vulkanDevice->device.createDescriptorPool(poolInfo);

	initImGui();
}

void UiLayout::initImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	vk::PipelineRenderingCreateInfo prci{
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &colorFormat,
		.depthAttachmentFormat = depthStencilFormat,
		.stencilAttachmentFormat = depthStencilFormat,
	};

	ImGui_ImplSDL2_InitForVulkan(window);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = vulkanDevice->instance;
	init_info.PhysicalDevice = vulkanDevice->physicalDevice;
	init_info.Device = vulkanDevice->device;
	init_info.QueueFamily = vulkanDevice->queueFamilyIndex;
	init_info.Queue = vulkanDevice->queue;
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


void UiLayout::handleSDLEvent(const SDL_Event& event)
{
	ImGui_ImplSDL2_ProcessEvent(&event);
}

void UiLayout::prepareFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
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

    if (!open) return;

	int boxWidth = 230;
	int boxHeight = 200;
    ImGui::SetNextWindowSize(ImVec2(boxWidth, boxHeight), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    ImGui::SetNextWindowSizeConstraints(ImVec2(boxWidth, boxHeight), ImVec2(boxWidth, 1000));

    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("FPS & Debug Info", &open, flags))
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

        ImVec4 fpsColor = (fps >= 55.0f) ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green
            : (fps >= 30.0f) ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f)  // Yellow
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
