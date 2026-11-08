#pragma once

#include "cavdyn/Parameters.h"

namespace cavdyn {

class Pump {
public:
    /// mode_area in m².
    Pump(PumpParams params, double mode_area)
        : params_(params), mode_area_(mode_area) {}

    /// Inversion density (m⁻³) to add to a fully pumped cell for the interval
    /// [interval_start, interval_start + time_step] (s). Partially pumped cells
    /// receive this times their pump fraction.
    ///
    /// - pulse_energy: the Gaussian (or instantaneous) pulse energy in the interval.
    /// - power: continuous pumping, P·Δt.
    /// - initial_inversion: N_0 in the interval containing pump.delay.
    ///
    /// An interval starting at or before t = 0 also receives everything the
    /// pump delivers before t = 0, so no pump energy is lost at the start.
    double deposition_density(double interval_start, double time_step) const;

private:
    PumpParams params_;
    double mode_area_;
};

} // namespace cavdyn
