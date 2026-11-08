#pragma once

#include "cavdyn/Parameters.h"

#include <cstddef>
#include <vector>

namespace cavdyn {

struct Cell {
    double z_start = 0.0;
    double length = 0.0;
    double refractive_index = 1.0;
    double internal_loss = 0.0;
    bool in_gain = false;
    double pump_fraction = 0.0;
};

class Grid {
public:
    explicit Grid(const Parameters& params);

    double time_step() const noexcept { return time_step_; }
    double actual_length() const noexcept { return actual_length_; }
    double round_trip_time() const noexcept { return 2.0 * cells_.size() * time_step_; }
    std::size_t size() const noexcept { return cells_.size(); }
    const std::vector<Cell>& cells() const noexcept { return cells_; }

private:
    double time_step_ = 0.0;
    double actual_length_ = 0.0;
    std::vector<Cell> cells_;
};

} // namespace cavdyn
