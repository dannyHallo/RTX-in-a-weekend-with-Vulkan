#include "ComputePipeline.hpp"
#include "app-context/VulkanApplicationContext.hpp"
#include "pipelines/DescriptorSetBundle.hpp"
#include "utils/logger/Logger.hpp"

#include <cassert>
#include <memory>
#include <vector>

void ComputePipeline::init() {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  // this is why the compute pipeline requires the descriptor set layout to be specified
  pipelineLayoutInfo.pSetLayouts = &_descriptorSetBundle->getDescriptorSetLayout();

  VkResult result = vkCreatePipelineLayout(_appContext->getDevice(), &pipelineLayoutInfo, nullptr,
                                           &_pipelineLayout);
  assert(result == VK_SUCCESS && "failed to create pipeline layout");

  VkShaderModule shaderModule = _createShaderModule(_shaderCode);

  VkPipelineShaderStageCreateInfo shaderStageInfo{};
  shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStageInfo.module = shaderModule;
  shaderStageInfo.pName  = "main"; // name of the entry function of current shader

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.layout = _pipelineLayout;
  computePipelineCreateInfo.flags  = 0;
  computePipelineCreateInfo.stage  = shaderStageInfo;

  result = vkCreateComputePipelines(_appContext->getDevice(), VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr, &_pipeline);
  assert(result == VK_SUCCESS && "failed to create compute pipeline");

  // the shader module can be destroyed after the pipeline has been created
  vkDestroyShaderModule(_appContext->getDevice(), shaderModule, nullptr);
}

void ComputePipeline::recordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame,
                                    uint32_t threadCountX, uint32_t threadCountY,
                                    uint32_t threadCountZ) {
  _bind(commandBuffer, currentFrame);
  vkCmdDispatch(commandBuffer, std::ceil((float)threadCountX / (float)_workGroupSize.x),
                std::ceil((float)threadCountY / (float)_workGroupSize.y),
                std::ceil((float)threadCountZ / (float)_workGroupSize.z));
}

void ComputePipeline::recordIndirectCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame,
                                            VkBuffer indirectBuffer) {
  _bind(commandBuffer, currentFrame);
  vkCmdDispatchIndirect(commandBuffer, indirectBuffer, 0);
}