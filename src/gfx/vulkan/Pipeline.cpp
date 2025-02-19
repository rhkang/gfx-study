#include "gfx/vulkan/Pipeline.hpp"

namespace Engine {
PipelineBuilder::PipelineBuilder(vk::Device device) {
    this->device = device;

    inputAssemblyCI.topology = vk::PrimitiveTopology::eTriangleList;

    viewportCI.viewportCount = 1;
    viewportCI.scissorCount = 1;

    rasterizationCI.polygonMode = vk::PolygonMode::eFill;
    rasterizationCI.cullMode = vk::CullModeFlagBits::eBack;
    rasterizationCI.frontFace = vk::FrontFace::eClockwise;
    rasterizationCI.lineWidth = 1.0f;

    depthStencilCI.depthTestEnable = VK_TRUE;
    depthStencilCI.depthWriteEnable = VK_TRUE;
    depthStencilCI.depthCompareOp = vk::CompareOp::eLess;
    depthStencilCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilCI.stencilTestEnable = VK_FALSE;

    multisampleCI.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampleCI.sampleShadingEnable = vk::True;
    multisampleCI.minSampleShading = 1.0f;

    dynamicStates.push_back(vk::DynamicState::eViewport);
    dynamicStates.push_back(vk::DynamicState::eScissor);
    dynamicStateCI.dynamicStateCount =
        static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCI.pDynamicStates = dynamicStates.data();
}

vk::Pipeline PipelineBuilder::build() {
    colorBlendCI.attachmentCount =
        static_cast<uint32_t>(colorBlendAttachmentStates.size());
    colorBlendCI.pAttachments = colorBlendAttachmentStates.data();

    vk::PipelineRenderingCreateInfo pipelineRenderingCI{
        .colorAttachmentCount =
            static_cast<uint32_t>(colorAttachmentFormats.size()),
        .pColorAttachmentFormats = colorAttachmentFormats.data(),
    };

    if (depthAttachmentFormat != vk::Format::eUndefined) {
        pipelineRenderingCI.depthAttachmentFormat = depthAttachmentFormat;
    }
    if (stencilAttachmentFormat != vk::Format::eUndefined) {
        pipelineRenderingCI.stencilAttachmentFormat = stencilAttachmentFormat;
    }

    vk::GraphicsPipelineCreateInfo pipelineCI{
        .pNext = &pipelineRenderingCI,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputCI,
        .pInputAssemblyState = &inputAssemblyCI,
        .pTessellationState = VK_NULL_HANDLE,
        .pViewportState = &viewportCI,
        .pRasterizationState = &rasterizationCI,
        .pMultisampleState = &multisampleCI,
        .pDepthStencilState = &depthStencilCI,
        .pColorBlendState = &colorBlendCI,
        .pDynamicState = &dynamicStateCI,
        .layout = layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
    };

    auto resultValue =
        device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineCI);
    return resultValue.value;
}

void PipelineBuilder::addColorAttachment(vk::Format format) {
    colorAttachmentFormats.push_back(format);

    vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{
        .blendEnable = VK_FALSE,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
}

void PipelineBuilder::addColorAttachment(
    vk::Format format, vk::PipelineColorBlendAttachmentState colorBlendState) {
    colorAttachmentFormats.push_back(format);
    colorBlendAttachmentStates.push_back(colorBlendState);
}
}  // namespace Engine