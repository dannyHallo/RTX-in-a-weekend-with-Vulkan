#pragma once
#include "../app-context/VulkanApplicationContext.h"
#include "../scene/Scene.h"
#include "../utils/vulkan.h"
#include <memory>
#include <vector>

namespace RenderSystem {
void allocateCommandBuffers(std::vector<VkCommandBuffer> &commandBuffers, uint32_t numBuffers) {
  commandBuffers.resize(numBuffers);
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = vulkanApplicationContext.getCommandPool();
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = numBuffers;

  if (vkAllocateCommandBuffers(vulkanApplicationContext.getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void beginCommandBuffer(const VkCommandBuffer &commandBuffer) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags            = 0;       // Optional
  beginInfo.pInheritanceInfo = nullptr; // Optional

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }
}

void endCommandBuffer(const VkCommandBuffer &commandBuffer) {
  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

VkCommandBuffer beginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool        = vulkanApplicationContext.getCommandPool();
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(vulkanApplicationContext.getDevice(), &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &commandBuffer;

  vkQueueSubmit(vulkanApplicationContext.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(vulkanApplicationContext.getGraphicsQueue());

  vkFreeCommandBuffers(vulkanApplicationContext.getDevice(), vulkanApplicationContext.getCommandPool(), 1, &commandBuffer);
}

void submit(const VkCommandBuffer *commandBuffer, const size_t &numWaitSemaphores, const VkSemaphore *waitSemaphores,
            const VkPipelineStageFlags *waitStages, const VkSemaphore *signalSemaphores, VkFence &fence) {
  VkSubmitInfo submitInfo{};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = static_cast<uint32_t>(numWaitSemaphores);
  submitInfo.pWaitSemaphores    = waitSemaphores;
  submitInfo.pWaitDstStageMask  = waitStages;

  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = commandBuffer;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = signalSemaphores;

  if (vkQueueSubmit(vulkanApplicationContext.getGraphicsQueue(), 1, &submitInfo, fence) != VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }
}

void present(const uint32_t &imageIndex, const VkSemaphore *semaphores, const size_t &numSemaphores) {
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = static_cast<uint32_t>(numSemaphores);
  presentInfo.pWaitSemaphores    = semaphores;
  VkSwapchainKHR swapChains[]    = {vulkanApplicationContext.getSwapchain()};
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = swapChains;
  presentInfo.pImageIndices      = &imageIndex;
  presentInfo.pResults           = nullptr;

  VkResult result = vkQueuePresentKHR(vulkanApplicationContext.getPresentQueue(), &presentInfo);

  if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}
} // namespace RenderSystem