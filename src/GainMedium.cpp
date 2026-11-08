#include "cavdyn/GainMedium.h"

#include "cavdyn/Constants.h"

#include <algorithm>
#include <cmath>

namespace cavdyn {
namespace {

double log_expm1(double value)
{
    if (value > 50.0) {
        return value + std::log1p(-std::exp(-value));
    }
    return std::log(std::expm1(value));
}

double log_one_plus_exp(double value)
{
    if (value > 0.0) {
        return value + std::log1p(std::exp(-value));
    }
    return std::log1p(std::exp(value));
}

double amplified_packet(double photons, double inversion, double cross_section,
                        double cell_length)
{
    if (photons <= 0.0 || inversion <= 0.0) {
        return photons;
    }
    const double sigma_j = cross_section * photons;
    const double log_gain = cross_section * inversion * cell_length;
    return log_one_plus_exp(log_gain + log_expm1(sigma_j)) / cross_section;
}

} // namespace

void GainMedium::amplify(double& photons, double& inversion,
                         double cell_length) const
{
    const double amplified = amplified_packet(
        photons, inversion, params_.cross_section, cell_length);
    inversion = std::max(0.0, inversion - (amplified - photons) / cell_length);
    photons = amplified;
}

double GainMedium::spontaneous_seed(double inversion, double time_step,
                                   double cell_length) const noexcept
{
    return 0.5 * params_.beta * params_.quantum_yield * inversion *
           (time_step / params_.lifetime) * cell_length;
}

double GainMedium::decay_factor(double time_step) const noexcept
{
    return std::isinf(params_.lifetime) ? 1.0 :
           std::exp(-time_step / params_.lifetime);
}

double GainMedium::photon_energy() const noexcept
{
    return planck_constant * speed_of_light / params_.emission_wavelength;
}

} // namespace cavdyn
