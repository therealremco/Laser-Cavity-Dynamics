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
    explicit Simulator(const Parameters& params);

    void step();
    void run(Observer* observer = nullptr);
    double time() const noexcept;
    std::size_t step_index() const noexcept { return step_index_; }
    const Grid& grid() const noexcept { return grid_; }
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
