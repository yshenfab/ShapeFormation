#ifndef FORMATION_EXPERIMENT_CONFIG_HH
#define FORMATION_EXPERIMENT_CONFIG_HH

#include <cstddef>
#include <string>

namespace formation {

struct ArenaResolution {
  bool success;
  double side;
  bool qualitative;
  std::string error;

  ArenaResolution();
};

ArenaResolution resolve_arena_side(
    std::size_t robot_count,
    const std::string& experiment_mode,
    const std::string& requested_side);

}  // namespace formation

#endif  // FORMATION_EXPERIMENT_CONFIG_HH
