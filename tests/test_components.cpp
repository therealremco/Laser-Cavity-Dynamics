// Parameters, Grid, GainMedium, Cavity and Pump in isolation.

#include "TestSupport.h"

#include "cavdyn/Cavity.h"
#include "cavdyn/Constants.h"
#include "cavdyn/GainMedium.h"
#include "cavdyn/Grid.h"
#include "cavdyn/Pump.h"

#include <limits>

using namespace cavdyn_test;

namespace {

const double infinity = std::numeric_limits<double>::infinity();

void parameter_validation_names_parameter()
{
    {
        cavdyn::Parameters params;
        params.cavity.r_oc = 1.5;
        require_throws<cavdyn::ParameterError>([&] { params.validate(); }, "cavity.r_oc",
                                               "reflectivity above 1");
    }
    {
        cavdyn::Parameters params;
        params.pump.start = 1e-3;
        require_throws<cavdyn::ParameterError>([&] { params.validate(); }, "pump.start",
                                               "pump stripe outside the gain medium");
    }
    {
        cavdyn::Parameters params;
        params.pump.power = 1.0;
        params.pump.pulse_duration = infinity;
        params.pump.initial_inversion = 1e23;
        require_throws<cavdyn::ParameterError>([&] { params.validate(); },
                                               "mutually exclusive", "two pump modes");
    }
    {
        cavdyn::Parameters params;
        params.pump.power = 1.0;
        require_throws<cavdyn::ParameterError>([&] { params.validate(); },
                                               "pulse_duration = inf",
                                               "continuous pump with finite pulse");
    }
}

void grid_equal_transit_time()
{
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 12;
    const cavdyn::Grid grid(params);
    require(grid.left_background_cells() + grid.gain_cells() +
                grid.right_background_cells() == grid.size(),
            "region cell counts do not add up to the grid size");
    require(grid.gain_cells() == 12, "gain cell count is wrong");
    double optical_length = 0.0;
    for (const auto& cell : grid.cells()) {
        require_close(cell.refractive_index * cell.length / cavdyn::speed_of_light,
                      grid.time_step(), 1e-12, "cell transit time");
        optical_length += cell.refractive_index * cell.length;
    }
    require_close(grid.round_trip_time(), 2.0 * optical_length / cavdyn::speed_of_light,
                  1e-12, "round-trip time");
}

void grid_partial_pump_fraction()
{
    cavdyn::Parameters params;
    params.pump.length = 4.4e-3;
    const cavdyn::Grid grid(params);
    double pumped_length = 0.0;
    for (const auto& cell : grid.cells()) {
        require(cell.pump_fraction >= 0.0 && cell.pump_fraction <= 1.0 + 1e-12,
                "pump fraction outside [0, 1]");
        require(cell.in_gain || cell.pump_fraction == 0.0, "pumped background cell");
        pumped_length += cell.pump_fraction * cell.length;
    }
    require_close(pumped_length, params.pump.length, 1e-9, "pumped length");
}

void frantz_nodvik_conserves_and_saturates()
{
    const cavdyn::GainMedium gain(cavdyn::GainMediumParams{});
    const double length = 1e-4;
    double inversion = 1e23;
    double photons = 1e18;
    const double before = inversion * length + photons;
    gain.amplify(photons, inversion, length);
    require_close(inversion * length + photons, before, 1e-14,
                  "photons plus inversion");
    require(inversion >= 0.0, "inversion became negative");

    // A packet far above saturation removes at most the stored inversion.
    inversion = 1e23;
    photons = 1e30;
    gain.amplify(photons, inversion, length);
    require(inversion >= 0.0 && inversion < 1e15, "strong packet did not saturate the gain");
}

void frantz_nodvik_small_signal_limit()
{
    cavdyn::GainMediumParams gain_params;
    const cavdyn::GainMedium gain(gain_params);
    const double length = 1e-4;
    double inversion = 1e23;
    double photons = 1.0;
    gain.amplify(photons, inversion, length);
    require_close(photons, std::exp(gain_params.cross_section * 1e23 * length), 1e-12,
                  "small-signal gain");
}

void spontaneous_seed_and_decay()
{
    cavdyn::GainMediumParams params;
    params.beta = 1e-3;
    params.quantum_yield = 0.5;
    params.lifetime = 2e-9;
    const cavdyn::GainMedium gain(params);
    require_close(gain.spontaneous_seed(1e22, 1e-12, 1e-4),
                  0.5 * 1e-3 * 0.5 * 1e22 * (1e-12 / 2e-9) * 1e-4, 1e-14, "seed");
    require_close(gain.decay_factor(1e-12), std::exp(-1e-12 / 2e-9), 1e-14, "decay");

    params.lifetime = infinity;
    const cavdyn::GainMedium stable(params);
    require(stable.decay_factor(1e-12) == 1.0, "infinite lifetime should not decay");
    require(stable.spontaneous_seed(1e22, 1e-12, 1e-4) == 0.0,
            "infinite lifetime should not seed");
}

void cavity_coupling()
{
    cavdyn::CavityParams params;
    params.r_hr = 0.9;
    params.r_oc = 0.8;
    params.mode_area = 2.0;
    const cavdyn::Cavity cavity(params);
    require(cavity.reflect_at_high_reflector(10.0) == 9.0, "high-reflector reflection");
    require(cavity.reflect_at_output_coupler(10.0) == 8.0, "output-coupler reflection");
    require_close(cavity.output_photons(10.0), 4.0, 1e-14, "output coupling");
    cavdyn::Cell cell;
    cell.length = 1e-3;
    cell.internal_loss = 5.0;
    require_close(cavity.internal_transmission(cell), std::exp(-5e-3), 1e-14,
                  "internal transmission");
}

double expected_pulse_density(const cavdyn::PumpParams& params, double area)
{
    const double photon_energy =
        cavdyn::planck_constant * cavdyn::speed_of_light / params.wavelength;
    return params.efficiency * params.absorbed_fraction * *params.pulse_energy /
           (photon_energy * params.length * area);
}

void pump_instantaneous_pulse()
{
    cavdyn::PumpParams params;
    params.pulse_energy = 1e-6;
    params.pulse_duration = 0.0;
    params.delay = 3.5e-12;
    const double area = 1e-8;
    const cavdyn::Pump pump(params, area);
    require(pump.deposition_density(2e-12, 1e-12) == 0.0, "deposit before the delay");
    require_close(pump.deposition_density(3e-12, 1e-12),
                  expected_pulse_density(params, area), 1e-14, "deposit at the delay");
    require(pump.deposition_density(4e-12, 1e-12) == 0.0, "deposit after the delay");
}

void pump_gaussian_pulse_complete()
{
    // Summed over all steps, including the part before t = 0, the full energy arrives.
    for (const double delay : {0.0, 1e-13, 2e-12}) {
        cavdyn::PumpParams params;
        params.pulse_energy = 1e-6;
        params.pulse_duration = 5e-13;
        params.delay = delay;
        const double area = 1e-8;
        const cavdyn::Pump pump(params, area);
        double total = 0.0;
        for (int step = 0; step < 100; ++step) {
            total += pump.deposition_density(step * 1e-13, 1e-13);
        }
        require_close(total, expected_pulse_density(params, area), 1e-12,
                      "Gaussian pulse total with delay " + std::to_string(delay));
    }
}

void pump_continuous_and_direct()
{
    cavdyn::PumpParams params;
    params.power = 2.0;
    params.pulse_duration = infinity;
    const cavdyn::Pump continuous(params, 1e-8);
    const double photon_energy =
        cavdyn::planck_constant * cavdyn::speed_of_light / params.wavelength;
    require_close(continuous.deposition_density(5e-12, 1e-12),
                  params.efficiency * 2.0 * 1e-12 / (photon_energy * params.length * 1e-8),
                  1e-14, "continuous pump");

    cavdyn::PumpParams direct;
    direct.initial_inversion = 4e22;
    direct.delay = 1.5e-12;
    const cavdyn::Pump pump(direct, 1e-8);
    require(pump.deposition_density(0.0, 1e-12) == 0.0, "direct inversion before delay");
    require(pump.deposition_density(1e-12, 1e-12) == 4e22, "direct inversion at delay");
    require(pump.deposition_density(2e-12, 1e-12) == 0.0, "direct inversion repeated");
}

} // namespace

int main(int argc, char** argv)
{
    return run_tests(argc, argv, {
        {"parameter_validation_names_parameter", parameter_validation_names_parameter},
        {"grid_equal_transit_time", grid_equal_transit_time},
        {"grid_partial_pump_fraction", grid_partial_pump_fraction},
        {"frantz_nodvik_conserves_and_saturates", frantz_nodvik_conserves_and_saturates},
        {"frantz_nodvik_small_signal_limit", frantz_nodvik_small_signal_limit},
        {"spontaneous_seed_and_decay", spontaneous_seed_and_decay},
        {"cavity_coupling", cavity_coupling},
        {"pump_instantaneous_pulse", pump_instantaneous_pulse},
        {"pump_gaussian_pulse_complete", pump_gaussian_pulse_complete},
        {"pump_continuous_and_direct", pump_continuous_and_direct},
    });
}
