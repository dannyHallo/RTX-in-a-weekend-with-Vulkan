#pragma once

#include "glm/glm.hpp" // IWYU pragma: export

class TomlConfigReader;

struct SvoTracerTweakingData {
  SvoTracerTweakingData(TomlConfigReader *tomlConfigReader);
  // debug parameters
  bool debugB1{};
  float debugF1{};

  // tweakable parameters
  bool visualizeChunks{};
  bool visualizeOctree{};
  bool beamOptimization{};
  bool traceIndirectRay{};
  bool taa{};

  // for env
  float sunAngleA{};
  float sunAngleB{};
  glm::vec3 rayleighScatteringBase{};
  float mieScatteringBase{};
  float mieAbsorptionBase{};
  glm::vec3 ozoneAbsorptionBase{};
  float sunLuminance{};
  float atmosLuminance{};
  float sunSize{};

  // for temporal filter info
  float temporalAlpha{};
  float temporalPositionPhi{};

  // for spatial filter info
  int aTrousIterationCount{};
  float phiC{};
  float phiN{};
  float phiP{};
  float minPhiZ{};
  float maxPhiZ{};
  float phiZStableSampleCount{};
  bool changingLuminancePhi{};

private:
  TomlConfigReader *_tomlConfigReader;

  void _loadConfig();
};
