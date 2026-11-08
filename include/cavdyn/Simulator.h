#pragma once

#include "cavdyn/Cavity.h"
#include "cavdyn/GainMedium.h"
#include "cavdyn/Grid.h"
#include "cavdyn/Observer.h"
#include "cavdyn/Parameters.h"
#include "cavdyn/Pump.h"
#include "cavdyn/Results.h"

#include <cstddef>
#include <vector>

namespace cavdyn {

class Simulator {
public:
    /// Validates the parameters (throws ParameterError) and builds the grid.
    explicit Simulator(const Parameters& params);

    /// Advances the state by one time step Δt.
    void step();
    /// Steps until the requested duration, or until the observer returns false.
    void run(Observer* observer = nullptr);

    /// Sets the right- and left-moving photon densities (photons/m²) in one
    /// cell, for example to start from a given packet instead of the
    /// spontaneous seed. Throws std::out_of_range for a cell outside the grid.
    void set_photons(std::size_t cell, double right_moving, double left_moving);

    /// Simulated time (s) at the end of the last step.
    double time() const noexcept;
    std::size_t step_index() const noexcept { return step_index_; }
    const Grid& grid() const noexcept { return grid_; }
    /// The parameters in use, with the default pump mode filled in.
    const Parameters& parameters() const noexcept { return params_; }
    StateView state() const noexcept;
    const Results& results() const noexcept { return results_; }

private:
    Parameters params_;
    Grid grid_;
    Cavity cavity_;
    GainMedium gain_medium_;
    Pump pump_;
    std::vector<double> inversion_;
    std::vector<double> photons_right_;
    std::vector<double> photons_left_;
    std::size_t step_index_ = 0;
    std::size_t requested_steps_ = 0;
    Results results_;

    double total_inversion() const noexcept;
    double cavity_photons() const noexcept;
};

} // namespace cavdyn
