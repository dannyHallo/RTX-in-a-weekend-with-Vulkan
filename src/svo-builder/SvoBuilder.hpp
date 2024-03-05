#pragma once

#include "scheduler/Scheduler.hpp"
#include "volk/volk.h"

#include "utils/incl/Glm.hpp" // IWYU pragma: export

#include <memory>

struct VoxData;

class DescriptorSetBundle;
class ComputePipeline;
class Logger;
class VulkanApplicationContext;
class Buffer;
class Image;
class ShaderChangeListener;

class SvoBuilder : public Scheduler {
public:
  SvoBuilder(VulkanApplicationContext *appContext, Logger *logger,
             ShaderChangeListener *shaderChangeListener);
  ~SvoBuilder() override;

  // disable copy and move
  SvoBuilder(SvoBuilder const &)            = delete;
  SvoBuilder(SvoBuilder &&)                 = delete;
  SvoBuilder &operator=(SvoBuilder const &) = delete;
  SvoBuilder &operator=(SvoBuilder &&)      = delete;

  void init();
  void update() override;

  void buildScene();

  Buffer *getOctreeBuffer() { return _octreeBuffer.get(); }
  Image *getChunksImage() { return _chunksImage.get(); }

  [[nodiscard]] uint32_t getVoxelLevelCount() const { return _voxelLevelCount; }
  [[nodiscard]] static glm::uvec3 getChunksDim();

private:
  VulkanApplicationContext *_appContext;
  Logger *_logger;
  ShaderChangeListener *_shaderChangeListener;

  uint32_t _voxelLevelCount = 0;

  std::unique_ptr<DescriptorSetBundle> _descriptorSetBundle;

  VkFence _timelineFence         = VK_NULL_HANDLE;
  VkCommandBuffer _commandBuffer = VK_NULL_HANDLE;

  void _recordCommandBuffer();

  void _buildChunk(glm::uvec3 currentlyWritingChunk);

  void _createFence();

  /// IMAGES
  std::unique_ptr<Image> _chunkFieldImage;
  std::unique_ptr<Image> _chunksImage;

  void _createImages();

  /// BUFFERS
  std::unique_ptr<Buffer> _chunksInfoBuffer;
  std::unique_ptr<Buffer> _octreeBufferUsedSizeInfoBuffer;

  std::unique_ptr<Buffer> _indirectFragLengthBuffer;
  std::unique_ptr<Buffer> _counterBuffer;
  std::unique_ptr<Buffer> _octreeBuffer;
  std::unique_ptr<Buffer> _fragmentListBuffer;
  std::unique_ptr<Buffer> _buildInfoBuffer;
  std::unique_ptr<Buffer> _indirectAllocNumBuffer;
  std::unique_ptr<Buffer> _fragmentListInfoBuffer;

  std::unique_ptr<Buffer> _octreeBufferUsedSizeInfoStagingBuffer; // for showing data on CPU side

  void _createBuffers();
  void _resetBufferDataForOctreeGeneration();
  void _resetBufferDataForNewChunkGeneration(glm::uvec3 currentlyWritingChunk);

  /// PIPELINES

  std::unique_ptr<ComputePipeline> _chunksBuilderPipeline;

  std::unique_ptr<ComputePipeline> _chunkFieldConstructionPipeline;
  std::unique_ptr<ComputePipeline> _chunkVoxelCreationPipeline;
  std::unique_ptr<ComputePipeline> _chunkModifyArgPipeline;

  std::unique_ptr<ComputePipeline> _initNodePipeline;
  std::unique_ptr<ComputePipeline> _tagNodePipeline;
  std::unique_ptr<ComputePipeline> _allocNodePipeline;
  std::unique_ptr<ComputePipeline> _modifyArgPipeline;
  std::unique_ptr<ComputePipeline> _lastModifyArgPipeline;

  void _createDescriptorSetBundle();
  void _createPipelines();
};