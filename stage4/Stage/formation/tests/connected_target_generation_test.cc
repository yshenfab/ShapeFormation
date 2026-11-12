#include "../connected_target_generation.hh"

#include <cmath>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

namespace {

constexpr double kTolerance = 1e-7;

int check(const bool condition, const std::string &message)
{
  if (condition) {
    return 0;
  }
  std::cerr << "FAILED: " << message << '\n';
  return 1;
}

std::vector<formation::TargetPoint2D> grid(const double x_offset,
                                           const double y_offset,
                                           const int columns,
                                           const int rows,
                                           const double spacing)
{
  std::vector<formation::TargetPoint2D> points;
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      points.push_back(formation::TargetPoint2D{x_offset + column * spacing,
                                                y_offset + row * spacing});
    }
  }
  return points;
}

formation::ConnectedTargetConfig default_config(const std::size_t target_count)
{
  formation::ConnectedTargetConfig config;
  config.target_count = target_count;
  config.safe_distance = 2.0;
  config.communication_distance = 5.0;
  config.arena_width = 50.0;
  config.arena_height = 50.0;
  config.boundary_margin = 1.0;
  config.max_compaction_iterations = 128;
  return config;
}

double distance(const formation::TargetPoint2D &lhs, const formation::TargetPoint2D &rhs)
{
  return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

int check_common_invariants(const formation::ConnectedTargetResult &result,
                            const formation::ConnectedTargetConfig &config)
{
  int failures = 0;
  failures += check(result.targets.size() == config.target_count, "target count is not exact");
  for (std::size_t i = 0; i < result.targets.size(); ++i) {
    const auto &point = result.targets[i].position;
    failures += check(std::isfinite(point.x) && std::isfinite(point.y),
                      "target coordinate is not finite");
    failures += check(std::fabs(point.x) <= config.arena_width * 0.5 - config.boundary_margin
                                        + kTolerance
                          && std::fabs(point.y)
                                 <= config.arena_height * 0.5 - config.boundary_margin + kTolerance,
                      "target violates arena boundary margin");
    for (std::size_t j = i + 1; j < result.targets.size(); ++j) {
      failures += check(distance(point, result.targets[j].position) + kTolerance
                            >= config.safe_distance,
                        "target pair violates safe distance");
    }
  }

  if (!result.targets.empty()) {
    std::vector<bool> visited(result.targets.size(), false);
    std::queue<std::size_t> pending;
    pending.push(0);
    visited[0] = true;
    while (!pending.empty()) {
      const std::size_t current = pending.front();
      pending.pop();
      for (std::size_t neighbor = 0; neighbor < result.targets.size(); ++neighbor) {
        if (!visited[neighbor]
            && distance(result.targets[current].position, result.targets[neighbor].position)
                   <= config.communication_distance + kTolerance) {
          visited[neighbor] = true;
          pending.push(neighbor);
        }
      }
    }
    for (const bool reached : visited) {
      failures += check(reached, "target communication graph is disconnected");
    }
  }
  return failures;
}

int test_single_component()
{
  const std::vector<std::vector<formation::TargetPoint2D>> components{grid(0, 0, 3, 3, 2)};
  const auto config = default_config(9);
  const auto result = formation::generate_connected_targets(components, config);

  int failures = 0;
  failures += check(result.success, "single component should be feasible: " + result.error);
  failures += check(result.targets.size() == 9, "single component must return exact target count");
  failures += check(result.metrics.component_count == 1, "single component count changed");
  failures += check(result.metrics.graph_component_count == 1, "single component graph disconnected");
  failures += check(result.metrics.minimum_distance + kTolerance >= 2.0,
                    "single component violates safe distance");
  failures += check_common_invariants(result, config);
  return failures;
}

int test_wide_components_are_compacted_rigidly()
{
  const std::vector<std::vector<formation::TargetPoint2D>> components{
      grid(0, 0, 2, 2, 2), grid(100, 0, 2, 2, 2)};
  const auto config = default_config(8);
  const auto result = formation::generate_connected_targets(components, config);

  int failures = 0;
  failures += check(result.success, "wide components should be compacted: " + result.error);
  failures += check(result.targets.size() == 8, "compaction inserted or removed targets");
  failures += check(result.metrics.component_count == 2, "component labels were not preserved");
  failures += check(result.metrics.graph_component_count == 1, "compacted graph is disconnected");
  failures += check_common_invariants(result, config);

  for (std::size_t i = 0; i < result.targets.size(); ++i) {
    const auto &target = result.targets[i];
    bool source_is_foreground = false;
    for (const auto &candidate : components[target.component]) {
      if (distance(candidate, target.source) < kTolerance) {
        source_is_foreground = true;
        break;
      }
    }
    failures += check(source_is_foreground, "output contains a relay target not sampled from foreground");

    for (std::size_t j = i + 1; j < result.targets.size(); ++j) {
      if (result.targets[i].component != result.targets[j].component) {
        continue;
      }
      const double source_distance = distance(result.targets[i].source, result.targets[j].source);
      const double output_distance = distance(result.targets[i].position, result.targets[j].position);
      failures += check(std::fabs(output_distance - source_distance * result.metrics.scale) < 1e-6,
                        "component internal geometry changed during compaction");
    }
  }
  return failures;
}

int test_three_components_and_determinism()
{
  const std::vector<std::vector<formation::TargetPoint2D>> components{
      grid(0, 0, 1, 2, 2), grid(40, 10, 1, 2, 2), grid(90, -5, 1, 2, 2)};
  const auto first = formation::generate_connected_targets(components, default_config(6));
  const auto second = formation::generate_connected_targets(components, default_config(6));

  int failures = 0;
  failures += check(first.success, "three components should be feasible: " + first.error);
  failures += check(first.metrics.graph_component_count == 1, "three components remain disconnected");
  failures += check_common_invariants(first, default_config(6));
  failures += check(first.targets.size() == second.targets.size(), "deterministic run changed target count");
  for (std::size_t i = 0; i < first.targets.size() && i < second.targets.size(); ++i) {
    failures += check(first.targets[i].component == second.targets[i].component,
                      "deterministic run changed component labels");
    failures += check(std::fabs(first.targets[i].position.x - second.targets[i].position.x) < kTolerance
                          && std::fabs(first.targets[i].position.y - second.targets[i].position.y)
                                 < kTolerance,
                      "identical input produced different targets");
  }
  return failures;
}

int test_skewed_component_areas_keep_tiny_component_quota_at_one()
{
  std::vector<formation::TargetPoint2D> large_component = grid(100, 0, 11, 9, 2);
  const std::vector<std::vector<formation::TargetPoint2D>> components{
      std::vector<formation::TargetPoint2D>{{0.0, 0.0}}, large_component};
  auto config = default_config(100);
  config.safe_distance = 1.0;
  config.communication_distance = 100.0;
  config.arena_width = 500.0;
  config.arena_height = 500.0;
  const auto result = formation::generate_connected_targets(components, config);

  int failures = 0;
  failures += check(result.success, "skewed but feasible quotas should succeed: " + result.error);
  failures += check_common_invariants(result, config);
  std::size_t tiny_targets = 0;
  std::size_t large_targets = 0;
  for (const auto &target : result.targets) {
    tiny_targets += target.component == 0 ? 1 : 0;
    large_targets += target.component == 1 ? 1 : 0;
  }
  failures += check(tiny_targets == 1, "one-pixel component received more than one target");
  failures += check(large_targets == 99, "large component did not receive remaining targets");
  return failures;
}

int test_compaction_iteration_limit_is_reported_as_search_limit()
{
  const std::vector<double> offsets{0.0, 1.0, 101.0, 201.0, 301.0, 401.0};
  std::vector<std::vector<formation::TargetPoint2D>> components;
  for (const double offset : offsets) {
    components.push_back(std::vector<formation::TargetPoint2D>{{offset, 0.0}});
  }

  auto limited_config = default_config(components.size());
  limited_config.max_compaction_iterations = 2;
  const auto limited = formation::generate_connected_targets(components, limited_config);
  auto sufficient_config = limited_config;
  sufficient_config.max_compaction_iterations = 10;
  const auto sufficient = formation::generate_connected_targets(components, sufficient_config);
  auto automatic_config = limited_config;
  automatic_config.max_compaction_iterations = 0;
  const auto automatic = formation::generate_connected_targets(components, automatic_config);

  int failures = 0;
  failures += check(!limited.success, "deliberately limited compaction should fail");
  failures += check(limited.error.find("search limit") != std::string::npos,
                    "iteration exhaustion was incorrectly reported as mathematical infeasibility");
  failures += check(sufficient.success,
                    "same layout should succeed with a sufficient search limit: " + sufficient.error);
  failures += check(automatic.success,
                    "automatic production search limit should solve the feasible layout: "
                        + automatic.error);
  return failures;
}

int test_infeasible_inputs_fail_explicitly()
{
  int failures = 0;

  const std::vector<std::vector<formation::TargetPoint2D>> too_many_components{
      grid(0, 0, 1, 1, 1), grid(10, 0, 1, 1, 1)};
  const auto component_failure =
      formation::generate_connected_targets(too_many_components, default_config(1));
  failures += check(!component_failure.success, "component count greater than N should fail");
  failures += check(component_failure.targets.empty(), "failed generation leaked partial targets");

  const std::vector<std::vector<formation::TargetPoint2D>> insufficient_pixels{
      grid(0, 0, 1, 1, 1), grid(10, 0, 1, 1, 1)};
  const auto quota_failure =
      formation::generate_connected_targets(insufficient_pixels, default_config(5));
  failures += check(!quota_failure.success, "component quota exceeding pixels should fail");

  auto tiny_arena = default_config(2);
  tiny_arena.arena_width = 3.0;
  tiny_arena.arena_height = 3.0;
  const std::vector<std::vector<formation::TargetPoint2D>> large_rigid_component{
      std::vector<formation::TargetPoint2D>{{0.0, 0.0}, {100.0, 0.0}}};
  const auto arena_failure = formation::generate_connected_targets(large_rigid_component, tiny_arena);
  failures += check(!arena_failure.success, "rigid component that cannot fit arena should fail");
  return failures;
}

} // namespace

int main()
{
  int failures = 0;
  failures += test_single_component();
  failures += test_wide_components_are_compacted_rigidly();
  failures += test_three_components_and_determinism();
  failures += test_skewed_component_areas_keep_tiny_component_quota_at_one();
  failures += test_compaction_iteration_limit_is_reported_as_search_limit();
  failures += test_infeasible_inputs_fail_explicitly();
  return failures == 0 ? 0 : 1;
}
