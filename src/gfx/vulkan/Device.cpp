#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "gfx/vulkan/Device.hpp"
#include "gfx/vulkan/Utils.hpp"
#include "core/Log.hpp"

namespace Engine {
void Device::init(void* window) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init();

    vk::ApplicationInfo appInfo{
        .pApplicationName = "Vulkan Device",
        .apiVersion = vk::ApiVersion13,
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    instanceExtensions.insert(
        instanceExtensions.end(), glfwExtensions,
        glfwExtensions + glfwExtensionCount
    );

    vk::InstanceCreateInfo ici{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    };

    instance = vk::createInstance(ici);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

    vk::DebugUtilsMessengerCreateInfoEXT dumci{
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugMessageFunc,
    };
    debugMessenger = instance.createDebugUtilsMessengerEXT(dumci);

    auto physicalDevices = instance.enumeratePhysicalDevices();
    assert(!physicalDevices.empty());
    physicalDevice = physicalDevices[0];

    auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    auto queueFamilyIndex = static_cast<uint32_t>(std::distance(
        queueFamilyProperties.begin(),
        std::find_if(
            queueFamilyProperties.begin(), queueFamilyProperties.end(),
            [](const vk::QueueFamilyProperties& qfp) {
                return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
            }
        )
    ));

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo dqci{
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    auto physicalDevicefeatures = physicalDevice.getFeatures();
    if (!physicalDevicefeatures.samplerAnisotropy) {
        LOG_ERROR("Physical device does not support sampler anisotropy");
    }

    vk::PhysicalDeviceDynamicRenderingFeatures drFeatures{
        .dynamicRendering = vk::True,
    };
    vk::PhysicalDeviceFeatures2 features2{
        .pNext = &drFeatures,
        .features =
            {
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

    VmaAllocatorCreateInfo allocatorCI{
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
    };
    vmaCreateAllocator(&allocatorCI, &allocator);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(instance, (GLFWwindow*)window, nullptr, &surface);
    this->windowHandle = window;
    this->surface = surface;

    uiLayout = std::make_unique<UiLayout>(this);
    uiLayout->init();
}

void Device::destroy() {
    device.waitIdle();

    uiLayout.reset();

    device.destroyCommandPool(cmdPool);
    device.destroyDescriptorPool(descriptorPool);

    vmaDestroyAllocator(allocator);

    device.destroy();
    instance.destroySurfaceKHR(surface);
    instance.destroyDebugUtilsMessengerEXT(debugMessenger);
    instance.destroy();
}

void Device::waitIdle() { device.waitIdle(); }

vk::CommandBuffer Device::allocateCommandBuffer(bool begin) {
    vk::CommandBuffer cmdBuffer =
        device
            .allocateCommandBuffers({
                .commandPool = cmdPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            })
            .front();

    if (begin) {
        cmdBuffer.begin({vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}});
    }

    return cmdBuffer;
}

void Device::flushCommandBuffer(vk::CommandBuffer cmdBuffer) {
    cmdBuffer.end();

    vk::SubmitInfo submitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer,
    };

    queue.submit(submitInfo);
    queue.waitIdle();

    device.freeCommandBuffers(cmdPool, cmdBuffer);
}

vk::ShaderModule Device::createShaderModule(const char* filename) {
    std::string fullPath = SHADER_DIR + std::string(filename);

    auto fileBinary = readFile(fullPath);

    vk::ShaderModuleCreateInfo shaderModuleCI{
        .codeSize = fileBinary.size(),
        .pCode = reinterpret_cast<uint32_t*>(fileBinary.data()),
    };

    return device.createShaderModule(shaderModuleCI);
}

Buffer Device::createBuffer() {
    Buffer buffer{};
    buffer.device = this;
    return buffer;
}

Texture Device::createTexture() {
    Texture texture{};
    texture.device = this;
    return texture;
}
}  // namespace Engine