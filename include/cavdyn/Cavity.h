#pragma once

#include "cavdyn/Grid.h"
#include "cavdyn/Parameters.h"

namespace cavdyn {

class Cavity {
public:
    explicit Cavity(CavityParams params) : params_(params) {}

    double internal_transmission(const Cell& cell) const noexcept;
    double reflect_at_high_reflector(double photons) const noexcept;
    double reflect_at_output_coupler(double photons) const noexcept;
    double output_photons(double incident_areal_density) const noexcept;

private:
    CavityParams params_;
};

} // namespace cavdyn
