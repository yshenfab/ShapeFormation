#include "target_image_generation.hh"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

std::string escape_json(const std::string &value)
{
  std::ostringstream escaped;
  for (std::string::const_iterator iterator = value.begin(); iterator != value.end(); ++iterator) {
    switch (*iterator) {
    case '"': escaped << "\\\""; break;
    case '\\': escaped << "\\\\"; break;
    case '\b': escaped << "\\b"; break;
    case '\f': escaped << "\\f"; break;
    case '\n': escaped << "\\n"; break;
    case '\r': escaped << "\\r"; break;
    case '\t': escaped << "\\t"; break;
    default:
      if (static_cast<unsigned char>(*iterator) < 0x20) {
        escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<int>(static_cast<unsigned char>(*iterator)) << std::dec;
      } else {
        escaped << *iterator;
      }
    }
  }
  return escaped.str();
}

bool parse_size(const char *text, std::size_t &value)
{
  if (text == NULL || *text == '\0' || *text == '-') {
    return false;
  }
  errno = 0;
  char *end = NULL;
  const unsigned long long parsed = std::strtoull(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || parsed == 0
      || parsed > std::numeric_limits<std::size_t>::max()) {
    return false;
  }
  value = static_cast<std::size_t>(parsed);
  return true;
}

bool parse_positive_double(const char *text, double &value)
{
  if (text == NULL || *text == '\0') {
    return false;
  }
  errno = 0;
  char *end = NULL;
  const double parsed = std::strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' || !std::isfinite(parsed) || parsed <= 0.0) {
    return false;
  }
  value = parsed;
  return true;
}

void write_number_or_null(const double value)
{
  if (std::isfinite(value)) {
    std::cout << std::setprecision(17) << value;
  } else {
    std::cout << "null";
  }
}

void write_result(const formation::TargetImageGenerationResult &result,
                  const std::size_t target_count,
                  const double arena_side,
                  const std::string &image_path)
{
  std::cout << "{\"success\":" << (result.success ? "true" : "false")
            << ",\"error_code\":\"" << escape_json(result.error_code)
            << "\",\"error\":\"" << escape_json(result.error)
            << "\",\"target_count\":" << target_count
            << ",\"generated_target_count\":" << result.targets.size()
            << ",\"image_path\":\"" << escape_json(image_path)
            << "\",\"arena_side\":" << std::setprecision(17) << arena_side
            << ",\"component_count\":" << result.metrics.component_count
            << ",\"graph_component_count\":" << result.metrics.graph_component_count
            << ",\"target_min_distance\":";
  write_number_or_null(result.success ? result.metrics.minimum_distance
                                      : std::numeric_limits<double>::quiet_NaN());
  std::cout << ",\"mst_bottleneck\":";
  write_number_or_null(result.success ? result.metrics.mst_bottleneck
                                      : std::numeric_limits<double>::quiet_NaN());
  std::cout << ",\"scale\":";
  write_number_or_null(result.success ? result.metrics.scale
                                      : std::numeric_limits<double>::quiet_NaN());
  std::cout << "}\n";
}

void write_usage_error(const std::string &message)
{
  std::cout << "{\"success\":false,\"error_code\":\"invalid_argument\",\"error\":\""
            << escape_json(message) << "\"}\n";
}

} // namespace

int main(int argc, char **argv)
{
  if (argc != 4) {
    write_usage_error("usage: target_qualification N image_path arena_side");
    return 2;
  }

  std::size_t target_count = 0;
  double arena_side = 0.0;
  if (!parse_size(argv[1], target_count) || !parse_positive_double(argv[3], arena_side)) {
    write_usage_error("N must be a positive integer and arena_side a positive finite number");
    return 2;
  }

  formation::TargetImageGenerationConfig config;
  config.target_count = target_count;
  config.safe_distance = 2.625;
  config.communication_distance = 11.4;
  config.arena_width = arena_side;
  config.arena_height = arena_side;
  config.boundary_margin = 1.0;
  try {
    const formation::TargetImageGenerationResult result =
        formation::generate_targets_from_image(argv[2], config);
    write_result(result, target_count, arena_side, argv[2]);

    if (result.success || result.error_code == "generation_failed") {
      return 0;
    }
    return 3;
  } catch (const std::exception &error) {
    std::cout << "{\"success\":false,\"error_code\":\"runtime_error\",\"error\":\""
              << escape_json(error.what()) << "\"}\n";
    return 3;
  }
}
