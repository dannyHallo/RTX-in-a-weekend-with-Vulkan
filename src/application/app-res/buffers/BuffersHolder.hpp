#pragma once

#include "memory/Buffer.hpp"
#include "ray-tracing/RtScene.hpp"
#include "utils/incl/Glm.hpp"

#include <memory>

constexpr int kGpuVecPackingSize = 4;

struct GlobalUniformBufferObject {
  alignas(sizeof(glm::vec3::x) * kGpuVecPackingSize) glm::vec3 camPosition;
  alignas(sizeof(glm::vec3::x) * kGpuVecPackingSize) glm::vec3 camFront;
  alignas(sizeof(glm::vec3::x) * kGpuVecPackingSize) glm::vec3 camUp;
  alignas(sizeof(glm::vec3::x) * kGpuVecPackingSize) glm::vec3 camRight;
  uint32_t swapchainWidth;
  uint32_t swapchainHeight;
  float vfov;
  uint32_t currentSample;
  float time;
};

struct GradientProjectionUniformBufferObject {
  int bypassGradientProjection;
  alignas(sizeof(glm::vec3::x) * kGpuVecPackingSize) glm::mat4 thisMvpe;
};

struct RtxUniformBufferObject {
  uint32_t numTriangles;
  uint32_t numLights;
  int movingLightSource;
  uint32_t outputType;
  float offsetX;
  float offsetY;
};

struct StratumFilterUniformBufferObject {
  int i;
  int bypassStratumFiltering;
};

struct TemporalFilterUniformBufferObject {
  int bypassTemporalFiltering;
  int useNormalTest;
  float normalThrehold;
  float blendingAlpha;
  alignas(sizeof(glm::vec3::x) * kGpuVecPackingSize) glm::mat4 lastMvpe;
};
struct VarianceUniformBufferObject {
  int bypassVarianceEstimation;
  int skipStoppingFunctions;
  int useTemporalVariance;
  int kernelSize;
  float phiGaussian;
  float phiDepth;
};

struct BlurFilterUniformBufferObject {
  int bypassBluring;
  int i;
  int iCap;
  int useVarianceGuidedFiltering;
  int useGradientInDepth;
  float phiLuminance;
  float phiDepth;
  float phiNormal;
  int ignoreLuminanceAtFirstIteration;
  int changingLuminancePhi;
  int useJittering;
};

struct PostProcessingUniformBufferObject {
  uint32_t displayType;
};

class VulkanApplicationContext;
class BuffersHolder {
public:
  BuffersHolder() = default;

  void init(GpuModel::RtScene *rtScene, int stratumFilterSize, int aTrousSize,
            size_t framesInFlight);

  // buffer bundles getter
  BufferBundle *getGlobalBufferBundle() { return _globalBufferBundle.get(); }
  BufferBundle *getGradientProjectionBufferBundle() {
    return _gradientProjectionBufferBundle.get();
  }
  BufferBundle *getRtxBufferBundle() { return _rtxBufferBundle.get(); }
  BufferBundle *getTemperalFilterBufferBundle() { return _temperalFilterBufferBundle.get(); }
  BufferBundle *getStratumFilterBufferBundle(int index) {
    return _stratumFilterBufferBundle[index].get();
  }
  BufferBundle *getVarianceBufferBundle() { return _varianceBufferBundle.get(); }
  BufferBundle *getBlurFilterBufferBundle(int index) {
    return _blurFilterBufferBundles[index].get();
  }
  BufferBundle *getPostProcessingBufferBundle() { return _postProcessingBufferBundle.get(); }

  BufferBundle *getTriangleBufferBundle() { return _triangleBufferBundle.get(); }
  BufferBundle *getMaterialBufferBundle() { return _materialBufferBundle.get(); }
  BufferBundle *getBvhBufferBundle() { return _bvhBufferBundle.get(); }
  BufferBundle *getLightsBufferBundle() { return _lightsBufferBundle.get(); }

  // buffers getter
  Buffer *getGlobalBuffer(size_t frameIndex) { return _globalBufferBundle->getBuffer(frameIndex); }
  Buffer *getGradientProjectionBuffer(size_t frameIndex) {
    return _gradientProjectionBufferBundle->getBuffer(frameIndex);
  }
  Buffer *getRtxBuffer(size_t frameIndex) { return _rtxBufferBundle->getBuffer(frameIndex); }
  Buffer *getTemperalFilterBuffer(size_t frameIndex) {
    return _temperalFilterBufferBundle->getBuffer(frameIndex);
  }
  Buffer *getStratumFilterBuffer(size_t frameIndex, int index) {
    return _stratumFilterBufferBundle[index]->getBuffer(frameIndex);
  }
  Buffer *getVarianceBuffer(size_t frameIndex) {
    return _varianceBufferBundle->getBuffer(frameIndex);
  }
  Buffer *getBlurFilterBuffer(size_t frameIndex, int index) {
    return _blurFilterBufferBundles[index]->getBuffer(frameIndex);
  }
  Buffer *getPostProcessingBuffer(size_t frameIndex) {
    return _postProcessingBufferBundle->getBuffer(frameIndex);
  }

  Buffer *getTriangleBuffer() { return _triangleBufferBundle->getBuffer(0); }
  Buffer *getMaterialBuffer() { return _materialBufferBundle->getBuffer(0); }
  Buffer *getBvhBuffer() { return _bvhBufferBundle->getBuffer(0); }
  Buffer *getLightsBuffer() { return _lightsBufferBundle->getBuffer(0); }

private:
  // https://www.reddit.com/r/vulkan/comments/10io2l8/is_framesinflight_fif_method_really_worth_it/
  // You seem to be saying you keep #FIF copies of all resources. Only resources that are actively
  // accessed by both the CPU and GPU need to because the GPU will not work on multiple frames at
  // the same time. You don't need to multiply textures or buffers in device memory. This means that
  // the memory increase should be small and solves your problem with uploading data.

  // buffers that are updated by CPU and sent to GPU every frame
  std::unique_ptr<BufferBundle> _globalBufferBundle;
  std::unique_ptr<BufferBundle> _gradientProjectionBufferBundle;
  std::unique_ptr<BufferBundle> _rtxBufferBundle;
  std::unique_ptr<BufferBundle> _temperalFilterBufferBundle;
  std::vector<std::unique_ptr<BufferBundle>> _stratumFilterBufferBundle;
  std::unique_ptr<BufferBundle> _varianceBufferBundle;
  std::vector<std::unique_ptr<BufferBundle>> _blurFilterBufferBundles;
  std::unique_ptr<BufferBundle> _postProcessingBufferBundle;

  // buffers that are updated by CPU and sent to GPU only once
  std::unique_ptr<BufferBundle> _triangleBufferBundle;
  std::unique_ptr<BufferBundle> _materialBufferBundle;
  std::unique_ptr<BufferBundle> _bvhBufferBundle;
  std::unique_ptr<BufferBundle> _lightsBufferBundle;

  void _createMultiBufferBundles(int stratumFilterSize, int aTrousSize, size_t framesInFlight);
  void _createSingleBufferBundles(GpuModel::RtScene *rtScene);
};