#ifndef FORMATION_TARGET_SPATIAL_INDEX_HH
#define FORMATION_TARGET_SPATIAL_INDEX_HH

#include "connected_target_generation.hh"

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace formation {

class TargetSpatialIndex {
public:
  explicit TargetSpatialIndex(double cell_size);

  void rebuild(const std::vector<TargetPoint2D> &points);
  std::vector<std::size_t> query_radius(const TargetPoint2D &query, double radius) const;
  std::size_t size() const;

private:
  typedef std::pair<long long, long long> Cell;

  Cell cell_for(const TargetPoint2D &point) const;

  double cell_size_;
  std::vector<TargetPoint2D> points_;
  std::map<Cell, std::vector<std::size_t>> cells_;
};

} // namespace formation

#endif // FORMATION_TARGET_SPATIAL_INDEX_HH
