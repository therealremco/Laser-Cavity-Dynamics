#include "cavdyn/Pump.h"

#include "cavdyn/Constants.h"

#include <cmath>

namespace cavdyn {

bool Pump::contains_delay(double interval_start, double time_step) const noexcept
{
    // The simulation starts at t = 0, so the first interval also covers
    // everything before it.
    const bool first_interval = interval_start <= 0.0;
    return params_.delay < interval_start + time_step &&
           (first_interval || params_.delay >= interval_start);
}

double Pump::incident_energy(double interval_start, double time_step) const
{
    if (params_.pulse_energy) {
        double fraction = 0.0;
        if (params_.pulse_duration == 0.0) {
            fraction = contains_delay(interval_start, time_step) ? 1.0 : 0.0;
        } else {
            // Whatever the pump delivers before t = 0 (for example the leading
            // half of a Gaussian centred on t_0 = 0) arrives in the first
            // interval instead of being lost.
            const double sigma = params_.pulse_duration /
                (2.0 * std::sqrt(2.0 * std::log(2.0)));
            const double scale = sigma * std::sqrt(2.0);
            const double lower = interval_start <= 0.0 ? -1.0 :
                std::erf((interval_start - params_.delay) / scale);
            fraction = 0.5 * (std::erf((interval_start + time_step - params_.delay) / scale) -
                              lower);
        }
        return *params_.pulse_energy * fraction;
    }
    if (params_.power) {
        return *params_.power * time_step;
    }
    return 0.0;
}

double Pump::deposition_density(double interval_start, double time_step) const
{
    if (params_.initial_inversion) {
        return contains_delay(interval_start, time_step) ? *params_.initial_inversion : 0.0;
    }
    const double photon_energy = planck_constant * speed_of_light / params_.wavelength;
    return params_.efficiency * params_.absorbed_fraction *
           incident_energy(interval_start, time_step) /
           (photon_energy * params_.length * mode_area_);
}

} // namespace cavdyn
