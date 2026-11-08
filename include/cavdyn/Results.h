#pragma once

#include <cstddef>
#include <vector>

namespace cavdyn {

/// Results of one step. Rates are averages over the step, which ends at `time`.
struct StepResult {
    double time = 0.0;              // s, end of the step
    double output_photons = 0.0;    // photons leaving through the output coupler
    double output_power = 0.0;      // W
    double cavity_photons = 0.0;    // photons in the cavity
    double total_inversion = 0.0;   // excited molecules in the cavity
    double pump_power = 0.0;        // W, incident pump power (0 for initial_inversion)
};

struct StateView {
    const std::vector<double>& inversion;
    const std::vector<double>& photons_right;
    const std::vector<double>& photons_left;
};

struct Snapshot {
    double time = 0.0;
    std::vector<double> inversion;
    std::vector<double> photons_right;
    std::vector<double> photons_left;
};

struct Pulse {
    double peak_time = 0.0;
    double peak_power = 0.0;
    double energy = 0.0;
    double fwhm = 0.0;
};

class Results {
public:
    const std::vector<StepResult>& steps() const noexcept { return steps_; }
    const std::vector<Snapshot>& snapshots() const noexcept { return snapshots_; }
    /// Splits the output power into pulses. Peaks below min_relative_peak times
    /// the highest peak are ignored. Two peaks are separate pulses when the power
    /// between them drops below valley_ratio times the smaller peak. Each pulse
    /// runs from the lowest point before its peak to the lowest point after it,
    /// so the pulse energies (J) add up to total_output_energy().
    std::vector<Pulse> pulses(double min_relative_peak = 1e-3,
                              double valley_ratio = 0.5) const;
    double total_output_energy() const noexcept;

private:
    friend class Simulator;
    std::vector<StepResult> steps_;
    std::vector<Snapshot> snapshots_;
    double time_step_ = 0.0;
};

} // namespace cavdyn
