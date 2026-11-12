#ifndef FORMATION_CONNECTED_TARGET_GENERATION_HH
#define FORMATION_CONNECTED_TARGET_GENERATION_HH

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace formation {

struct TargetPoint2D {
  double x;
  double y;

  TargetPoint2D(double x_value = 0.0, double y_value = 0.0) : x(x_value), y(y_value) {}
};

struct ConnectedTargetConfig {
  std::size_t target_count;
  double safe_distance;
  double communication_distance;
  double arena_width;
  double arena_height;
  double boundary_margin;
  std::size_t max_compaction_iterations; // zero selects a component-count-based limit

  ConnectedTargetConfig();
};

struct ConnectedTarget {
  TargetPoint2D position;
  TargetPoint2D source;
  std::size_t component;
};

struct ConnectedTargetMetrics {
  std::size_t component_count;
  std::size_t graph_component_count;
  double minimum_distance;
  double mst_bottleneck;
  double scale;
  double safety_scale_lower_bound;
  double communication_scale_upper_bound;
  double arena_scale_upper_bound;

  ConnectedTargetMetrics();
};

struct ConnectedTargetResult {
  bool success;
  std::string error;
  std::vector<ConnectedTarget> targets;
  std::vector<std::pair<std::size_t, std::size_t>> mst_edges;
  std::vector<TargetPoint2D> component_translations;
  ConnectedTargetMetrics metrics;

  ConnectedTargetResult();
};

ConnectedTargetResult generate_connected_targets(
    const std::vector<std::vector<TargetPoint2D>> &component_candidates,
    const ConnectedTargetConfig &config);

} // namespace formation

#endif // FORMATION_CONNECTED_TARGET_GENERATION_HH
