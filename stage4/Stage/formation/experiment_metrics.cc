#include "experiment_metrics.hh"

#include <algorithm>
#include <cmath>
#include <limits>

namespace formation {
namespace {

double distance(const Point2D& lhs, const Point2D& rhs) {
  return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

CompletionResult invalid_completion_result() {
  CompletionResult result = {false, std::numeric_limits<double>::infinity()};
  return result;
}

}  // namespace

CompletionResult evaluate_completion(
    const std::vector<RobotAssignment>& robots,
    const std::vector<Point2D>& targets,
    double arrival_tolerance) {
  if (targets.empty() || robots.size() != targets.size() ||
      !std::isfinite(arrival_tolerance) || arrival_tolerance < 0.0) {
    return invalid_completion_result();
  }

  std::vector<bool> assigned_targets(targets.size(), false);
  bool assignments_are_bijective = true;
  bool all_robots_arrived = true;
  double max_target_error = 0.0;

  for (std::size_t robot_index = 0; robot_index < robots.size();
       ++robot_index) {
    const RobotAssignment& robot = robots[robot_index];
    if (robot.target_index >= targets.size()) {
      return invalid_completion_result();
    }

    if (assigned_targets[robot.target_index]) {
      assignments_are_bijective = false;
    }
    assigned_targets[robot.target_index] = true;

    const double target_error =
        distance(robot.pose, targets[robot.target_index]);
    if (!std::isfinite(target_error)) {
      return invalid_completion_result();
    }
    max_target_error = std::max(max_target_error, target_error);
    if (target_error > arrival_tolerance) {
      all_robots_arrived = false;
    }
  }

  CompletionResult result = {
      assignments_are_bijective && all_robots_arrived, max_target_error};
  return result;
}

PathLengthTracker::PathLengthTracker()
    : has_previous_pose_(false),
      previous_pose_({0.0, 0.0}),
      distance_stage_units_(0.0) {}

void PathLengthTracker::observe(const Point2D& pose) {
  if (has_previous_pose_) {
    distance_stage_units_ += distance(previous_pose_, pose);
  }
  previous_pose_ = pose;
  has_previous_pose_ = true;
}

double PathLengthTracker::distance_stage_units() const {
  return distance_stage_units_;
}

double PathLengthTracker::distance_meters() const {
  return distance_stage_units_ * kMetersPerStageUnit;
}

double total_distance_meters(const std::vector<PathLengthTracker>& trackers) {
  double total = 0.0;
  for (std::size_t tracker_index = 0; tracker_index < trackers.size();
       ++tracker_index) {
    total += trackers[tracker_index].distance_meters();
  }
  return total;
}

}  // namespace formation
