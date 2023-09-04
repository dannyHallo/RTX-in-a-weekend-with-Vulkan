#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

// VOLK_IMPLEMENTATION lets volk define the functions, by letting volk.h include volk.c
// this must only be defined in one translation unit
#define VOLK_IMPLEMENTATION
#include "app-context/VulkanApplicationContext.h"

#include "utils/logger.h"

#include "memory/Image.h"

#include <algorithm>
#include <cstdint>
#include <set>

static const std::vector<const char *> validationLayers         = {"VK_LAYER_KHRONOS_validation"};
static const std::vector<const char *> requiredDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
static const bool enableDebug                                   = true;

std::unique_ptr<VulkanApplicationContext> VulkanApplicationContext::sInstance = nullptr;

VulkanApplicationContext *VulkanApplicationContext::initInstance(GLFWwindow *window) {
  if (sInstance != nullptr) {
    logger::throwError("VulkanApplicationContext::initInstance: instance is already initialized");
  }
  sInstance = std::unique_ptr<VulkanApplicationContext>(new VulkanApplicationContext(window));
  return sInstance.get();
}

void VulkanApplicationContext::destroyInstance() {
  if (sInstance == nullptr) {
    logger::throwError("VulkanApplicationContext::destroyInstance: instance is not initialized");
  }
  sInstance.reset(nullptr);
}

VulkanApplicationContext *VulkanApplicationContext::getInstance() {
  if (sInstance == nullptr) {
    logger::throwError("VulkanApplicationContext::getInstance: instance is not initialized");
  }
  return sInstance.get();
}

VulkanApplicationContext::VulkanApplicationContext(GLFWwindow *glWindow) : mGlWindow(glWindow) {
  VkResult result = volkInitialize();
  logger::checkStep("volkInitialize", result);

  createInstance();

  // load instance related functions
  volkLoadInstance(mVkInstance);

  setupDebugMessager();
  createSurface();
  createDevice();

  // reduce loading overhead by specifing only one device is used
  volkLoadDevice(mDevice);

  createSwapchain();

  createAllocator();
  createCommandPool();
}

VulkanApplicationContext::~VulkanApplicationContext() {
  logger::print("Destroying VulkanApplicationContext");

  vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
  vkDestroyCommandPool(mDevice, mGuiCommandPool, nullptr);

  for (auto &swapchainImageView : mSwapchainImageViews) {
    vkDestroyImageView(mDevice, swapchainImageView, nullptr);
  }

  vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

  vkDestroySurfaceKHR(mVkInstance, mSurface, nullptr);

  // this step destroys allocated VkDestroyMemory allocated by VMA when creating buffers and images,
  // by destroying the global allocator
  vmaDestroyAllocator(mAllocator);
  vkDestroyDevice(mDevice, nullptr);

  if (enableDebug) {
    vkDestroyDebugUtilsMessengerEXT(mVkInstance, mDebugMessager, nullptr);
  }

  vkDestroyInstance(mVkInstance, nullptr);
}

bool VulkanApplicationContext::checkValidationLayerSupport() {
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  // logger::print all availiable layers
  logger::print("available validation layers", availableLayers.size());
  std::set<std::string> availableLayersSet{};
  for (const auto &layerProperty : availableLayers) {
    availableLayersSet.insert(static_cast<const char *>(layerProperty.layerName));
    logger::print("\t", static_cast<const char *>(layerProperty.layerName));
  }

  logger::print();
  logger::print("using validation layers", validationLayers.size());

  std::vector<std::string> unavailableLayerNames{};
  // for each validation layer, we check for its validity from the avaliable layer pool
  for (const auto &layerName : validationLayers) {
    logger::print("\t", layerName);
    if (availableLayersSet.find(layerName) == availableLayersSet.end()) {
      unavailableLayerNames.emplace_back(layerName);
    }
  }

  if (unavailableLayerNames.empty()) {
    logger::print("\t\t");
    return true;
  }

  for (const auto &unavailableLayerName : unavailableLayerNames) {
    logger::print("\t", unavailableLayerName.c_str());
  }
  logger::print("\t\t");
  return false;
}

bool VulkanApplicationContext::checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice) {
  uint32_t extensionCount = 0;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

  std::set<std::string> availableExtensionsSet{};
  for (const auto &extension : availableExtensions) {
    availableExtensionsSet.insert(static_cast<const char *>(extension.extensionName));
  }

  logger::print("available device extensions count", availableExtensions.size());
  logger::print();
  logger::print("using device extensions", requiredDeviceExtensions.size());
  for (const auto &extensionName : requiredDeviceExtensions) {
    logger::print("\t", extensionName);
  }
  logger::print();
  logger::print();

  std::vector<std::string> unavailableExtensionNames{};
  for (const auto &requiredExtension : requiredDeviceExtensions) {
    if (availableExtensionsSet.find(requiredExtension) == availableExtensionsSet.end()) {
      unavailableExtensionNames.emplace_back(requiredExtension);
    }
  }

  if (unavailableExtensionNames.empty()) {
    return true;
  }

  logger::print("the following device extensions are not available:");
  for (const auto &unavailableExtensionName : unavailableExtensionNames) {
    logger::print("\t", unavailableExtensionName.c_str());
  }
  return false;
}

// returns instance required extension names (i.e glfw, validation layers), they are device-irrational extensions
std::vector<const char *> VulkanApplicationContext::getRequiredInstanceExtensions() {
  // Get glfw required extensions
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions = nullptr;

  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

  // Due to the nature of the Vulkan interface, there is very little error information available to the developer and
  // application. By using the VK_EXT_debug_utils extension, developers can obtain more information. When combined with
  // validation layers, even more detailed feedback on the application’s use of Vulkan will be provided.
  if (enableDebug) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

// we can change the color of the debug messages from this callback function!
// in this case, we change the debug messages to red
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
                                             VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
                                             const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                             void * /*pUserData*/) {

  // we may change display color according to its importance level
  // if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){...}

  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
  return VK_FALSE;
}

void populateDebugMessagerInfo(VkDebugUtilsMessengerCreateInfoEXT &debugCreateInfo) {
  // Avoid some of the debug details by leaving some of the flags
  debugCreateInfo       = {};
  debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

  // customize message severity here, to focus on the most significant messages that the validation layer can give us
  debugCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  // leaving the following message severities out, for simpler validation debug infos
  // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
  // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT

  // we'd like to leave all the message types out
  debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugCreateInfo.pfnUserCallback = debugCallback;
}

void VulkanApplicationContext::createInstance() {
  VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};

  VkApplicationInfo appInfo{};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName   = "Compute Ray Tracing";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName        = "No Engine";
  appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion         = VK_API_VERSION_1_2;

  createInfo.pApplicationInfo = &appInfo;

  // Get all available extensions
  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> availavleExtensions(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availavleExtensions.data());

  // logger::prints all available instance extensions
  logger::print("available instance extensions", availavleExtensions.size());
  for (const auto &extension : availavleExtensions) {
    logger::print("\t", static_cast<const char *>(extension.extensionName));
  }
  logger::print();

  // get glfw (+ debug) extensions
  auto instanceRequiredExtensions    = getRequiredInstanceExtensions();
  createInfo.enabledExtensionCount   = static_cast<uint32_t>(instanceRequiredExtensions.size());
  createInfo.ppEnabledExtensionNames = instanceRequiredExtensions.data();

  logger::print("using instance extensions", instanceRequiredExtensions.size());
  for (const auto &extension : instanceRequiredExtensions) {
    logger::print("\t", extension);
  }
  logger::print();
  logger::print();

  // Setup debug messager info during vkCreateInstance and vkDestroyInstance
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
  if (enableDebug) {
    if (!checkValidationLayerSupport()) {
      logger::throwError("Validation layers requested, but not available!");
    }
    createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    populateDebugMessagerInfo(debugCreateInfo);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext             = nullptr;
  }

  // create VK Instance
  VkResult result = vkCreateInstance(&createInfo, nullptr, &mVkInstance);
  logger::checkStep("vkCreateInstance", result);
}

// setup runtime debug messager
void VulkanApplicationContext::setupDebugMessager() {
  if (!enableDebug) {
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  populateDebugMessagerInfo(createInfo);

  VkResult result = vkCreateDebugUtilsMessengerEXT(mVkInstance, &createInfo, nullptr, &mDebugMessager);
  logger::checkStep("vkCreateDebugUtilsMessengerEXT", result);
}

void VulkanApplicationContext::createSurface() {
  VkResult result = glfwCreateWindowSurface(mVkInstance, mGlWindow, nullptr, &mSurface);
  logger::checkStep("glfwCreateWindowSurface", result);
}

VkSampleCountFlagBits getDeviceMaxUsableSampleCount(VkPhysicalDevice device) {
  VkPhysicalDeviceProperties physicalDeviceProperties;
  vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

  VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                              physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  if ((counts & VK_SAMPLE_COUNT_64_BIT) != 0) {
    return VK_SAMPLE_COUNT_64_BIT;
  }
  if ((counts & VK_SAMPLE_COUNT_32_BIT) != 0) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  if ((counts & VK_SAMPLE_COUNT_16_BIT) != 0) {
    return VK_SAMPLE_COUNT_16_BIT;
  }
  if ((counts & VK_SAMPLE_COUNT_8_BIT) != 0) {
    return VK_SAMPLE_COUNT_8_BIT;
  }
  if ((counts & VK_SAMPLE_COUNT_4_BIT) != 0) {
    return VK_SAMPLE_COUNT_4_BIT;
  }
  if ((counts & VK_SAMPLE_COUNT_2_BIT) != 0) {
    return VK_SAMPLE_COUNT_2_BIT;
  }

  return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanApplicationContext::checkDeviceSuitable(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice) {
  // Check if the queue family is valid
  QueueFamilyIndices indices{};
  bool indicesAreFilled = findQueueFamilies(physicalDevice, indices);
  // Check extension support
  bool extensionSupported = checkDeviceExtensionSupport(physicalDevice);
  bool swapChainAdequate  = false;
  if (extensionSupported) {
    SwapchainSupportDetails swapChainSupport = querySwapchainSupport(surface, physicalDevice);
    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
  }

  // Query for device features if needed
  // VkPhysicalDeviceFeatures supportedFeatures;
  // vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
  if (indicesAreFilled && extensionSupported && swapChainAdequate) {
    return;
  }

  logger::throwError("physical device not suitable");
}

// helper function to customize the physical device ranking mechanism, returns the physical device with the highest
// score
// the marking criteria should be further optimized
VkPhysicalDevice VulkanApplicationContext::selectBestDevice(std::vector<VkPhysicalDevice> physicalDevices) {
  VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

  static constexpr uint32_t kDiscreteGpuMark   = 100;
  static constexpr uint32_t kIntegratedGpuMark = 20;

  // Give marks to all devices available, returns the best usable device
  std::vector<uint32_t> deviceMarks(physicalDevices.size());
  size_t deviceId = 0;

  logger::print("-------------------------------------------------------");

  for (const auto &physicalDevice : physicalDevices) {

    VkPhysicalDeviceProperties deviceProperty;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperty);

    // Discrete GPU will mark better
    if (deviceProperty.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      deviceMarks[deviceId] += kDiscreteGpuMark;
    } else if (deviceProperty.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      deviceMarks[deviceId] += kIntegratedGpuMark;
    }

    VkPhysicalDeviceMemoryProperties memoryProperty;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperty);

    auto *heapsPointer = static_cast<VkMemoryHeap *>(memoryProperty.memoryHeaps);
    auto heaps         = std::vector<VkMemoryHeap>(heapsPointer, heapsPointer + memoryProperty.memoryHeapCount);

    size_t deviceMemory = 0;
    for (const auto &heap : heaps) {
      // At least one heap has this flag
      if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
        deviceMemory += heap.size;
      }
    }

    // MSAA
    VkSampleCountFlagBits msaaSamples = getDeviceMaxUsableSampleCount(physicalDevice);

    std::cout << "Device " << deviceId << "    " << static_cast<const char *>(deviceProperty.deviceName)
              << "    Memory in bytes: " << deviceMemory << "    MSAA max sample count: " << msaaSamples
              << "    Mark: " << deviceMarks[deviceId] << "\n";

    deviceId++;
  }

  logger::print("-------------------------------------------------------");
  logger::print();

  uint32_t bestMark = 0;
  deviceId          = 0;

  for (const auto &deviceMark : deviceMarks) {
    if (deviceMark > bestMark) {
      bestMark   = deviceMark;
      bestDevice = physicalDevices[deviceId];
    }

    deviceId++;
  }

  if (bestDevice == VK_NULL_HANDLE) {
    logger::throwError("no suitable GPU found.");
  } else {
    VkPhysicalDeviceProperties bestDeviceProperty;
    vkGetPhysicalDeviceProperties(bestDevice, &bestDeviceProperty);
    std::cout << "Selected: " << static_cast<const char *>(bestDeviceProperty.deviceName) << std::endl;
    logger::print();

    checkDeviceSuitable(mSurface, bestDevice);
  }
  return bestDevice;
}

bool VulkanApplicationContext::queueIndicesAreFilled(const VulkanApplicationContext::QueueFamilyIndices &indices) {
  return indices.computeFamily != -1 && indices.transferFamily != -1 && indices.graphicsFamily != -1 &&
         indices.presentFamily != -1;
}

bool VulkanApplicationContext::findQueueFamilies(VkPhysicalDevice physicalDevice,
                                                 VulkanApplicationContext::QueueFamilyIndices &indices) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    const auto &queueFamily = queueFamilies[i];

    if (indices.computeFamily == -1) {
      if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
        indices.computeFamily = i;
      }
    }

    if (indices.transferFamily == -1) {
      if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
        indices.transferFamily = i;
      }
    }

    if (indices.graphicsFamily == -1) {
      if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
        uint32_t presentSupport = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, mSurface, &presentSupport);
        if (presentSupport != 0) {
          indices.graphicsFamily = i;
          indices.presentFamily  = i;
        }
      }
    }

    if (queueIndicesAreFilled(indices)) {
      return true;
    }
  }
  return false;
}

// pick the most suitable physical device, and create logical device from it
void VulkanApplicationContext::createDevice() {
  // pick the physical device with the best performance
  {
    mPhysicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mVkInstance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      logger::throwError("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(mVkInstance, &deviceCount, physicalDevices.data());

    mPhysicalDevice = selectBestDevice(physicalDevices);
  }

  // create logical device from the physical device we've picked
  {
    findQueueFamilies(mPhysicalDevice, mQueueFamilyIndices);

    std::set<uint32_t> queueFamilyIndicesSet = {mQueueFamilyIndices.graphicsFamily, mQueueFamilyIndices.presentFamily,
                                                mQueueFamilyIndices.computeFamily, mQueueFamilyIndices.transferFamily};

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.F; // ranges from 0 - 1.;
    for (uint32_t queueFamilyIndex : queueFamilyIndicesSet) {
      VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
      queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
      queueCreateInfo.queueCount       = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    rayTracingPipeline.pNext              = &bufferDeviceAddress;
    rayTracingPipeline.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR rayTracingStructure = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    rayTracingStructure.pNext                 = &rayTracingPipeline;
    rayTracingStructure.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
    // descriptorIndexing.pNext = &rayTracingStructure; // uncomment this to enable the features above

    physicalDeviceFeatures.pNext = &descriptorIndexing;

    vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &physicalDeviceFeatures); // enable all the features our GPU has

    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.pNext                = &physicalDeviceFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos    = queueCreateInfos.data();
    // createInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.pEnabledFeatures = nullptr;

    // enabling device extensions
    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    // The enabledLayerCount and ppEnabledLayerNames fields of
    // VkDeviceCreateInfo are ignored by up-to-date implementations.
    deviceCreateInfo.enabledLayerCount   = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;

    VkResult result = vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice);
    logger::checkStep("vkCreateDevice", result);

    vkGetDeviceQueue(mDevice, getGraphicsFamilyIndex(), 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, getPresentFamilyIndex(), 0, &mPresentQueue);
    vkGetDeviceQueue(mDevice, getComputeFamilyIndex(), 0, &mComputeQueue);
    vkGetDeviceQueue(mDevice, getTransferFamilyIndex(), 0, &mTransferQueue);

    // // if raytracing support requested - let's get raytracing properties to
    // // know shader header size and max recursion
    // mRTProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    // VkPhysicalDeviceProperties2 devProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    // devProps.pNext      = &mRTProps;
    // devProps.properties = {};

    // vkGetPhysicalDeviceProperties2(mPhysicalDevice, &devProps);
  }
}

// query for physical device's swapchain sepport details
VulkanApplicationContext::SwapchainSupportDetails
VulkanApplicationContext::querySwapchainSupport(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice) {
  // get swapchain support details using surface and device info
  SwapchainSupportDetails details;

  // get capabilities
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

  // get surface format
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
  }

  // get available presentation modes
  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

// find the most suitable swapchain surface format
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    // format: VK_FORMAT_B8G8R8A8_SRGB
    // this is actually irretional due to imgui impl
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB) {
      return availableFormat;
    }
  }

  logger::print("Surface format requirement didn't meet, the first available format is chosen!");
  return availableFormats[0];
}

// choose the present mode of the swapchain
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
  // our preferance: Mailbox present mode
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  logger::print("Present mode preferance doesn't meet, switching to FIFO");
  return VK_PRESENT_MODE_FIFO_KHR;
}

// return the current extent, or create another one
VkExtent2D VulkanApplicationContext::getSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
  // if the current extent is valid (we can read the width and height out of it)
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    std::cout << "Using resolution: (" << capabilities.currentExtent.width << ", " << capabilities.currentExtent.height
              << ")" << std::endl;
    return capabilities.currentExtent;
  }

  logger::throwError("This part shouldn't be reached!");
  return {};

  // int width  = 0;
  // int height = 0;
  // glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);

  // VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
  // actualExtent.width =
  //     std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  // actualExtent.height =
  //     std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  // return actualExtent;
}

// create swapchain and swapchain imageviews
void VulkanApplicationContext::createSwapchain() {
  SwapchainSupportDetails swapchainSupport = querySwapchainSupport(mSurface, mPhysicalDevice);
  VkSurfaceFormatKHR surfaceFormat         = chooseSwapSurfaceFormat(swapchainSupport.formats);
  mSwapchainImageFormat                    = surfaceFormat.format;
  VkPresentModeKHR presentMode             = chooseSwapPresentMode(swapchainSupport.presentModes);
  mSwapchainExtent                         = getSwapExtent(swapchainSupport.capabilities);

  // recommanded: min + 1
  uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
  // otherwise: max
  if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
    imageCount = swapchainSupport.capabilities.maxImageCount;
  }
  logger::print("number of swapchain images", imageCount);

  VkSwapchainCreateInfoKHR swapchainCreateInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  swapchainCreateInfo.surface          = mSurface;
  swapchainCreateInfo.minImageCount    = imageCount;
  swapchainCreateInfo.imageFormat      = surfaceFormat.format;
  swapchainCreateInfo.imageColorSpace  = surfaceFormat.colorSpace;
  swapchainCreateInfo.imageExtent      = mSwapchainExtent;
  swapchainCreateInfo.imageArrayLayers = 1; // the amount of layers each image consists of
  swapchainCreateInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  uint32_t queueFamilyIndicesArray[] = {mQueueFamilyIndices.graphicsFamily, mQueueFamilyIndices.presentFamily};

  if (mQueueFamilyIndices.graphicsFamily != mQueueFamilyIndices.presentFamily) {
    // images can be used across multiple queue families without explicit ownership transfers.
    swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    swapchainCreateInfo.queueFamilyIndexCount = 2;
    swapchainCreateInfo.pQueueFamilyIndices   = static_cast<const uint32_t *>(queueFamilyIndicesArray);
  } else {
    // an image is owned by one queue family at a time and ownership must be explicitly transferred before the image is
    // being used in another queue family. This offers the best performance.
    swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;       // Optional
    swapchainCreateInfo.pQueueFamilyIndices   = nullptr; // Optional
  }

  swapchainCreateInfo.preTransform   = swapchainSupport.capabilities.currentTransform;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  swapchainCreateInfo.presentMode = presentMode;
  swapchainCreateInfo.clipped     = VK_TRUE;

  swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

  VkResult result = vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, nullptr, &mSwapchain);
  logger::checkStep("vkCreateSwapchainKHR", result);

  // obtain the actual number of swapchain images
  vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
  mSwapchainImages.resize(imageCount);
  mSwapchainImageViews.reserve(imageCount);
  vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

  for (size_t i = 0; i < imageCount; i++) {
    mSwapchainImageViews.emplace_back(
        Image::createImageView(mDevice, mSwapchainImages[i], mSwapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT));
  }
}

void VulkanApplicationContext::createAllocator() {
  // load vulkan functions dynamically
  VmaVulkanFunctions vmaVulkanFunc{};
  vmaVulkanFunc.vkAllocateMemory                        = vkAllocateMemory;
  vmaVulkanFunc.vkBindBufferMemory                      = vkBindBufferMemory;
  vmaVulkanFunc.vkBindImageMemory                       = vkBindImageMemory;
  vmaVulkanFunc.vkCreateBuffer                          = vkCreateBuffer;
  vmaVulkanFunc.vkCreateImage                           = vkCreateImage;
  vmaVulkanFunc.vkDestroyBuffer                         = vkDestroyBuffer;
  vmaVulkanFunc.vkDestroyImage                          = vkDestroyImage;
  vmaVulkanFunc.vkFlushMappedMemoryRanges               = vkFlushMappedMemoryRanges;
  vmaVulkanFunc.vkFreeMemory                            = vkFreeMemory;
  vmaVulkanFunc.vkGetBufferMemoryRequirements           = vkGetBufferMemoryRequirements;
  vmaVulkanFunc.vkGetImageMemoryRequirements            = vkGetImageMemoryRequirements;
  vmaVulkanFunc.vkGetPhysicalDeviceMemoryProperties     = vkGetPhysicalDeviceMemoryProperties;
  vmaVulkanFunc.vkGetPhysicalDeviceProperties           = vkGetPhysicalDeviceProperties;
  vmaVulkanFunc.vkInvalidateMappedMemoryRanges          = vkInvalidateMappedMemoryRanges;
  vmaVulkanFunc.vkMapMemory                             = vkMapMemory;
  vmaVulkanFunc.vkUnmapMemory                           = vkUnmapMemory;
  vmaVulkanFunc.vkCmdCopyBuffer                         = vkCmdCopyBuffer;
  vmaVulkanFunc.vkGetBufferMemoryRequirements2KHR       = vkGetBufferMemoryRequirements2;
  vmaVulkanFunc.vkGetImageMemoryRequirements2KHR        = vkGetImageMemoryRequirements2;
  vmaVulkanFunc.vkBindBufferMemory2KHR                  = vkBindBufferMemory2;
  vmaVulkanFunc.vkBindImageMemory2KHR                   = vkBindImageMemory2;
  vmaVulkanFunc.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
  vmaVulkanFunc.vkGetDeviceBufferMemoryRequirements     = vkGetDeviceBufferMemoryRequirements;
  vmaVulkanFunc.vkGetDeviceImageMemoryRequirements      = vkGetDeviceImageMemoryRequirements;

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.vulkanApiVersion       = VK_API_VERSION_1_2;
  allocatorInfo.physicalDevice         = mPhysicalDevice;
  allocatorInfo.device                 = mDevice;
  allocatorInfo.instance               = mVkInstance;
  allocatorInfo.pVulkanFunctions       = &vmaVulkanFunc;

  VkResult result = vmaCreateAllocator(&allocatorInfo, &mAllocator);
  logger::checkStep("vmaCreateAllocator", result);
}

// create a command pool for rendering commands and a command pool for gui commands (imgui)
void VulkanApplicationContext::createCommandPool() {
  VkCommandPoolCreateInfo commandPoolCreateInfo1{};
  commandPoolCreateInfo1.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo1.queueFamilyIndex = getGraphicsFamilyIndex();

  VkResult result = vkCreateCommandPool(mDevice, &commandPoolCreateInfo1, nullptr, &mCommandPool);
  logger::checkStep("vkCreateCommandPool(commandPoolCreateInfo1)", result);

  VkCommandPoolCreateInfo commandPoolCreateInfo2{};
  commandPoolCreateInfo2.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo2.flags =
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allows the use of vkResetCommandBuffer
  commandPoolCreateInfo2.queueFamilyIndex = getGraphicsFamilyIndex();

  result = vkCreateCommandPool(mDevice, &commandPoolCreateInfo2, nullptr, &mGuiCommandPool);
  logger::checkStep("vkCreateCommandPool(commandPoolCreateInfo2)", result);
}

VkFormat VulkanApplicationContext::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                                                       VkFormatFeatureFlags features) const {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
      return format;
    }
    if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  logger::throwError("failed to find supported format!");
  return VK_FORMAT_UNDEFINED;
}