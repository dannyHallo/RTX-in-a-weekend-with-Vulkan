#pragma once

#include "svo-ray-tracing/im-data/ImData.hpp"
// #include "svo-ray-tracing/octree/BaseLevelBuilder.hpp"
#include "svo-ray-tracing/octree/UpperLevelBuilder.hpp"

#include <array>
#include <memory>
#include <vector>

class Logger;
class SvoScene {
public:
  SvoScene(Logger *logger);

  [[nodiscard]] const std::vector<uint32_t> &getVoxelBuffer() const { return _voxelBuffer; }
  [[nodiscard]] const std::vector<uint32_t> &getPaletteBuffer() const { return _paletteBuffer; }

private:
  Logger *_logger;
  std::vector<std::unique_ptr<ImData>> _imageDatas;

  std::vector<uint32_t> _voxelBuffer;
  std::vector<uint32_t> _paletteBuffer;

  void _run();

  void _buildImageDatas();
  void _printImageDatas();
  void _createVoxelBuffer();

void _printBuffer();
};