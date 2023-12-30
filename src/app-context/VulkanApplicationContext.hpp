#pragma once

// use stl version of std::min(), std::max(), and ignore the macro function with
// the same name provided by windows.h
#define NOMINMAX

// this should be defined first for the definition of VK_VERSION_1_0, which is
// used in glfw3.h
#include "context-creators/ContextCreators.hpp"

// glfw3 will define APIENTRY if it is not defined yet
#include "GLFW/glfw3.h"
#ifdef APIENTRY
#undef APIENTRY
#endif
// we undefine this to solve conflict with systemLog

#include "vk_mem_alloc.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

class Logger;
// also, this class should be configed out of class
class VulkanApplicationContext {
public:
  static VulkanApplicationContext *getInstance();

  // use glwindow to init the instance, can be only called once
  void init(Logger *logger, GLFWwindow *glWindow);

  ~VulkanApplicationContext();

  // disable move and copy
  VulkanApplicationContext(const VulkanApplicationContext &)            = delete;
  VulkanApplicationContext &operator=(const VulkanApplicationContext &) = delete;
  VulkanApplicationContext(VulkanApplicationContext &&)                 = delete;
  VulkanApplicationContext &operator=(VulkanApplicationContext &&)      = delete;

  void cleanupSwapchainDimensionRelatedResources();
  void createSwapchainDimensionRelatedResources();

  [[nodiscard]] inline const VkInstance &getVkInstance() const { return _vkInstance; }
  [[nodiscard]] inline const VkDevice &getDevice() const { return _device; }
  [[nodiscard]] inline const VkSurfaceKHR &getSurface() const { return _surface; }
  [[nodiscard]] inline const VkPhysicalDevice &getPhysicalDevice() const { return _physicalDevice; }

  [[nodiscard]] inline const VkCommandPool &getCommandPool() const { return _commandPool; }
  [[nodiscard]] inline const VkCommandPool &getGuiCommandPool() const { return _guiCommandPool; }
  [[nodiscard]] inline const VmaAllocator &getAllocator() const { return _allocator; }
  [[nodiscard]] inline const std::vector<VkImage> &getSwapchainImages() const {
    return _swapchainImages;
  }
  [[nodiscard]] inline const std::vector<VkImageView> &getSwapchainImageViews() const {
    return _swapchainImageViews;
  }
  [[nodiscard]] inline size_t getSwapchainSize() const { return _swapchainImages.size(); }
  [[nodiscard]] inline const VkFormat &getSwapchainImageFormat() const {
    return _swapchainImageFormat;
  }
  [[nodiscard]] inline const VkExtent2D &getSwapchainExtent() const { return _swapchainExtent; }
  [[nodiscard]] inline uint32_t getSwapchainExtentWidth() const { return _swapchainExtent.width; }
  [[nodiscard]] inline uint32_t getSwapchainExtentHeight() const { return _swapchainExtent.height; }
  [[nodiscard]] inline const VkSwapchainKHR &getSwapchain() const { return _swapchain; }

  [[nodiscard]] const VkQueue &getGraphicsQueue() const { return _graphicsQueue; }
  [[nodiscard]] const VkQueue &getPresentQueue() const { return _presentQueue; }
  [[nodiscard]] const VkQueue &getComputeQueue() const { return _computeQueue; }
  [[nodiscard]] const VkQueue &getTransferQueue() const { return _transferQueue; }

  [[nodiscard]] const ContextCreator::QueueFamilyIndices &getQueueFamilyIndices() const {
    return _queueFamilyIndices;
  }

private:
  // stores the indices of the each queue family, they might not overlap
  ContextCreator::QueueFamilyIndices _queueFamilyIndices;

  ContextCreator::SwapchainSupportDetails _swapchainSupportDetails;

  GLFWwindow *_glWindow = nullptr;
  Logger *_logger       = nullptr;

  VkInstance _vkInstance           = VK_NULL_HANDLE;
  VkSurfaceKHR _surface            = VK_NULL_HANDLE;
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDevice _device                 = VK_NULL_HANDLE;
  VmaAllocator _allocator          = VK_NULL_HANDLE;

  // These queues are implicitly cleaned up when the device is destroyed
  VkQueue _graphicsQueue = VK_NULL_HANDLE;
  VkQueue _presentQueue  = VK_NULL_HANDLE;
  VkQueue _computeQueue  = VK_NULL_HANDLE;
  VkQueue _transferQueue = VK_NULL_HANDLE;

  VkCommandPool _commandPool    = VK_NULL_HANDLE;
  VkCommandPool _guiCommandPool = VK_NULL_HANDLE;

  VkDebugUtilsMessengerEXT _debugMessager = VK_NULL_HANDLE;

  VkSwapchainKHR _swapchain      = VK_NULL_HANDLE;
  VkFormat _swapchainImageFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D _swapchainExtent    = {0, 0};

  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;

  VulkanApplicationContext() = default; // not allowed to be called outside

  void _initWindow(uint8_t windowSize);

  void _createCommandPool();
  void _createAllocator();

  static std::vector<const char *> _getRequiredInstanceExtensions();
  void _checkDeviceSuitable(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice);
  static bool _checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice);

  // find the indices of the queue families, return whether the indices are
  // fully filled
  bool _findQueueFamilies(VkPhysicalDevice physicalDevice,
                          ContextCreator::QueueFamilyIndices &indices);
  static bool _queueIndicesAreFilled(const ContextCreator::QueueFamilyIndices &indices);

  static ContextCreator::SwapchainSupportDetails
  _querySwapchainSupport(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice);
  VkPhysicalDevice _selectBestDevice(std::vector<VkPhysicalDevice> physicalDevices);
  static VkExtent2D _getSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
};