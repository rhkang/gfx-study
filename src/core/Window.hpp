#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>

namespace Engine {
class Window {
   public:
    void create();
    void destroy();

    void pollEvents();

    inline void* getHandle() { return window; }
    bool shouldClose();
    bool minimized();

    void updateSize(int width, int height);

   private:
    int width = 1920;
    int height = 1080;
    std::string title = "Engine";

    bool isMinimized = false;
    GLFWwindow* window;
};

class EventBase {
   public:
    static EventBase* instance;
    void registerInstance() { instance = this; }

    virtual void keyCallback(GLFWwindow* window, int key, int scancode,
                             int action, int mods) = 0;
    static void keyCallbackDispatch(GLFWwindow* window, int key, int scancode,
                                    int action, int mods) {
        if (instance) {
            instance->keyCallback(window, key, scancode, action, mods);
        }
    }

    virtual void resizeCallback(GLFWwindow* window, int width, int height) = 0;
    static void resizeCallbackDispatch(GLFWwindow* window, int width,
                                       int height) {
        if (instance) {
            instance->resizeCallback(window, width, height);
        }
    }

    virtual void mouseButtonCallback(GLFWwindow* window, int button, int action,
                                     int mods) = 0;
    static void mouseButtonCallbackDispatch(GLFWwindow* window, int button,
                                            int action, int mods) {
        if (instance) {
            instance->mouseButtonCallback(window, button, action, mods);
        }
    }
};
}  // namespace Engine