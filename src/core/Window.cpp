#include "core/Window.hpp"
#include "imgui_impl_glfw.h"

#include <functional>

namespace Engine
{
	void Window::create()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(width, height, "Renderer", nullptr, nullptr);
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
