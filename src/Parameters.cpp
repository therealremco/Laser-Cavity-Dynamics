#include "cavdyn/Parameters.h"

#include <cmath>
#include <string>

namespace cavdyn {
namespace {

void positive(double value, const char* name, bool allow_zero = false)
{
    if (!std::isfinite(value) || (allow_zero ? value < 0.0 : value <= 0.0)) {
        throw ParameterError(std::string(name) + " must be " +
                             (allow_zero ? "finite and non-negative" :
                                           "finite and positive"));
    }
}

void fraction(double value, const char* name)
{
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw ParameterError(std::string(name) + " must be between 0 and 1");
    }
}

} // namespace

void Parameters::validate() const
{
    positive(cavity.length, "cavity.length");
    fraction(cavity.r_hr, "cavity.r_hr");
    fraction(cavity.r_oc, "cavity.r_oc");
    positive(cavity.n_background, "cavity.n_background");
    positive(cavity.mode_area, "cavity.mode_area");

    positive(gain_medium.start, "gain_medium.start", true);
    positive(gain_medium.length, "gain_medium.length");
    positive(gain_medium.n_medium, "gain_medium.n_medium");
    positive(gain_medium.cross_section, "gain_medium.cross_section");
    if (std::isnan(gain_medium.lifetime) || gain_medium.lifetime <= 0.0) {
        throw ParameterError("gain_medium.lifetime must be positive");
    }
    fraction(gain_medium.quantum_yield, "gain_medium.quantum_yield");
    fraction(gain_medium.beta, "gain_medium.beta");
    positive(gain_medium.internal_loss, "gain_medium.internal_loss", true);
    positive(gain_medium.molecule_density, "gain_medium.molecule_density", true);
    positive(gain_medium.emission_wavelength, "gain_medium.emission_wavelength");

    if (gain_medium.start + gain_medium.length > cavity.length) {
        throw ParameterError("gain_medium.start + gain_medium.length must not exceed cavity.length");
    }

    positive(pump.wavelength, "pump.wavelength");
    positive(pump.start, "pump.start", true);
    positive(pump.length, "pump.length");
    if (std::isnan(pump.pulse_duration) || pump.pulse_duration < 0.0) {
        throw ParameterError("pump.pulse_duration must be non-negative");
    }
    positive(pump.delay, "pump.delay", true);
    fraction(pump.absorbed_fraction, "pump.absorbed_fraction");
    fraction(pump.efficiency, "pump.efficiency");
    if (pump.start < gain_medium.start ||
        pump.start + pump.length > gain_medium.start + gain_medium.length) {
        throw ParameterError("pump.start and pump.length must lie inside the gain medium");
    }

    const int selected_pumps = static_cast<int>(pump.pulse_energy.has_value()) +
                               static_cast<int>(pump.power.has_value()) +
                               static_cast<int>(pump.initial_inversion.has_value());
    if (selected_pumps > 1) {
        throw ParameterError("pump.pulse_energy, pump.power and pump.initial_inversion are mutually exclusive");
    }
    if (pump.pulse_energy) {
        positive(*pump.pulse_energy, "pump.pulse_energy", true);
    }
    if (pump.power) {
        positive(*pump.power, "pump.power", true);
        if (!std::isinf(pump.pulse_duration)) {
            throw ParameterError("pump.power requires pump.pulse_duration = inf");
        }
    }
    if (pump.pulse_energy && std::isinf(pump.pulse_duration)) {
        throw ParameterError("pump.pulse_energy requires a finite pump.pulse_duration");
    }
    if (pump.initial_inversion) {
        positive(*pump.initial_inversion, "pump.initial_inversion", true);
    }

    if (simulation.cells_in_gain == 0) {
        throw ParameterError("simulation.cells_in_gain must be positive");
    }
    if (simulation.duration) {
        positive(*simulation.duration, "simulation.duration");
    } else {
        positive(simulation.round_trips, "simulation.round_trips");
    }
}

} // namespace cavdyn
