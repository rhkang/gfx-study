#include "gfx/vulkan/UiLayout.hpp"
#include "gfx/vulkan/Utils.hpp"
#include "gfx/vulkan/Device.hpp"

#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

namespace Engine {
UiLayout::UiLayout(Device* device) { this->device = device; }

UiLayout::~UiLayout() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    device->getLogicalDevice().destroyDescriptorPool(descriptorPool);
}

void UiLayout::init() {
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eCombinedImageSampler, 20},
    };

    uint32_t maxSets = 0;
    for (auto& poolSize : poolSizes) {
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

void UiLayout::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.Fonts->AddFontFromFileTTF(
        (std::string(RESOURCE_DIR) + "/fonts/" + fontName).c_str(), fontScale
    );

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

void UiLayout::prepareFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UiLayout::draw() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    if (ImGui::Begin(
            "Root", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_MenuBar
        )) {
        ImGuiID dockspaceId = ImGui::GetID("Root");
        ImGui::DockSpace(
            dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None
        );

        showMenuBar();
    }
    ImGui::End();

    if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
        isShowMetrics = !isShowMetrics;
    }

    if (isShowScene) {
        showScene();
    }
    if (isShowMetrics) {
        device->getUiLayout()->showMetrics();
    }
    if (isShowDemo) {
        ImGui::ShowDemoWindow();
    }
}

void UiLayout::record(vk::CommandBuffer& cmdBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

void UiLayout::addOffscreenTextureForImGui(
    VkSampler sampler, VkImageView imageView, VkImageLayout layout
) {
    offscreenInfo.descriptorSet =
        ImGui_ImplVulkan_AddTexture(sampler, imageView, layout);
}

void UiLayout::removeOffscreenTextureForImGui() {
    ImGui_ImplVulkan_RemoveTexture(offscreenInfo.descriptorSet);
}

void UiLayout::showMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem("Scene", nullptr, &isShowScene);
                ImGui::MenuItem("Metrics", "F1", &isShowMetrics);
                ImGui::MenuItem("Demo", nullptr, &isShowDemo);

                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void UiLayout::showScene() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Scene", nullptr, ImGuiConfigFlags_DockingEnable)) {
        offscreenInfo.doRender = true;

        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        uint32_t width = uint32_t(floor(contentSize.x));
        uint32_t height = uint32_t(floor(contentSize.y));
        ImGui::Image(
            (ImTextureID)offscreenInfo.descriptorSet, ImVec2(width, height)
        );

        if (width != offscreenInfo.width || height != offscreenInfo.height) {
            offscreenInfo.width = width;
            offscreenInfo.height = height;
            offscreenInfo.sizeChanged = true;
        }
    } else {
        offscreenInfo.doRender = false;
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}

void UiLayout::showMetrics() {
    static bool showExtraDebug = true;
    static float fpsHistory[100] = {};
    static int fpsIndex = 0;
    static float renderTimeHistory[100] = {};
    static int renderTimeIndex = 0;

    int boxWidth = fontScale * 15;
    int boxHeight = fontScale * 25;
    ImGui::SetNextWindowSize(
        ImVec2(boxWidth, boxHeight), ImGuiCond_FirstUseEver
    );
    ImGui::SetNextWindowPos(
        ImVec2(fontScale, fontScale), ImGuiCond_FirstUseEver
    );

    ImGui::SetNextWindowSizeConstraints(
        ImVec2(boxWidth, boxHeight), ImVec2(boxWidth, fontScale * 1000)
    );

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("Debug Info", nullptr, flags)) {
        static float fps = 0.0f;
        static float lastTime = 0.0f;
        float elapsedTime = ImGui::GetTime();
        float deltaTime = elapsedTime - lastTime;
        lastTime = elapsedTime;

        if (deltaTime > 0.0f) fps = 1.0f / deltaTime;

        fpsHistory[fpsIndex] = fps;
        fpsIndex = (fpsIndex + 1) % IM_ARRAYSIZE(fpsHistory);

        renderTimeHistory[renderTimeIndex] = renderTime;
        renderTimeIndex =
            (renderTimeIndex + 1) % IM_ARRAYSIZE(renderTimeHistory);

        ImVec4 fpsColor =
            (fps >= 55.0f) ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green
            : (fps >= 30.0f)
                ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f)   // Yellow
                : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red (Below 30 FPS)

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
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::PlotLines(
            "##FPS History", fpsHistory, IM_ARRAYSIZE(fpsHistory), fpsIndex,
            nullptr, 0.0f, frameTimePlotMax, ImVec2(0, 50)
        );
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::SliderInt("##Fps Plot Scale", &frameTimePlotMax, 1, 120);

        static float renderTimePlotMax = 5.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
        ImGui::Text("Render Time History");
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::PlotLines(
            "##Render Time History", renderTimeHistory,
            IM_ARRAYSIZE(renderTimeHistory), renderTimeIndex, nullptr, 0.0f,
            renderTimePlotMax, ImVec2(0, 50)
        );
        ImGui::SetNextItemWidth(
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2
        );
        ImGui::SliderFloat(
            "##Render Time Plot Scale", &renderTimePlotMax, 0.001f, 17.0f
        );

        ImGui::Checkbox("Show Extra Debug", &showExtraDebug);
        if (showExtraDebug) {
            ImGui::Text(
                "Mouse Position: (%.1f, %.1f)", ImGui::GetIO().MousePos.x,
                ImGui::GetIO().MousePos.y
            );
            ImGui::Text(
                "Window Size: (%.0f, %.0f)", ImGui::GetIO().DisplaySize.x,
                ImGui::GetIO().DisplaySize.y
            );
            ImGui::Text("Delta Time: %.6f", deltaTime);
        }
    }
    ImGui::End();
}
}  // namespace Engine
