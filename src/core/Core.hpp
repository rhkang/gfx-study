#pragma once

#include <GLFW/glfw3.h>

#include <string>
#include <fstream>

#include "gfx/vulkan/Device.hpp"
#include "gfx/vulkan/Renderer.hpp"

#include "core/Window.hpp"

namespace Engine {
class Application : EventBase {
   public:
    void init();
    void prepare();
    void mainLoop();
    void update();
    void render();
    void destroy();

    virtual void onInit() {}
    virtual void onPrepare() {}
    virtual void onUpdate() {}
    virtual void onDestroy() {}

    std::unique_ptr<Device> device;
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;

   protected:
    std::string appName = "Renderer";

   private:
    void keyCallback(
        GLFWwindow* window, int key, int scancode, int action, int mods
    ) override;

    void resizeCallback(GLFWwindow* window, int width, int height) override;

    void mouseButtonCallback(
        GLFWwindow* window, int button, int action, int mods
    ) override;
};
}  // namespace Engine