#include "../target_spatial_index.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void expect_true(const bool condition, const char *message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

std::vector<std::size_t> brute_force(const std::vector<formation::TargetPoint2D> &points,
                                     const formation::TargetPoint2D &query,
                                     const double radius)
{
  std::vector<std::size_t> matches;
  const double radius_squared = radius * radius;
  for (std::size_t index = 0; index < points.size(); ++index) {
    const double dx = points[index].x - query.x;
    const double dy = points[index].y - query.y;
    if (dx * dx + dy * dy < radius_squared)
      matches.push_back(index);
  }
  return matches;
}

void test_query_matches_strict_radius_and_preserves_index_order()
{
  const std::vector<formation::TargetPoint2D> points = {
      formation::TargetPoint2D(-12.1, -0.2), formation::TargetPoint2D(-0.1, -0.1),
      formation::TargetPoint2D(11.9, 0.0), formation::TargetPoint2D(12.0, 0.0),
      formation::TargetPoint2D(8.0, 8.0), formation::TargetPoint2D(20.0, 20.0)};
  formation::TargetSpatialIndex index(12.0);
  index.rebuild(points);

  const std::vector<std::size_t> actual =
      index.query_radius(formation::TargetPoint2D(0.0, 0.0), 12.0);
  const std::vector<std::size_t> expected =
      brute_force(points, formation::TargetPoint2D(0.0, 0.0), 12.0);
  expect_true(actual == expected, "indexed query must match the original strict-radius scan");
  expect_true(std::is_sorted(actual.begin(), actual.end()),
              "query results must preserve target index order");
}

void test_many_queries_match_brute_force()
{
  std::vector<formation::TargetPoint2D> points;
  for (int y = -20; y <= 20; ++y) {
    for (int x = -20; x <= 20; ++x)
      points.push_back(formation::TargetPoint2D(x * 1.7, y * 1.3));
  }
  formation::TargetSpatialIndex index(12.0);
  index.rebuild(points);

  for (int y = -15; y <= 15; y += 3) {
    for (int x = -15; x <= 15; x += 3) {
      const formation::TargetPoint2D query(x * 2.1 + 0.25, y * 1.8 - 0.5);
      expect_true(index.query_radius(query, 12.0) == brute_force(points, query, 12.0),
                  "indexed and brute-force queries must agree");
    }
  }
}

void test_rebuild_replaces_previous_points()
{
  formation::TargetSpatialIndex index(5.0);
  index.rebuild(std::vector<formation::TargetPoint2D>{formation::TargetPoint2D(0.0, 0.0)});
  index.rebuild(std::vector<formation::TargetPoint2D>{formation::TargetPoint2D(20.0, 20.0)});
  expect_true(index.size() == 1, "rebuild must replace rather than append points");
  expect_true(index.query_radius(formation::TargetPoint2D(0.0, 0.0), 5.0).empty(),
              "rebuild must discard old cells");
}

void test_invalid_arguments_are_rejected()
{
  bool invalid_cell_size = false;
  try {
    formation::TargetSpatialIndex index(0.0);
  } catch (const std::invalid_argument &) {
    invalid_cell_size = true;
  }
  expect_true(invalid_cell_size, "non-positive cell size must be rejected");

  formation::TargetSpatialIndex index(5.0);
  bool invalid_radius = false;
  try {
    index.query_radius(formation::TargetPoint2D(), 0.0);
  } catch (const std::invalid_argument &) {
    invalid_radius = true;
  }
  expect_true(invalid_radius, "non-positive query radius must be rejected");
}

} // namespace

int main()
{
  test_query_matches_strict_radius_and_preserves_index_order();
  test_many_queries_match_brute_force();
  test_rebuild_replaces_previous_points();
  test_invalid_arguments_are_rejected();
  std::cout << "target spatial index tests passed\n";
  return 0;
}
