#pragma once

#include <optional>
#include <stdexcept>
#include <string>

namespace cavdyn {

class ParameterError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

struct CavityParams {
    double length = 1.5e-2;
    double r_hr = 1.0;
    double r_oc = 0.95;
    double n_background = 1.0;
    double mode_area = 1e-8;
};

struct GainMediumParams {
    double start = 3.0e-3;
    double length = 9.0e-3;
    double n_medium = 1.5;
    double cross_section = 1e-20;
    double lifetime = 1e-9;
    double quantum_yield = 1.0;
    double beta = 1e-4;
    double internal_loss = 0.0;
    double molecule_density = 0.0;
    double emission_wavelength = 5.5e-7;
};

struct PumpParams {
    double wavelength = 4.0e-7;
    double start = 3.0e-3;
    double length = 9.0e-3;
    std::optional<double> pulse_energy;
    double pulse_duration = 1e-13;
    std::optional<double> power;
    double delay = 0.0;
    double absorbed_fraction = 1.0;
    double efficiency = 0.8;
    std::optional<double> initial_inversion;
};

struct SimulationParams {
    std::size_t cells_in_gain = 60;
    std::optional<double> duration;
    double round_trips = 25.0;
    std::size_t snapshot_interval = 0;
};

struct Parameters {
    CavityParams cavity;
    GainMediumParams gain_medium;
    PumpParams pump;
    SimulationParams simulation;

    /// Throws ParameterError naming the first invalid parameter.
    void validate() const;

    /// Validated copy with defaults applied: when no pump mode is given,
    /// pump.initial_inversion takes default_initial_inversion.
    Parameters resolved() const;

    static constexpr double default_initial_inversion = 1e23;
};

} // namespace cavdyn
