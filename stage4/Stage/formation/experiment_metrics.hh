#ifndef FORMATION_EXPERIMENT_METRICS_HH
#define FORMATION_EXPERIMENT_METRICS_HH

#include <cstddef>
#include <vector>

namespace formation {

constexpr double kMetersPerStageUnit = 0.1;

struct Point2D {
  double x;
  double y;
};

struct RobotAssignment {
  std::size_t target_index;
  Point2D pose;
};

struct CompletionResult {
  bool complete;
  double max_target_error;
};

// Completion requires exactly one robot per target and every robot no farther
// than arrival_tolerance from its currently assigned target.
CompletionResult evaluate_completion(
    const std::vector<RobotAssignment>& robots,
    const std::vector<Point2D>& targets,
    double arrival_tolerance);

class PathLengthTracker {
 public:
  PathLengthTracker();

  // The first observation establishes the starting pose and adds no distance.
  void observe(const Point2D& pose);

  double distance_stage_units() const;
  double distance_meters() const;

 private:
  bool has_previous_pose_;
  Point2D previous_pose_;
  double distance_stage_units_;
};

double total_distance_meters(const std::vector<PathLengthTracker>& trackers);

}  // namespace formation

#endif  // FORMATION_EXPERIMENT_METRICS_HH
