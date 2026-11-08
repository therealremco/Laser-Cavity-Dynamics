#pragma once

#include "cavdyn/Parameters.h"

namespace cavdyn {

class GainMedium {
public:
    explicit GainMedium(GainMediumParams params) : params_(params) {}

    void amplify(double& photons_areal_density, double& inversion,
                 double cell_length) const;
    double spontaneous_seed(double inversion, double time_step,
                           double cell_length) const noexcept;
    double decay_factor(double time_step) const noexcept;
    double photon_energy() const noexcept;

private:
    GainMediumParams params_;
};

} // namespace cavdyn
