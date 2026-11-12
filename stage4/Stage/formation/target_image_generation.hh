#ifndef FORMATION_TARGET_IMAGE_GENERATION_HH
#define FORMATION_TARGET_IMAGE_GENERATION_HH

#include "connected_target_generation.hh"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace formation {

struct TargetImageGenerationConfig {
  std::size_t target_count;
  double safe_distance;
  double communication_distance;
  double arena_width;
  double arena_height;
  double boundary_margin;
  double binary_threshold;

  TargetImageGenerationConfig();
};

struct TargetImageGenerationResult {
  bool success;
  std::string error;
  std::string error_code;
  int source_width;
  int source_height;
  int cropped_width;
  int cropped_height;
  std::vector<std::vector<TargetPoint2D>> component_candidates;
  std::vector<ConnectedTarget> targets;
  std::vector<std::pair<std::size_t, std::size_t>> mst_edges;
  std::vector<TargetPoint2D> component_translations;
  ConnectedTargetMetrics metrics;

  TargetImageGenerationResult();
};

TargetImageGenerationResult generate_targets_from_image(
    const std::string &image_path,
    const TargetImageGenerationConfig &config);

} // namespace formation

#endif // FORMATION_TARGET_IMAGE_GENERATION_HH
