#include "experiment_config.hh"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace formation {

namespace {

bool parse_positive_double(const std::string& text, double* value) {
  if (text.empty() || value == NULL) {
    return false;
  }
  char* end = NULL;
  errno = 0;
  const double parsed = std::strtod(text.c_str(), &end);
  if (errno != 0 || end == text.c_str() || *end != '\0' ||
      !std::isfinite(parsed) || parsed <= 0.0) {
    return false;
  }
  *value = parsed;
  return true;
}

ArenaResolution failure(const std::string& message) {
  ArenaResolution result;
  result.error = message;
  return result;
}

}  // namespace

ArenaResolution::ArenaResolution()
    : success(false), side(0.0), qualitative(false) {}

ArenaResolution resolve_arena_side(
    std::size_t robot_count,
    const std::string& experiment_mode,
    const std::string& requested_side) {
  if (robot_count == 0) {
    return failure("robot count must be positive");
  }

  const std::string mode =
      experiment_mode.empty() ? "fixed_density" : experiment_mode;
  const double expected_side =
      std::sqrt(50.0 * static_cast<double>(robot_count));

  if (mode == "fixed_density") {
    double side = expected_side;
    if (!requested_side.empty() &&
        !parse_positive_double(requested_side, &side)) {
      return failure("invalid fixed-density arena side: '" + requested_side + "'");
    }
    const double tolerance = 1e-6 * std::fmax(1.0, expected_side);
    if (std::fabs(side - expected_side) > tolerance) {
      std::ostringstream message;
      message << "arena side " << side << " does not match sqrt(50*N)="
              << expected_side << " in fixed_density mode";
      return failure(message.str());
    }
    ArenaResolution result;
    result.success = true;
    result.side = expected_side;
    return result;
  }

  if (mode == "qualitative") {
    double side = 0.0;
    if (!parse_positive_double(requested_side, &side)) {
      return failure(
          "qualitative mode requires a positive FORMATION_ARENA_SIDE");
    }
    ArenaResolution result;
    result.success = true;
    result.side = side;
    result.qualitative = true;
    return result;
  }

  return failure("unknown FORMATION_EXPERIMENT_MODE: '" + mode + "'");
}

}  // namespace formation
