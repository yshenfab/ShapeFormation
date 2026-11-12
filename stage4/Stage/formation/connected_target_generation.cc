#include "connected_target_generation.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>

namespace formation {
namespace {

constexpr double kTolerance = 1e-9;

struct WorkingTarget {
  TargetPoint2D position;
  TargetPoint2D source;
  std::size_t component;
};

struct MstResult {
  bool success;
  double bottleneck;
  std::vector<std::pair<std::size_t, std::size_t>> edges;
};

struct LayoutEvaluation {
  bool valid;
  bool feasible;
  double minimum_distance;
  double mst_bottleneck;
  double safety_lower;
  double communication_upper;
  double arena_upper;
  double upper;
  double infeasibility;
  double span_x;
  double span_y;
  MstResult target_mst;
};

struct ComponentEdge {
  std::size_t first;
  std::size_t second;
  std::size_t first_target;
  std::size_t second_target;
  double distance;
};

double squared_distance(const TargetPoint2D &lhs, const TargetPoint2D &rhs)
{
  const double dx = lhs.x - rhs.x;
  const double dy = lhs.y - rhs.y;
  return dx * dx + dy * dy;
}

double point_distance(const TargetPoint2D &lhs, const TargetPoint2D &rhs)
{
  return std::sqrt(squared_distance(lhs, rhs));
}

bool lexicographic_pixel_less(const TargetPoint2D &lhs, const TargetPoint2D &rhs)
{
  if (lhs.y != rhs.y) {
    return lhs.y < rhs.y;
  }
  return lhs.x < rhs.x;
}

std::vector<std::size_t> allocate_quotas(
    const std::vector<std::vector<TargetPoint2D>> &components,
    const std::size_t target_count,
    std::string &error)
{
  const std::size_t component_count = components.size();
  if (component_count == 0 || component_count > target_count) {
    error = "foreground component count exceeds target count or is zero";
    return std::vector<std::size_t>();
  }

  std::size_t total_candidates = 0;
  for (std::size_t component = 0; component < component_count; ++component) {
    if (components[component].empty()) {
      error = "foreground component contains no candidate pixels";
      return std::vector<std::size_t>();
    }
    total_candidates += components[component].size();
  }
  if (total_candidates < target_count) {
    error = "foreground has fewer distinct pixels than requested targets";
    return std::vector<std::size_t>();
  }

  std::vector<std::size_t> quotas(component_count, 1);
  std::vector<double> remainders(component_count, 0.0);
  std::size_t assigned = 0;
  for (std::size_t component = 0; component < component_count; ++component) {
    const double raw = static_cast<double>(target_count) * components[component].size()
                       / static_cast<double>(total_candidates);
    const std::size_t proportional = static_cast<std::size_t>(std::floor(raw));
    quotas[component] = std::max<std::size_t>(1, proportional);
    assigned += quotas[component];
    remainders[component] = raw - static_cast<double>(proportional);
  }

  std::vector<std::size_t> order(component_count, 0);
  for (std::size_t component = 0; component < component_count; ++component) {
    order[component] = component;
  }
  std::sort(order.begin(), order.end(), [&](const std::size_t lhs, const std::size_t rhs) {
    if (std::fabs(remainders[lhs] - remainders[rhs]) > kTolerance) {
      return remainders[lhs] > remainders[rhs];
    }
    return lhs < rhs;
  });
  for (std::size_t index = 0; assigned < target_count; ++index) {
    const std::size_t component = order[index % component_count];
    if (quotas[component] < components[component].size()) {
      ++quotas[component];
      ++assigned;
    }
  }
  while (assigned > target_count) {
    std::size_t component_to_reduce = component_count;
    for (std::size_t component = 0; component < component_count; ++component) {
      if (quotas[component] <= 1) {
        continue;
      }
      if (component_to_reduce == component_count
          || remainders[component] < remainders[component_to_reduce] - kTolerance
          || (std::fabs(remainders[component] - remainders[component_to_reduce]) <= kTolerance
              && component > component_to_reduce)) {
        component_to_reduce = component;
      }
    }
    if (component_to_reduce == component_count) {
      error = "minimum-one component quotas exceed target count";
      return std::vector<std::size_t>();
    }
    --quotas[component_to_reduce];
    --assigned;
  }

  for (std::size_t component = 0; component < component_count; ++component) {
    if (quotas[component] > components[component].size()) {
      std::ostringstream message;
      message << "component " << component << " quota " << quotas[component]
              << " exceeds its " << components[component].size() << " candidate pixels";
      error = message.str();
      return std::vector<std::size_t>();
    }
  }
  return quotas;
}

std::vector<TargetPoint2D> farthest_point_sample(std::vector<TargetPoint2D> candidates,
                                                 const std::size_t quota)
{
  std::sort(candidates.begin(), candidates.end(), lexicographic_pixel_less);
  double centroid_x = 0.0;
  double centroid_y = 0.0;
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    centroid_x += candidates[index].x;
    centroid_y += candidates[index].y;
  }
  centroid_x /= static_cast<double>(candidates.size());
  centroid_y /= static_cast<double>(candidates.size());

  std::size_t seed = 0;
  double seed_distance = std::numeric_limits<double>::infinity();
  const TargetPoint2D centroid(centroid_x, centroid_y);
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const double candidate_distance = squared_distance(candidates[index], centroid);
    if (candidate_distance + kTolerance < seed_distance) {
      seed = index;
      seed_distance = candidate_distance;
    }
  }

  std::vector<TargetPoint2D> selected;
  selected.reserve(quota);
  selected.push_back(candidates[seed]);
  std::vector<double> minimum_squared_distance(candidates.size(),
                                                std::numeric_limits<double>::infinity());
  std::vector<bool> used(candidates.size(), false);
  used[seed] = true;

  while (selected.size() < quota) {
    const TargetPoint2D &newest = selected.back();
    std::size_t best = candidates.size();
    double best_distance = -1.0;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
      if (used[index]) {
        continue;
      }
      minimum_squared_distance[index] =
          std::min(minimum_squared_distance[index], squared_distance(candidates[index], newest));
      if (minimum_squared_distance[index] > best_distance + kTolerance) {
        best = index;
        best_distance = minimum_squared_distance[index];
      }
    }
    if (best == candidates.size()) {
      return std::vector<TargetPoint2D>();
    }
    used[best] = true;
    selected.push_back(candidates[best]);
  }
  return selected;
}

MstResult compute_target_mst(const std::vector<TargetPoint2D> &points)
{
  MstResult result{false, 0.0, std::vector<std::pair<std::size_t, std::size_t>>()};
  if (points.empty()) {
    return result;
  }
  if (points.size() == 1) {
    result.success = true;
    return result;
  }

  const std::size_t count = points.size();
  std::vector<double> best(count, std::numeric_limits<double>::infinity());
  std::vector<std::size_t> parent(count, count);
  std::vector<bool> in_tree(count, false);
  best[0] = 0.0;

  for (std::size_t iteration = 0; iteration < count; ++iteration) {
    std::size_t current = count;
    for (std::size_t candidate = 0; candidate < count; ++candidate) {
      if (!in_tree[candidate]
          && (current == count || best[candidate] < best[current] - kTolerance
              || (std::fabs(best[candidate] - best[current]) <= kTolerance
                  && candidate < current))) {
        current = candidate;
      }
    }
    if (current == count || !std::isfinite(best[current])) {
      return result;
    }
    in_tree[current] = true;
    if (parent[current] != count) {
      result.edges.push_back(std::make_pair(parent[current], current));
      result.bottleneck = std::max(result.bottleneck, best[current]);
    }
    for (std::size_t candidate = 0; candidate < count; ++candidate) {
      if (in_tree[candidate]) {
        continue;
      }
      const double distance = point_distance(points[current], points[candidate]);
      if (distance < best[candidate] - kTolerance
          || (std::fabs(distance - best[candidate]) <= kTolerance
              && current < parent[candidate])) {
        best[candidate] = distance;
        parent[candidate] = current;
      }
    }
  }
  result.success = result.edges.size() + 1 == count;
  return result;
}

double fit_scale(const double usable_span, const double point_span)
{
  if (point_span <= kTolerance) {
    return std::numeric_limits<double>::infinity();
  }
  return usable_span / point_span;
}

LayoutEvaluation evaluate_layout(const std::vector<WorkingTarget> &targets,
                                 const ConnectedTargetConfig &config)
{
  LayoutEvaluation result;
  result.valid = false;
  result.feasible = false;
  result.minimum_distance = std::numeric_limits<double>::infinity();
  result.mst_bottleneck = 0.0;
  result.safety_lower = 0.0;
  result.communication_upper = std::numeric_limits<double>::infinity();
  result.arena_upper = std::numeric_limits<double>::infinity();
  result.upper = std::numeric_limits<double>::infinity();
  result.infeasibility = std::numeric_limits<double>::infinity();
  result.span_x = 0.0;
  result.span_y = 0.0;

  std::vector<TargetPoint2D> points;
  points.reserve(targets.size());
  for (std::size_t index = 0; index < targets.size(); ++index) {
    points.push_back(targets[index].position);
  }
  result.target_mst = compute_target_mst(points);
  if (!result.target_mst.success) {
    return result;
  }
  result.mst_bottleneck = result.target_mst.bottleneck;

  double min_x = points.front().x;
  double max_x = points.front().x;
  double min_y = points.front().y;
  double max_y = points.front().y;
  for (std::size_t i = 0; i < points.size(); ++i) {
    min_x = std::min(min_x, points[i].x);
    max_x = std::max(max_x, points[i].x);
    min_y = std::min(min_y, points[i].y);
    max_y = std::max(max_y, points[i].y);
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      result.minimum_distance =
          std::min(result.minimum_distance, point_distance(points[i], points[j]));
    }
  }
  result.span_x = max_x - min_x;
  result.span_y = max_y - min_y;
  if (points.size() > 1 && result.minimum_distance <= kTolerance) {
    return result;
  }

  if (points.size() > 1) {
    result.safety_lower = config.safe_distance / result.minimum_distance;
    result.communication_upper = config.communication_distance / result.mst_bottleneck;
  }
  const double usable_width = config.arena_width - 2.0 * config.boundary_margin;
  const double usable_height = config.arena_height - 2.0 * config.boundary_margin;
  result.arena_upper = std::min(fit_scale(usable_width, result.span_x),
                                fit_scale(usable_height, result.span_y));
  result.upper = std::min(result.communication_upper, result.arena_upper);
  if (points.size() == 1 && !std::isfinite(result.upper)) {
    result.upper = 1.0;
  }
  result.valid = result.upper > 0.0 && std::isfinite(result.upper);
  result.infeasibility = std::max(0.0, result.safety_lower - result.upper);
  result.feasible = result.valid && result.infeasibility <= kTolerance;
  return result;
}

std::vector<ComponentEdge> compute_component_mst(const std::vector<WorkingTarget> &targets,
                                                 const std::size_t component_count)
{
  std::vector<std::vector<ComponentEdge>> edges(
      component_count, std::vector<ComponentEdge>(component_count));
  for (std::size_t first = 0; first < component_count; ++first) {
    for (std::size_t second = first + 1; second < component_count; ++second) {
      ComponentEdge edge{first, second, targets.size(), targets.size(),
                         std::numeric_limits<double>::infinity()};
      for (std::size_t i = 0; i < targets.size(); ++i) {
        if (targets[i].component != first) {
          continue;
        }
        for (std::size_t j = 0; j < targets.size(); ++j) {
          if (targets[j].component != second) {
            continue;
          }
          const double distance = point_distance(targets[i].position, targets[j].position);
          if (distance < edge.distance - kTolerance
              || (std::fabs(distance - edge.distance) <= kTolerance
                  && std::make_pair(i, j) < std::make_pair(edge.first_target, edge.second_target))) {
            edge.distance = distance;
            edge.first_target = i;
            edge.second_target = j;
          }
        }
      }
      edges[first][second] = edge;
      edges[second][first] =
          ComponentEdge{second, first, edge.second_target, edge.first_target, edge.distance};
    }
  }

  std::vector<ComponentEdge> mst;
  std::vector<double> best(component_count, std::numeric_limits<double>::infinity());
  std::vector<ComponentEdge> parent_edge(component_count);
  std::vector<bool> in_tree(component_count, false);
  best[0] = 0.0;
  for (std::size_t iteration = 0; iteration < component_count; ++iteration) {
    std::size_t current = component_count;
    for (std::size_t candidate = 0; candidate < component_count; ++candidate) {
      if (!in_tree[candidate]
          && (current == component_count || best[candidate] < best[current] - kTolerance
              || (std::fabs(best[candidate] - best[current]) <= kTolerance
                  && candidate < current))) {
        current = candidate;
      }
    }
    if (current == component_count) {
      return std::vector<ComponentEdge>();
    }
    in_tree[current] = true;
    if (iteration > 0) {
      mst.push_back(parent_edge[current]);
    }
    for (std::size_t candidate = 0; candidate < component_count; ++candidate) {
      if (!in_tree[candidate]
          && (edges[current][candidate].distance < best[candidate] - kTolerance
              || (std::fabs(edges[current][candidate].distance - best[candidate]) <= kTolerance
                  && current < parent_edge[candidate].first))) {
        best[candidate] = edges[current][candidate].distance;
        parent_edge[candidate] = edges[current][candidate];
      }
    }
  }
  return mst;
}

std::vector<std::size_t> component_subtree(const std::vector<ComponentEdge> &mst,
                                           const std::size_t component_count,
                                           const ComponentEdge &removed,
                                           const std::size_t seed)
{
  std::vector<std::vector<std::size_t>> adjacency(component_count);
  for (std::size_t index = 0; index < mst.size(); ++index) {
    const ComponentEdge &edge = mst[index];
    const bool is_removed =
        (edge.first == removed.first && edge.second == removed.second)
        || (edge.first == removed.second && edge.second == removed.first);
    if (!is_removed) {
      adjacency[edge.first].push_back(edge.second);
      adjacency[edge.second].push_back(edge.first);
    }
  }
  std::vector<std::size_t> result;
  std::vector<bool> visited(component_count, false);
  std::queue<std::size_t> pending;
  pending.push(seed);
  visited[seed] = true;
  while (!pending.empty()) {
    const std::size_t current = pending.front();
    pending.pop();
    result.push_back(current);
    for (std::size_t index = 0; index < adjacency[current].size(); ++index) {
      const std::size_t neighbor = adjacency[current][index];
      if (!visited[neighbor]) {
        visited[neighbor] = true;
        pending.push(neighbor);
      }
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

bool contains_component(const std::vector<std::size_t> &components, const std::size_t component)
{
  return std::binary_search(components.begin(), components.end(), component);
}

double first_cross_collision(const std::vector<WorkingTarget> &targets,
                             const std::vector<std::size_t> &moving_components,
                             const TargetPoint2D &direction,
                             const double minimum_separation)
{
  double collision = std::numeric_limits<double>::infinity();
  const double minimum_squared = minimum_separation * minimum_separation;
  for (std::size_t moving = 0; moving < targets.size(); ++moving) {
    if (!contains_component(moving_components, targets[moving].component)) {
      continue;
    }
    for (std::size_t fixed = 0; fixed < targets.size(); ++fixed) {
      if (contains_component(moving_components, targets[fixed].component)) {
        continue;
      }
      const double rx = targets[moving].position.x - targets[fixed].position.x;
      const double ry = targets[moving].position.y - targets[fixed].position.y;
      const double projection = rx * direction.x + ry * direction.y;
      const double c = rx * rx + ry * ry - minimum_squared;
      const double discriminant = projection * projection - c;
      if (projection < 0.0 && discriminant >= -kTolerance) {
        const double entry = -projection - std::sqrt(std::max(0.0, discriminant));
        if (entry >= -kTolerance) {
          collision = std::min(collision, std::max(0.0, entry));
        }
      }
    }
  }
  return collision;
}

std::vector<WorkingTarget> translated_targets(const std::vector<WorkingTarget> &targets,
                                              const std::vector<std::size_t> &moving_components,
                                              const TargetPoint2D &translation)
{
  std::vector<WorkingTarget> translated = targets;
  for (std::size_t index = 0; index < translated.size(); ++index) {
    if (contains_component(moving_components, translated[index].component)) {
      translated[index].position.x += translation.x;
      translated[index].position.y += translation.y;
    }
  }
  return translated;
}

double internal_minimum_distance(const std::vector<WorkingTarget> &targets)
{
  double minimum = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < targets.size(); ++i) {
    for (std::size_t j = i + 1; j < targets.size(); ++j) {
      if (targets[i].component == targets[j].component) {
        minimum = std::min(minimum, point_distance(targets[i].position, targets[j].position));
      }
    }
  }
  return std::isfinite(minimum) ? minimum : 1.0;
}

bool better_layout(const LayoutEvaluation &candidate, const LayoutEvaluation &current)
{
  if (!candidate.valid) {
    return false;
  }
  if (candidate.infeasibility < current.infeasibility - kTolerance) {
    return true;
  }
  if (std::fabs(candidate.infeasibility - current.infeasibility) <= kTolerance
      && candidate.mst_bottleneck < current.mst_bottleneck - kTolerance) {
    return true;
  }
  const double candidate_span = candidate.span_x + candidate.span_y;
  const double current_span = current.span_x + current.span_y;
  return std::fabs(candidate.infeasibility - current.infeasibility) <= kTolerance
         && std::fabs(candidate.mst_bottleneck - current.mst_bottleneck) <= kTolerance
         && candidate_span < current_span - kTolerance;
}

std::size_t graph_component_count(const std::vector<TargetPoint2D> &points,
                                  const double communication_distance)
{
  std::size_t components = 0;
  std::vector<bool> visited(points.size(), false);
  for (std::size_t seed = 0; seed < points.size(); ++seed) {
    if (visited[seed]) {
      continue;
    }
    ++components;
    std::queue<std::size_t> pending;
    pending.push(seed);
    visited[seed] = true;
    while (!pending.empty()) {
      const std::size_t current = pending.front();
      pending.pop();
      for (std::size_t neighbor = 0; neighbor < points.size(); ++neighbor) {
        if (!visited[neighbor]
            && point_distance(points[current], points[neighbor])
                   <= communication_distance + kTolerance) {
          visited[neighbor] = true;
          pending.push(neighbor);
        }
      }
    }
  }
  return components;
}

ConnectedTargetResult failure(const std::string &message,
                              const std::size_t component_count = 0)
{
  ConnectedTargetResult result;
  result.error = message;
  result.metrics.component_count = component_count;
  return result;
}

} // namespace

ConnectedTargetConfig::ConnectedTargetConfig()
    : target_count(0)
    , safe_distance(0.0)
    , communication_distance(0.0)
    , arena_width(0.0)
    , arena_height(0.0)
    , boundary_margin(0.0)
    , max_compaction_iterations(0)
{
}

ConnectedTargetMetrics::ConnectedTargetMetrics()
    : component_count(0)
    , graph_component_count(0)
    , minimum_distance(0.0)
    , mst_bottleneck(0.0)
    , scale(0.0)
    , safety_scale_lower_bound(0.0)
    , communication_scale_upper_bound(0.0)
    , arena_scale_upper_bound(0.0)
{
}

ConnectedTargetResult::ConnectedTargetResult() : success(false) {}

ConnectedTargetResult generate_connected_targets(
    const std::vector<std::vector<TargetPoint2D>> &component_candidates,
    const ConnectedTargetConfig &config)
{
  if (config.target_count == 0 || config.safe_distance <= 0.0
      || config.communication_distance <= 0.0 || config.arena_width <= 0.0
      || config.arena_height <= 0.0 || config.boundary_margin < 0.0
      || config.arena_width <= 2.0 * config.boundary_margin
      || config.arena_height <= 2.0 * config.boundary_margin
      || (config.target_count > 1
          && config.safe_distance > config.communication_distance + kTolerance)) {
    return failure("invalid target-generation limits", component_candidates.size());
  }

  std::string quota_error;
  const std::vector<std::size_t> quotas =
      allocate_quotas(component_candidates, config.target_count, quota_error);
  if (quotas.empty()) {
    return failure(quota_error, component_candidates.size());
  }

  std::vector<WorkingTarget> working;
  working.reserve(config.target_count);
  for (std::size_t component = 0; component < component_candidates.size(); ++component) {
    const std::vector<TargetPoint2D> sampled =
        farthest_point_sample(component_candidates[component], quotas[component]);
    if (sampled.size() != quotas[component]) {
      return failure("deterministic farthest-point sampling failed", component_candidates.size());
    }
    for (std::size_t index = 0; index < sampled.size(); ++index) {
      working.push_back(WorkingTarget{sampled[index], sampled[index], component});
    }
  }

  LayoutEvaluation layout = evaluate_layout(working, config);
  if (!layout.valid) {
    return failure("initial sampled layout is invalid", component_candidates.size());
  }
  const double minimum_component_separation =
      working.size() > 1 ? layout.minimum_distance : internal_minimum_distance(working);
  if (!std::isfinite(minimum_component_separation)
      || minimum_component_separation <= kTolerance) {
    return failure("sampled targets are not distinct", component_candidates.size());
  }

  const std::size_t compaction_limit =
      config.max_compaction_iterations > 0
          ? config.max_compaction_iterations
          : std::max<std::size_t>(128, 4 * component_candidates.size());
  std::size_t iteration = 0;
  while (!layout.feasible && component_candidates.size() > 1
         && iteration < compaction_limit) {
    ++iteration;
    const std::vector<ComponentEdge> component_mst =
        compute_component_mst(working, component_candidates.size());
    if (component_mst.size() + 1 != component_candidates.size()) {
      return failure("component MST construction failed", component_candidates.size());
    }

    std::vector<WorkingTarget> best_targets;
    LayoutEvaluation best_layout = layout;
    bool found_progress = false;
    std::vector<ComponentEdge> ordered_edges = component_mst;
    std::sort(ordered_edges.begin(), ordered_edges.end(), [](const ComponentEdge &lhs,
                                                             const ComponentEdge &rhs) {
      if (std::fabs(lhs.distance - rhs.distance) > kTolerance) {
        return lhs.distance > rhs.distance;
      }
      return std::make_pair(lhs.first, lhs.second) < std::make_pair(rhs.first, rhs.second);
    });

    for (std::size_t edge_index = 0; edge_index < ordered_edges.size(); ++edge_index) {
      const ComponentEdge &edge = ordered_edges[edge_index];
      const std::vector<std::size_t> second_side =
          component_subtree(component_mst, component_candidates.size(), edge, edge.second);
      std::vector<std::size_t> first_side;
      for (std::size_t component = 0; component < component_candidates.size(); ++component) {
        if (!contains_component(second_side, component)) {
          first_side.push_back(component);
        }
      }

      const std::vector<std::vector<std::size_t>> sides{second_side, first_side};
      for (std::size_t side_index = 0; side_index < sides.size(); ++side_index) {
        const std::vector<std::size_t> &moving_components = sides[side_index];
        const bool moving_second = contains_component(moving_components, edge.second);
        const std::size_t moving_target = moving_second ? edge.second_target : edge.first_target;
        const std::size_t fixed_target = moving_second ? edge.first_target : edge.second_target;
        const double dx = working[fixed_target].position.x - working[moving_target].position.x;
        const double dy = working[fixed_target].position.y - working[moving_target].position.y;
        const double length = std::hypot(dx, dy);
        if (length <= minimum_component_separation + kTolerance) {
          continue;
        }
        const TargetPoint2D direction(dx / length, dy / length);
        const double desired_translation = length - minimum_component_separation;
        const double collision_limit = first_cross_collision(
            working, moving_components, direction, minimum_component_separation);
        const double translation_length = std::min(desired_translation, collision_limit);
        if (translation_length <= kTolerance || !std::isfinite(translation_length)) {
          continue;
        }
        const TargetPoint2D translation(direction.x * translation_length,
                                        direction.y * translation_length);
        const std::vector<WorkingTarget> candidate_targets =
            translated_targets(working, moving_components, translation);
        const LayoutEvaluation candidate_layout = evaluate_layout(candidate_targets, config);
        if (better_layout(candidate_layout, best_layout)) {
          best_targets = candidate_targets;
          best_layout = candidate_layout;
          found_progress = true;
        }
      }
    }
    if (!found_progress) {
      break;
    }
    working.swap(best_targets);
    layout = best_layout;
  }

  if (!layout.feasible) {
    std::ostringstream message;
    if (iteration >= compaction_limit) {
      message << "component compaction reached its search limit (" << compaction_limit
              << ") before finding a feasible scale interval: lower=" << layout.safety_lower
              << ", upper=" << layout.upper;
    } else {
      message << "no collision-safe rigid translation found by the component-MST search: lower="
              << layout.safety_lower << ", upper=" << layout.upper;
    }
    return failure(message.str(), component_candidates.size());
  }

  const double scale = layout.upper;
  double min_x = working.front().position.x;
  double max_x = working.front().position.x;
  double min_y = working.front().position.y;
  double max_y = working.front().position.y;
  for (std::size_t index = 1; index < working.size(); ++index) {
    min_x = std::min(min_x, working[index].position.x);
    max_x = std::max(max_x, working[index].position.x);
    min_y = std::min(min_y, working[index].position.y);
    max_y = std::max(max_y, working[index].position.y);
  }
  const double center_x = 0.5 * (min_x + max_x);
  const double center_y = 0.5 * (min_y + max_y);

  ConnectedTargetResult result;
  result.metrics.component_count = component_candidates.size();
  result.metrics.scale = scale;
  result.metrics.safety_scale_lower_bound = layout.safety_lower;
  result.metrics.communication_scale_upper_bound = layout.communication_upper;
  result.metrics.arena_scale_upper_bound = layout.arena_upper;
  result.component_translations.assign(component_candidates.size(), TargetPoint2D());
  std::vector<bool> translation_set(component_candidates.size(), false);
  std::vector<TargetPoint2D> world_points;
  world_points.reserve(working.size());
  for (std::size_t index = 0; index < working.size(); ++index) {
    const TargetPoint2D world((working[index].position.x - center_x) * scale,
                              -(working[index].position.y - center_y) * scale);
    result.targets.push_back(ConnectedTarget{world, working[index].source, working[index].component});
    world_points.push_back(world);
    if (!translation_set[working[index].component]) {
      result.component_translations[working[index].component] =
          TargetPoint2D(working[index].position.x - working[index].source.x,
                        working[index].position.y - working[index].source.y);
      translation_set[working[index].component] = true;
    }
  }

  double final_minimum = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < world_points.size(); ++i) {
    if (!std::isfinite(world_points[i].x) || !std::isfinite(world_points[i].y)
        || std::fabs(world_points[i].x) > config.arena_width * 0.5 - config.boundary_margin
                                         + kTolerance
        || std::fabs(world_points[i].y) > config.arena_height * 0.5 - config.boundary_margin
                                         + kTolerance) {
      return failure("final target violates finiteness or arena bounds", component_candidates.size());
    }
    for (std::size_t j = i + 1; j < world_points.size(); ++j) {
      final_minimum = std::min(final_minimum, point_distance(world_points[i], world_points[j]));
    }
  }
  if (world_points.size() == 1) {
    final_minimum = std::numeric_limits<double>::infinity();
  }
  if (world_points.size() > 1 && final_minimum + kTolerance < config.safe_distance) {
    return failure("final target pair violates safe distance", component_candidates.size());
  }
  const MstResult final_mst = compute_target_mst(world_points);
  const std::size_t final_components =
      graph_component_count(world_points, config.communication_distance);
  if (!final_mst.success || final_components != 1) {
    return failure("final target communication graph is disconnected", component_candidates.size());
  }

  result.metrics.minimum_distance = final_minimum;
  result.metrics.mst_bottleneck = final_mst.bottleneck;
  result.metrics.graph_component_count = final_components;
  result.mst_edges = final_mst.edges;
  result.success = true;
  return result;
}

} // namespace formation
