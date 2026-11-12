#ifndef FORMATION_COLLISION_AVOIDANCE_HH
#define FORMATION_COLLISION_AVOIDANCE_HH

#include <cmath>

namespace formation {

struct AvoidanceVector {
  double x;
  double y;
};

inline AvoidanceVector collision_avoidance_vector(const double range,
                                                  const double relative_bearing,
                                                  const double robot_heading,
                                                  const double safe_distance)
{
  if (!std::isfinite(range) || !std::isfinite(relative_bearing)
      || !std::isfinite(robot_heading) || !std::isfinite(safe_distance) || range < 0.0
      || safe_distance <= 0.0 || range >= safe_distance) {
    return AvoidanceVector{0.0, 0.0};
  }

  const double clearance_deficit = safe_distance - range;
  const double world_bearing = robot_heading + relative_bearing;
  return AvoidanceVector{-clearance_deficit * std::cos(world_bearing),
                         -clearance_deficit * std::sin(world_bearing)};
}

} // namespace formation

#endif // FORMATION_COLLISION_AVOIDANCE_HH
