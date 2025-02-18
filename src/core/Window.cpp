#include "core/Window.hpp"
#include "imgui_impl_glfw.h"

#include <stb_image.h>
#include <functional>
#include <format>
#include <cstdlib>
#include <ctime>

namespace Engine
{
	void Window::create()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(width, height, "Engine", nullptr, nullptr);

		GLFWimage images[1]{};
		srand(time(0));
		int random = rand() % 8 + 1;
		auto path = RESOURCE_DIR + std::format("icons/0{}.png", random);
		images[0].pixels = stbi_load(path.c_str(), &images[0].width, &images[0].height, 0, 4); //rgba channels 
		glfwSetWindowIcon(window, 1, images);
		stbi_image_free(images[0].pixels);
	}

	void Window::destroy()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void Window::pollEvents()
	{
		glfwPollEvents();
	}

	bool Window::shouldClose()
	{
		return glfwWindowShouldClose(window);
	}

	bool Window::minimized()
	{
		return glfwGetWindowAttrib(window, GLFW_ICONIFIED);
	}

	void Window::updateSize(int width, int height)
	{
		this->width = width;
		this->height = height;
	}

	void (*buildCallback())(GLFWwindow*, int, int)
	{
		void (*callback)(GLFWwindow*, int, int) = [](GLFWwindow* window, int width, int height)
			{
				ImGui_ImplGlfw_WindowFocusCallback(window, width);
			};

		return callback;
	}

	EventBase* EventBase::instance;
}
