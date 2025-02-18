#include "core/Core.hpp"
#include "imgui_impl_glfw.h"

namespace Engine
{
	void Application::init()
	{
		window = std::make_unique<Window>();
		window->create();

		device = std::make_unique<Device>();
		device->init(window->getHandle());
		renderer->init(device.get());

		onInit();
	}

	void Application::destroy()
	{
		device->waitIdle();

		onDestroy();

		renderer->destroy();
		device->destroy();
		window->destroy();
	}

	void Application::prepare()
	{
		if (renderer == nullptr)
		{
			throw std::runtime_error("Renderer is not set");
		}

		registerInstance();
		glfwSetKeyCallback((GLFWwindow*)window->getHandle(), EventBase::keyCallbackDispatch);
		glfwSetWindowSizeCallback((GLFWwindow*)window->getHandle(), EventBase::resizeCallbackDispatch);
		glfwSetMouseButtonCallback((GLFWwindow*)window->getHandle(), EventBase::mouseButtonCallbackDispatch);
		glfwSetScrollCallback((GLFWwindow*)window->getHandle(), ImGui_ImplGlfw_ScrollCallback);

		onPrepare();
		renderer->prepare();
	}

	void Application::update()
	{
		onUpdate();
		renderer->update();
	}

	void Application::render()
	{
		renderer->render();
	}

	void Application::mainLoop()
	{
		prepare();

		bool running = true;
		bool minimized = false;

		while (running)
		{
			window->pollEvents();
			if (window->shouldClose())
			{
				running = false;
			}

			minimized = (window->minimized()) ? true : false;

			update();

			if (renderer->windowResized)
			{
				renderer->handleWindowResize();
				renderer->windowResized = false;
			}

			if (!minimized)
			{
				render();
			}
		}
	}

	void Application::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE)
		{
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
	}

	void Application::resizeCallback(GLFWwindow* window, int width, int height)
	{
		this->window->updateSize(width, height);
		this->renderer->windowResized = true;
	}

	void Application::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
	}

	void Application::scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
	{
		ImGui_ImplGlfw_ScrollCallback(window, xOffset, yOffset);
	}

	std::vector<char> readFile(const std::string& filename)
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
}
