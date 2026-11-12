#include "../target_image_generation.hh"

#include <cstdio>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

#ifndef FORMATION_TEST_IMAGE_PATH
#error "FORMATION_TEST_IMAGE_PATH must point to a checked-in target image"
#endif

namespace {

int check(const bool condition, const std::string &message)
{
  if (condition) {
    return 0;
  }
  std::cerr << "FAILED: " << message << '\n';
  return 1;
}

formation::TargetImageGenerationConfig valid_config()
{
  formation::TargetImageGenerationConfig config;
  config.target_count = 50;
  config.safe_distance = 2.625;
  config.communication_distance = 11.4;
  config.arena_width = 50.0;
  config.arena_height = 50.0;
  config.boundary_margin = 1.0;
  return config;
}

int test_invalid_config_fails_before_image_loading()
{
  formation::TargetImageGenerationConfig config = valid_config();
  config.target_count = 0;
  const formation::TargetImageGenerationResult result =
      formation::generate_targets_from_image(FORMATION_TEST_IMAGE_PATH, config);

  int failures = 0;
  failures += check(!result.success, "zero targets must fail");
  failures += check(result.error_code == "invalid_argument",
                    "invalid configuration must have a stable error code");
  return failures;
}

int test_missing_image_fails_explicitly()
{
  const formation::TargetImageGenerationResult result =
      formation::generate_targets_from_image("/definitely/not/a/target-image.png", valid_config());

  int failures = 0;
  failures += check(!result.success, "missing image must fail");
  failures += check(result.error_code == "image_read_failed",
                    "missing image must be distinguished from an infeasible layout");
  return failures;
}

int test_blank_image_has_no_foreground()
{
  const std::string image_path = "/tmp/formation_target_image_blank_test.pgm";
  {
    std::ofstream image(image_path.c_str(), std::ios::trunc);
    image << "P2\n2 2\n255\n255 255\n255 255\n";
  }
  const formation::TargetImageGenerationResult result =
      formation::generate_targets_from_image(image_path, valid_config());
  std::remove(image_path.c_str());

  int failures = 0;
  failures += check(!result.success, "blank image must fail");
  failures += check(result.error_code == "no_foreground",
                    "blank image must report that it has no foreground");
  return failures;
}

int test_checked_in_image_generates_connected_targets()
{
  const formation::TargetImageGenerationConfig config = valid_config();
  const formation::TargetImageGenerationResult result =
      formation::generate_targets_from_image(FORMATION_TEST_IMAGE_PATH, config);

  int failures = 0;
  failures += check(result.success, "checked-in image should qualify: " + result.error);
  failures += check(result.targets.size() == config.target_count,
                    "successful image generation must return exactly N targets");
  failures += check(result.metrics.component_count == result.component_candidates.size(),
                    "component diagnostics and metrics disagree");
  failures += check(result.metrics.graph_component_count == 1,
                    "final communication graph must be connected");
  failures += check(result.metrics.minimum_distance + 1e-8 >= config.safe_distance,
                    "target minimum distance violates the safety threshold");
  failures += check(result.metrics.mst_bottleneck <= config.communication_distance + 1e-8,
                    "MST bottleneck violates the communication threshold");
  for (std::size_t index = 0; index < result.targets.size(); ++index) {
    const formation::TargetPoint2D &point = result.targets[index].position;
    failures += check(std::fabs(point.x) <= config.arena_width * 0.5 - config.boundary_margin + 1e-8
                          && std::fabs(point.y)
                                 <= config.arena_height * 0.5 - config.boundary_margin + 1e-8,
                      "target violates the arena boundary margin");
  }
  return failures;
}

} // namespace

int main()
{
  int failures = 0;
  failures += test_invalid_config_fails_before_image_loading();
  failures += test_missing_image_fails_explicitly();
  failures += test_blank_image_has_no_foreground();
  failures += test_checked_in_image_generates_connected_targets();
  if (failures == 0) {
    std::cout << "target image generation tests passed\n";
  }
  return failures == 0 ? 0 : 1;
}
