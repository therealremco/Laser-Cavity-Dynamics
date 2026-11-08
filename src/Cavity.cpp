#include "cavdyn/Cavity.h"

#include <cmath>

namespace cavdyn {

double Cavity::internal_transmission(const Cell& cell) const noexcept
{
    return std::exp(-cell.internal_loss * cell.length);
}

double Cavity::reflect_at_high_reflector(double photons) const noexcept
{
    return params_.r_hr * photons;
}

double Cavity::reflect_at_output_coupler(double photons) const noexcept
{
    return params_.r_oc * photons;
}

double Cavity::output_photons(double incident_areal_density) const noexcept
{
    return (1.0 - params_.r_oc) * incident_areal_density * params_.mode_area;
}

} // namespace cavdyn
