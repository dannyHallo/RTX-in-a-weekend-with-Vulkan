#include "BaseLevelBuilder.hpp"

#include <vector>

namespace BaseLevelBuilder {

namespace {
void _buildVoxel(ImageData *imageData, ImCoor3D const &coor) {
  uint32_t constexpr kAllChilds = 0x0000FFFF;
  imageData->imageStore(coor, kAllChilds);
}

void _buildPlane(ImageData *imageData, ImCoor3D const &imageSize, int const y) {
  for (int x = 0; x < imageSize.x; ++x) {
    for (int z = 0; z < imageSize.z; ++z) {
      _buildVoxel(imageData, {x, y, z});
    }
  }
}
} // namespace

const std::vector<ImCoor3D> kBuildVoxelCoors{
    {0, 0, 0}, {0, 0, 7}, {0, 7, 0}, {0, 7, 7}, {7, 0, 0}, {7, 0, 7}, {7, 7, 0}, {7, 7, 7},
};

void build(ImageData *imageData, ImCoor3D const &imageSize) {
  _buildPlane(imageData, imageSize, 1); // bottom plane
  // for (auto const &coor : kBuildVoxelCoors) {
  //   _buildVoxel(imageData, coor);
  // }
}

} // namespace BaseLevelBuilder
