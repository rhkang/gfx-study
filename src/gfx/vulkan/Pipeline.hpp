#pragma once

#include "gfx/vulkan/VulkanUsage.hpp"

namespace Engine {
class PipelineBuilder {
   public:
    PipelineBuilder(vk::Device device);

    vk::Pipeline build();
    inline void setLayout(vk::PipelineLayout layout) { this->layout = layout; }

    std::vector<vk::Format> colorAttachmentFormats;

    vk::Format colorFormat = vk::Format::eB8G8R8A8Srgb;
    vk::Format depthAttachmentFormat = vk::Format::eD32SfloatS8Uint;
    vk::Format stencilAttachmentFormat = vk::Format::eUndefined;

    vk::GraphicsPipelineCreateInfo graphicsPipelineCI{};

    vk::PipelineVertexInputStateCreateInfo vertexInputCI{};
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
    vk::PipelineViewportStateCreateInfo viewportCI{};
    vk::PipelineRasterizationStateCreateInfo rasterizationCI{};
    vk::PipelineMultisampleStateCreateInfo multisampleCI{};
    vk::PipelineDepthStencilStateCreateInfo depthStencilCI{};
    vk::PipelineColorBlendStateCreateInfo colorBlendCI{};
    vk::PipelineDynamicStateCreateInfo dynamicStateCI{};

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    std::vector<vk::PipelineColorBlendAttachmentState>
        colorBlendAttachmentStates{};
    std::vector<vk::DynamicState> dynamicStates{};

    void addColorAttachment(vk::Format format);
    void addColorAttachment(
        vk::Format format,
        vk::PipelineColorBlendAttachmentState colorBlendState);

   private:
    vk::Device device;
    vk::PipelineLayout layout;
};
}  // namespace Engine