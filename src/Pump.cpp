#include "cavdyn/Pump.h"

#include "cavdyn/Constants.h"

#include <cmath>

namespace cavdyn {

double Pump::deposition_density(double interval_start, double time_step) const
{
    // The simulation starts at t = 0. Whatever the pump delivers before that
    // (for example the leading half of a Gaussian centred on t_0 = 0) is
    // deposited in the first step instead of being lost.
    const bool first_interval = interval_start <= 0.0;
    const double interval_end = interval_start + time_step;
    const bool contains_delay = params_.delay < interval_end &&
                                (first_interval || params_.delay >= interval_start);

    if (params_.initial_inversion) {
        return contains_delay ? *params_.initial_inversion : 0.0;
    }

    double energy = 0.0;
    if (params_.pulse_energy) {
        double fraction = 0.0;
        if (params_.pulse_duration == 0.0) {
            fraction = contains_delay ? 1.0 : 0.0;
        } else {
            const double sigma = params_.pulse_duration /
                (2.0 * std::sqrt(2.0 * std::log(2.0)));
            const double scale = sigma * std::sqrt(2.0);
            const double lower = first_interval ? -1.0 :
                std::erf((interval_start - params_.delay) / scale);
            fraction = 0.5 * (std::erf((interval_end - params_.delay) / scale) - lower);
        }
        energy = *params_.pulse_energy * fraction;
    } else if (params_.power) {
        energy = *params_.power * time_step;
    }
    const double photon_energy = planck_constant * speed_of_light / params_.wavelength;
    return params_.efficiency * params_.absorbed_fraction * energy /
           (photon_energy * params_.length * mode_area_);
}

} // namespace cavdyn
