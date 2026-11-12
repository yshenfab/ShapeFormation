#include "target_image_generation.hh"

#include <cmath>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace formation {
namespace {

TargetImageGenerationResult failure(const std::string &code, const std::string &message)
{
  TargetImageGenerationResult result;
  result.error_code = code;
  result.error = message;
  return result;
}

bool valid_config(const TargetImageGenerationConfig &config)
{
  return config.target_count > 0 && std::isfinite(config.safe_distance)
         && config.safe_distance > 0.0 && std::isfinite(config.communication_distance)
         && config.communication_distance > 0.0
         && (config.target_count == 1
             || config.safe_distance <= config.communication_distance)
         && std::isfinite(config.arena_width) && config.arena_width > 0.0
         && std::isfinite(config.arena_height) && config.arena_height > 0.0
         && std::isfinite(config.boundary_margin) && config.boundary_margin >= 0.0
         && config.arena_width > 2.0 * config.boundary_margin
         && config.arena_height > 2.0 * config.boundary_margin
         && std::isfinite(config.binary_threshold) && config.binary_threshold >= 0.0
         && config.binary_threshold <= 255.0;
}

} // namespace

TargetImageGenerationConfig::TargetImageGenerationConfig()
    : target_count(0),
      safe_distance(2.625),
      communication_distance(11.4),
      arena_width(0.0),
      arena_height(0.0),
      boundary_margin(1.0),
      binary_threshold(100.0)
{
}

TargetImageGenerationResult::TargetImageGenerationResult()
    : success(false),
      source_width(0),
      source_height(0),
      cropped_width(0),
      cropped_height(0)
{
}

TargetImageGenerationResult generate_targets_from_image(
    const std::string &image_path,
    const TargetImageGenerationConfig &config)
{
  if (image_path.empty() || !valid_config(config)) {
    return failure("invalid_argument", "invalid image path or target-generation limits");
  }

  const cv::Mat grayscale = cv::imread(image_path, cv::IMREAD_GRAYSCALE);
  if (grayscale.empty()) {
    return failure("image_read_failed", "unable to read image: " + image_path);
  }

  TargetImageGenerationResult result;
  result.source_width = grayscale.cols;
  result.source_height = grayscale.rows;

  cv::Mat binary;
  cv::threshold(grayscale, binary, config.binary_threshold, 255, cv::THRESH_BINARY);
  const cv::Mat foreground = binary == 0;
  std::vector<cv::Point> foreground_pixels;
  cv::findNonZero(foreground, foreground_pixels);
  if (foreground_pixels.empty()) {
    result.error_code = "no_foreground";
    result.error = "image contains no foreground pixels";
    return result;
  }

  const cv::Rect bounds = cv::boundingRect(foreground_pixels);
  const cv::Mat cropped_foreground = foreground(bounds).clone();
  result.cropped_width = cropped_foreground.cols;
  result.cropped_height = cropped_foreground.rows;

  cv::Mat labels;
  cv::Mat component_stats;
  cv::Mat component_centroids;
  const int label_count = cv::connectedComponentsWithStats(
      cropped_foreground, labels, component_stats, component_centroids, 8, CV_32S);
  const int foreground_component_count = label_count - 1;
  if (foreground_component_count <= 0) {
    result.error_code = "component_analysis_failed";
    result.error = "connected-component analysis found no foreground";
    return result;
  }

  result.component_candidates.resize(static_cast<std::size_t>(foreground_component_count));
  for (int row = 0; row < labels.rows; ++row) {
    for (int column = 0; column < labels.cols; ++column) {
      const int label = labels.at<int>(row, column);
      if (label > 0) {
        result.component_candidates[static_cast<std::size_t>(label - 1)].push_back(
            TargetPoint2D(static_cast<double>(column), static_cast<double>(row)));
      }
    }
  }

  ConnectedTargetConfig connected_config;
  connected_config.target_count = config.target_count;
  connected_config.safe_distance = config.safe_distance;
  connected_config.communication_distance = config.communication_distance;
  connected_config.arena_width = config.arena_width;
  connected_config.arena_height = config.arena_height;
  connected_config.boundary_margin = config.boundary_margin;

  const ConnectedTargetResult generated =
      generate_connected_targets(result.component_candidates, connected_config);
  result.error = generated.error;
  result.error_code = generated.success ? "" : "generation_failed";
  result.targets = generated.targets;
  result.mst_edges = generated.mst_edges;
  result.component_translations = generated.component_translations;
  result.metrics = generated.metrics;
  result.success = generated.success;
  return result;
}

} // namespace formation
