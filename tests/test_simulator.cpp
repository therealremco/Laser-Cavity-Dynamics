// Simulator behaviour: stepping, pumping, observers, snapshots and pulse analysis.

#include "TestSupport.h"

#include "cavdyn/Constants.h"
#include "cavdyn/Simulator.h"

#include <limits>

using namespace cavdyn_test;

namespace {

const double infinity = std::numeric_limits<double>::infinity();

/// Default parameters without spontaneous emission or decay, so the inversion
/// changes only through the pump.
cavdyn::Parameters quiet_parameters()
{
    cavdyn::Parameters params;
    params.gain_medium.beta = 0.0;
    params.gain_medium.lifetime = infinity;
    params.simulation.round_trips = 1.0;
    return params;
}

double inversion_after_one_step(const cavdyn::Parameters& params)
{
    cavdyn::Simulator simulator(params);
    simulator.step();
    return simulator.results().steps().back().total_inversion;
}

void inversion_decays_exponentially()
{
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 12;
    params.gain_medium.beta = 0.0;
    cavdyn::Simulator simulator(params);
    simulator.step();
    const double expected = cavdyn::Parameters::default_initial_inversion *
                            params.gain_medium.length * params.cavity.mode_area *
                            std::exp(-simulator.grid().time_step() /
                                     params.gain_medium.lifetime);
    require_close(simulator.results().steps().back().total_inversion, expected, 1e-12,
                  "inversion after one step");
}

void gaussian_pump_at_start_is_not_lost()
{
    // A pulse centred on t = 0 deposits all of its energy, not only the half
    // that arrives after the simulation starts.
    auto params = quiet_parameters();
    params.pump.pulse_energy = 1e-6;
    const double photon_energy =
        cavdyn::planck_constant * cavdyn::speed_of_light / params.pump.wavelength;
    require_close(inversion_after_one_step(params),
                  params.pump.efficiency * params.pump.absorbed_fraction *
                      *params.pump.pulse_energy / photon_energy,
                  1e-9, "inversion from a pulse at t = 0");
}

void initial_inversion_scales_with_pump_fraction()
{
    // A stripe of 29.33 gain cells holds N_0·L_p·A, not N_0 in 30 full cells.
    auto params = quiet_parameters();
    params.pump.length = 4.4e-3;
    require_close(inversion_after_one_step(params),
                  1e23 * params.pump.length * params.cavity.mode_area, 1e-9,
                  "initial inversion over a partial stripe");
}

void initial_inversion_waits_for_delay()
{
    auto params = quiet_parameters();
    params.pump.delay = 5e-12;
    params.simulation.duration = 1e-11;
    cavdyn::Simulator simulator(params);
    simulator.run();
    const double dt = simulator.grid().time_step();
    const double expected = 1e23 * params.pump.length * params.cavity.mode_area;
    for (const auto& step : simulator.results().steps()) {
        if (step.time <= params.pump.delay) {
            require(step.total_inversion == 0.0, "inversion appeared before pump.delay");
        } else if (step.time - dt > params.pump.delay) {
            require_close(step.total_inversion, expected, 1e-9,
                          "inversion after pump.delay");
        }
    }
}

void pump_power_follows_the_pulse()
{
    // A 5 ps pulse at 20 ps, several steps wide: the pump power column must
    // integrate to the pulse energy, peak at the delay and have the pulse FWHM.
    auto params = quiet_parameters();
    params.pump.pulse_energy = 1e-6;
    params.pump.pulse_duration = 5e-12;
    params.pump.delay = 20e-12;
    params.simulation.duration = 60e-12;
    cavdyn::Simulator simulator(params);
    simulator.run();
    const auto& steps = simulator.results().steps();
    const double dt = simulator.grid().time_step();

    double energy = 0.0;
    const cavdyn::StepResult* peak = &steps.front();
    for (const auto& step : steps) {
        energy += step.pump_power * dt;
        if (step.pump_power > peak->pump_power) {
            peak = &step;
        }
    }
    require_close(energy, *params.pump.pulse_energy, 1e-9, "pump power integrated over time");
    require(std::abs(peak->time - 0.5 * dt - params.pump.delay) <= 0.5 * dt + 1e-18,
            "pump power does not peak at pump.delay");
    std::size_t above_half = 0;
    for (const auto& step : steps) {
        above_half += step.pump_power >= 0.5 * peak->pump_power ? 1 : 0;
    }
    require(std::abs(static_cast<double>(above_half) * dt - params.pump.pulse_duration) <=
                1.5 * dt,
            "pump power FWHM does not match pump.pulse_duration");
}

void pump_power_continuous_and_direct()
{
    auto continuous = quiet_parameters();
    continuous.pump.power = 2.5;
    continuous.pump.pulse_duration = infinity;
    cavdyn::Simulator pumped(continuous);
    pumped.run();
    for (const auto& step : pumped.results().steps()) {
        require_close(step.pump_power, 2.5, 1e-12, "continuous pump power");
    }

    cavdyn::Simulator direct(quiet_parameters());
    direct.run();
    for (const auto& step : direct.results().steps()) {
        require(step.pump_power == 0.0, "initial_inversion should report no pump power");
    }
}

void molecule_density_clamps_inversion()
{
    auto params = quiet_parameters();
    params.gain_medium.molecule_density = 4e22;
    require_close(inversion_after_one_step(params),
                  4e22 * params.pump.length * params.cavity.mode_area, 1e-12,
                  "clamped inversion");
}

void emission_and_snapshots()
{
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 12;
    params.simulation.round_trips = 2.0;
    params.simulation.snapshot_interval = 2;
    params.gain_medium.beta = 0.05;
    cavdyn::Simulator simulator(params);
    simulator.run();
    const auto& results = simulator.results();
    require(results.steps().size() == simulator.step_index(), "one result per step");
    require(results.snapshots().size() == simulator.step_index() / 2,
            "one snapshot every snapshot_interval steps");
    require(results.snapshots().front().inversion.size() == simulator.grid().size(),
            "snapshot does not cover the grid");
    require(results.total_output_energy() > 0.0, "spontaneous seed produced no output");
}

void observer_sees_every_step_and_can_cancel()
{
    struct StopAfter final : cavdyn::Observer {
        std::size_t calls = 0;
        std::size_t grid_size = 0;
        bool on_step(const cavdyn::StepResult&, cavdyn::StateView state) override
        {
            ++calls;
            grid_size = state.inversion.size();
            return calls < 5;
        }
    } observer;

    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 8;
    cavdyn::Simulator simulator(params);
    simulator.run(&observer);
    require(observer.calls == 5 && simulator.step_index() == 5,
            "observer cancellation did not stop the run");
    require(observer.grid_size == simulator.grid().size(), "observer got no state");
}

void set_photons_checks_range()
{
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 8;
    cavdyn::Simulator simulator(params);
    simulator.set_photons(0, 1.0, 2.0);
    require(simulator.state().photons_right[0] == 1.0 &&
                simulator.state().photons_left[0] == 2.0,
            "set_photons did not set the state");
    require_throws<std::out_of_range>(
        [&] { simulator.set_photons(simulator.grid().size(), 1.0, 1.0); }, "outside",
        "cell beyond the grid");
    require_throws<std::invalid_argument>([&] { simulator.set_photons(0, -1.0, 0.0); },
                                          "non-negative", "negative density");
}

void pulse_train_is_split_per_half_round_trip()
{
    // The default cavity emits one pulse every half round trip. The valleys
    // between them stay well above 5 % of the peak, which a fixed cutoff merges.
    cavdyn::Parameters params;
    params.simulation.round_trips = 8.0;
    cavdyn::Simulator simulator(params);
    simulator.run();
    const auto pulses = simulator.results().pulses();
    const double half_round_trip = 0.5 * simulator.grid().round_trip_time();
    require(pulses.size() >= 10, "pulse train was not split into separate pulses");
    double energy = 0.0;
    for (std::size_t i = 0; i < pulses.size(); ++i) {
        energy += pulses[i].energy;
        if (i > 0) {
            require_close(pulses[i].peak_time - pulses[i - 1].peak_time, half_round_trip,
                          0.1, "pulse spacing");
        }
        require(pulses[i].fwhm < half_round_trip, "pulse FWHM spans a whole period");
    }
    require_close(energy, simulator.results().total_output_energy(), 1e-9,
                  "sum of pulse energies");
}

} // namespace

int main(int argc, char** argv)
{
    return run_tests(argc, argv, {
        {"inversion_decays_exponentially", inversion_decays_exponentially},
        {"gaussian_pump_at_start_is_not_lost", gaussian_pump_at_start_is_not_lost},
        {"initial_inversion_scales_with_pump_fraction",
         initial_inversion_scales_with_pump_fraction},
        {"initial_inversion_waits_for_delay", initial_inversion_waits_for_delay},
        {"pump_power_follows_the_pulse", pump_power_follows_the_pulse},
        {"pump_power_continuous_and_direct", pump_power_continuous_and_direct},
        {"molecule_density_clamps_inversion", molecule_density_clamps_inversion},
        {"emission_and_snapshots", emission_and_snapshots},
        {"observer_sees_every_step_and_can_cancel", observer_sees_every_step_and_can_cancel},
        {"set_photons_checks_range", set_photons_checks_range},
        {"pulse_train_is_split_per_half_round_trip", pulse_train_is_split_per_half_round_trip},
    });
}
