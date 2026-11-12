#include "../experiment_metrics.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

void expect_near(double actual,
                 double expected,
                 double tolerance,
                 const std::string& message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "FAIL: " << message << " (expected " << expected << ", got "
              << actual << ')' << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

formation::Point2D point(double x, double y) {
  formation::Point2D result = {x, y};
  return result;
}

formation::RobotAssignment assignment(std::size_t target_index,
                                      double x,
                                      double y) {
  formation::RobotAssignment result = {target_index, point(x, y)};
  return result;
}

void test_duplicate_assignment_is_not_complete() {
  const std::vector<formation::Point2D> targets = {point(0.0, 0.0),
                                                    point(2.0, 0.0)};
  const std::vector<formation::RobotAssignment> robots = {
      assignment(0, 0.0, 0.0), assignment(0, 0.0, 0.0)};

  const formation::CompletionResult result =
      formation::evaluate_completion(robots, targets, 0.5);

  expect_true(!result.complete,
              "duplicate target assignments must not form a bijection");
}

void test_unreached_target_is_not_complete_and_reports_max_error() {
  const std::vector<formation::Point2D> targets = {point(0.0, 0.0),
                                                    point(2.0, 0.0)};
  const std::vector<formation::RobotAssignment> robots = {
      assignment(0, 0.0, 0.0), assignment(1, 2.5001, 0.0)};

  const formation::CompletionResult result =
      formation::evaluate_completion(robots, targets, 0.5);

  expect_true(!result.complete,
              "a robot outside the arrival tolerance must not be complete");
  expect_near(result.max_target_error, 0.5001, 1e-12,
              "maximum target error must include the unreached robot");
}

void test_tolerance_boundary_is_inclusive() {
  const std::vector<formation::Point2D> targets = {point(0.0, 0.0),
                                                    point(2.0, 0.0)};
  const std::vector<formation::RobotAssignment> robots = {
      assignment(1, 2.0, 0.5), assignment(0, -0.3, -0.4)};

  const formation::CompletionResult result =
      formation::evaluate_completion(robots, targets, 0.5);

  expect_true(result.complete,
              "robots exactly 0.5 Stage units away must count as arrived");
  expect_near(result.max_target_error, 0.5, 1e-12,
              "maximum error must be reported at the inclusive boundary");
}

void test_complete_bijection() {
  const std::vector<formation::Point2D> targets = {
      point(0.0, 0.0), point(2.0, 0.0), point(0.0, 3.0)};
  const std::vector<formation::RobotAssignment> robots = {
      assignment(2, 0.0, 3.1), assignment(0, 0.1, 0.0),
      assignment(1, 1.8, 0.0)};

  const formation::CompletionResult result =
      formation::evaluate_completion(robots, targets, 0.5);

  expect_true(result.complete,
              "a reached permutation of every target must be complete");
  expect_near(result.max_target_error, 0.2, 1e-12,
              "maximum target error must be computed across all robots");
}

void test_invalid_assignment_is_safe() {
  const std::vector<formation::Point2D> targets = {point(0.0, 0.0)};
  const std::vector<formation::RobotAssignment> robots = {
      assignment(1, 0.0, 0.0)};

  const formation::CompletionResult result =
      formation::evaluate_completion(robots, targets, 0.5);

  expect_true(!result.complete,
              "an out-of-range target index must not be complete");
  expect_true(std::isinf(result.max_target_error),
              "invalid target indices must report an unbounded error");
}

void test_path_tracker_accumulates_pose_displacement() {
  formation::PathLengthTracker tracker;
  tracker.observe(point(0.0, 0.0));
  tracker.observe(point(3.0, 4.0));
  tracker.observe(point(6.0, 4.0));

  expect_near(tracker.distance_stage_units(), 8.0, 1e-12,
              "tracker must sum displacement between consecutive poses");
  expect_near(tracker.distance_meters(), 0.8, 1e-12,
              "Stage distances must use 0.1 meters per Stage unit");
}

void test_global_path_sum() {
  formation::PathLengthTracker first;
  first.observe(point(0.0, 0.0));
  first.observe(point(3.0, 4.0));

  formation::PathLengthTracker second;
  second.observe(point(1.0, 1.0));
  second.observe(point(1.0, 3.0));

  const std::vector<formation::PathLengthTracker> trackers = {first, second};
  expect_near(formation::total_distance_meters(trackers), 0.7, 1e-12,
              "global distance must sum every robot path and convert units");
}

}  // namespace

int main() {
  test_duplicate_assignment_is_not_complete();
  test_unreached_target_is_not_complete_and_reports_max_error();
  test_tolerance_boundary_is_inclusive();
  test_complete_bijection();
  test_invalid_assignment_is_safe();
  test_path_tracker_accumulates_pose_displacement();
  test_global_path_sum();
  std::cout << "experiment_metrics_test: PASS" << std::endl;
  return EXIT_SUCCESS;
}
