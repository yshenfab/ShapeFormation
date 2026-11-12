#include "target_spatial_index.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace formation {

TargetSpatialIndex::TargetSpatialIndex(const double cell_size) : cell_size_(cell_size)
{
  if (!std::isfinite(cell_size_) || cell_size_ <= 0.0)
    throw std::invalid_argument("target spatial-index cell size must be positive");
}

TargetSpatialIndex::Cell TargetSpatialIndex::cell_for(const TargetPoint2D &point) const
{
  return Cell(static_cast<long long>(std::floor(point.x / cell_size_)),
              static_cast<long long>(std::floor(point.y / cell_size_)));
}

void TargetSpatialIndex::rebuild(const std::vector<TargetPoint2D> &points)
{
  points_ = points;
  cells_.clear();
  for (std::size_t index = 0; index < points_.size(); ++index)
    cells_[cell_for(points_[index])].push_back(index);
}

std::vector<std::size_t> TargetSpatialIndex::query_radius(const TargetPoint2D &query,
                                                          const double radius) const
{
  if (!std::isfinite(radius) || radius <= 0.0)
    throw std::invalid_argument("target spatial-index query radius must be positive");

  std::vector<std::size_t> matches;
  const Cell center = cell_for(query);
  const long long reach = static_cast<long long>(std::ceil(radius / cell_size_));
  const double radius_squared = radius * radius;
  for (long long cell_y = center.second - reach; cell_y <= center.second + reach; ++cell_y) {
    for (long long cell_x = center.first - reach; cell_x <= center.first + reach; ++cell_x) {
      const std::map<Cell, std::vector<std::size_t>>::const_iterator bucket =
          cells_.find(Cell(cell_x, cell_y));
      if (bucket == cells_.end())
        continue;
      for (std::size_t offset = 0; offset < bucket->second.size(); ++offset) {
        const std::size_t index = bucket->second[offset];
        const double dx = points_[index].x - query.x;
        const double dy = points_[index].y - query.y;
        if (dx * dx + dy * dy < radius_squared)
          matches.push_back(index);
      }
    }
  }
  std::sort(matches.begin(), matches.end());
  return matches;
}

std::size_t TargetSpatialIndex::size() const
{
  return points_.size();
}

} // namespace formation
