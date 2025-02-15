#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl2.h>

#include "VulkanUtils.hpp"

class UiLayout
{
public:
    explicit UiLayout(VulkanDevice* vulkanDevice, SDL_Window* window);
	~UiLayout();

    void init();
	void handleSDLEvent(const SDL_Event& event);
    void prepareFrame();
    void draw(vk::CommandBuffer& cmdBuffer);

	uint32_t minImageCount = 2;
    uint32_t imageCount = 3;
	vk::Format colorFormat = vk::Format::eB8G8R8A8Srgb;
	vk::Format depthStencilFormat = vk::Format::eD32SfloatS8Uint;
	double renderTime = 0.0;
private:
	void showMetrics();
	void initImGui();

	VulkanDevice* vulkanDevice;
	SDL_Window* window;

    vk::DescriptorPool descriptorPool;
};