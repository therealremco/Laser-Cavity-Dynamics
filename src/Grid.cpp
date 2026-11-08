#include "cavdyn/Grid.h"

#include "cavdyn/Constants.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cavdyn {

Grid::Grid(const Parameters& params)
{
    const auto& cavity = params.cavity;
    const auto& gain = params.gain_medium;
    const auto& pump = params.pump;
    const double gain_cell_length = gain.length /
                                    static_cast<double>(params.simulation.cells_in_gain);
    time_step_ = gain.n_medium * gain_cell_length / speed_of_light;
    const double background_cell_length =
        speed_of_light * time_step_ / cavity.n_background;
    const double right_background_start = gain.start + gain.length;
    const double right_background_length = cavity.length - right_background_start;
    const auto left_count = static_cast<std::size_t>(
        std::llround(gain.start / background_cell_length));
    const auto right_count = static_cast<std::size_t>(
        std::llround(right_background_length / background_cell_length));

    cells_.reserve(left_count + params.simulation.cells_in_gain + right_count);
    double z = 0.0;
    for (std::size_t i = 0; i < left_count; ++i) {
        cells_.push_back({z, background_cell_length, cavity.n_background, 0.0, false, 0.0});
        z += background_cell_length;
    }

    const double actual_gain_start = z;
    const double actual_pump_start = actual_gain_start + pump.start - gain.start;
    const double actual_pump_end = actual_pump_start + pump.length;
    for (std::size_t i = 0; i < params.simulation.cells_in_gain; ++i) {
        const double cell_start = z;
        const double cell_end = cell_start + gain_cell_length;
        const double overlap = std::max(
            0.0, std::min(cell_end, actual_pump_end) - std::max(cell_start, actual_pump_start));
        cells_.push_back({cell_start, gain_cell_length, gain.n_medium,
                          gain.internal_loss, true, overlap / gain_cell_length});
        z = cell_end;
    }

    for (std::size_t i = 0; i < right_count; ++i) {
        cells_.push_back({z, background_cell_length, cavity.n_background, 0.0, false, 0.0});
        z += background_cell_length;
    }
    actual_length_ = z;
    left_count_ = left_count;
    gain_count_ = params.simulation.cells_in_gain;
    right_count_ = right_count;
}

} // namespace cavdyn
