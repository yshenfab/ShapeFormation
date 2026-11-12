#include "../collision_avoidance.hh"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

constexpr double kTolerance = 1e-9;
constexpr double kPi = 3.14159265358979323846;

bool nearly_equal(const double lhs, const double rhs)
{
  return std::fabs(lhs - rhs) < kTolerance;
}

int check(const bool condition, const char *message)
{
  if (condition) {
    return 0;
  }

  std::cerr << "FAILED: " << message << '\n';
  return 1;
}

} // namespace

int main()
{
  constexpr double safe_distance = 2.5;
  int failures = 0;

  const auto outside = formation::collision_avoidance_vector(3.0, 0.0, 0.0, safe_distance);
  failures += check(nearly_equal(outside.x, 0.0), "x offset outside safe distance");
  failures += check(nearly_equal(outside.y, 0.0), "y offset outside safe distance");

  const auto boundary =
      formation::collision_avoidance_vector(safe_distance, 0.0, 0.0, safe_distance);
  failures += check(nearly_equal(boundary.x, 0.0), "x offset at safe-distance boundary");
  failures += check(nearly_equal(boundary.y, 0.0), "y offset at safe-distance boundary");

  const auto north =
      formation::collision_avoidance_vector(2.0, kPi / 2.0, 0.0, safe_distance);
  failures += check(nearly_equal(north.x, 0.0), "north neighbor changes x");
  failures += check(north.y < 0.0, "north neighbor does not produce negative y avoidance");

  const auto rotated =
      formation::collision_avoidance_vector(2.0, 0.0, kPi / 2.0, safe_distance);
  failures += check(nearly_equal(rotated.x, 0.0), "rotated local bearing changes world x");
  failures += check(rotated.y < 0.0, "robot heading is not applied to local bearing");

  const auto far = formation::collision_avoidance_vector(2.0, 0.0, 0.0, safe_distance);
  const auto near = formation::collision_avoidance_vector(1.0, 0.0, 0.0, safe_distance);
  failures += check(std::fabs(near.x) > std::fabs(far.x),
                    "avoidance does not become stronger at shorter range");
  failures += check(nearly_equal(std::hypot(far.x, far.y), safe_distance - 2.0),
                    "avoidance magnitude does not match clearance deficit");

  const auto diagonal =
      formation::collision_avoidance_vector(2.0, 3.0 * kPi / 4.0, 0.0, safe_distance);
  failures += check(diagonal.x > 0.0, "diagonal neighbor has the wrong x direction");
  failures += check(diagonal.y < 0.0, "diagonal neighbor has the wrong y direction");

  const auto invalid = formation::collision_avoidance_vector(
      std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, safe_distance);
  failures += check(nearly_equal(invalid.x, 0.0), "non-finite range changes x");
  failures += check(nearly_equal(invalid.y, 0.0), "non-finite range changes y");

  return failures == 0 ? 0 : 1;
}
