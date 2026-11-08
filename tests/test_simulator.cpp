#include "ConfigReader.h"
#include "ResultWriter.h"

#include "cavdyn/Cavity.h"
#include "cavdyn/Constants.h"
#include "cavdyn/GainMedium.h"
#include "cavdyn/Pump.h"
#include "cavdyn/Simulator.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_config_round_trip()
{
    std::ostringstream serialized;
    cavdyn::write_default_config(serialized);
    std::istringstream input(serialized.str());
    const auto params = cavdyn::read_config(input);
    require(params.cavity.length == 0.015, "default cavity length was not parsed");
    require(params.simulation.cells_in_gain == 60, "default gain grid was not parsed");
    require(params.pump.initial_inversion == 1e23, "default inversion was not parsed");
}

void test_invalid_config_rejected()
{
    std::istringstream input("[cavity]\nunknown = 2\n");
    bool rejected = false;
    try {
        (void)cavdyn::read_config(input, "bad.conf");
    } catch (const cavdyn::ParameterError& error) {
        rejected = std::string(error.what()).find("unknown key") != std::string::npos;
    }
    require(rejected, "unknown configuration key was not rejected explicitly");
}

void test_grid_and_decay()
{
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 12;
    params.simulation.duration = 2.0e-12;
    params.gain_medium.beta = 0.0;
    cavdyn::Simulator simulator(params);
    require(simulator.grid().size() > params.simulation.cells_in_gain,
            "grid should include background cavity cells");
    require(std::abs(simulator.grid().round_trip_time() -
                     2.0 * simulator.grid().size() * simulator.grid().time_step()) < 1e-25,
            "round-trip time does not match the equal-transit-time grid");

    const double initial_inversion = 1e23;
    const double initial_total = initial_inversion *
        params.gain_medium.length * params.cavity.mode_area;
    simulator.step();
    const double expected = initial_total *
        std::exp(-simulator.grid().time_step() / params.gain_medium.lifetime);
    const double actual = simulator.results().steps().back().total_inversion;
    require(std::abs(actual - expected) / expected < 1e-12,
            "inversion did not decay exponentially over one time step");
}

void test_emission_and_snapshot()
{
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 12;
    params.simulation.round_trips = 2.0;
    params.simulation.snapshot_interval = 2;
    params.gain_medium.beta = 0.05;
    cavdyn::Simulator simulator(params);
    simulator.run();
    require(!simulator.results().steps().empty(), "simulation produced no steps");
    require(!simulator.results().snapshots().empty(), "requested snapshots were not recorded");
    require(simulator.results().total_output_energy() > 0.0,
            "spontaneous seed did not produce output");
    require(!simulator.results().pulses().empty(), "output pulse analysis found no pulse");
}

void test_observer_cancellation_and_writing()
{
    struct StopAfterOne final : cavdyn::Observer {
        bool on_step(const cavdyn::StepResult&, cavdyn::StateView) override
        {
            return false;
        }
    } observer;

    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 8;
    params.simulation.round_trips = 1.0;
    params.simulation.snapshot_interval = 1;
    cavdyn::Simulator simulator(params);
    simulator.run(&observer);
    require(simulator.step_index() == 1, "observer cancellation did not stop the run");

    const auto path = std::filesystem::temp_directory_path() / "cavdyn-test-output.dat";
    const auto snapshots = std::filesystem::temp_directory_path() /
                           "cavdyn-test-output.snapshots.dat";
    cavdyn::write_results(path.string(), params, simulator.grid(), simulator.results());
    std::ifstream output(path);
    std::string header;
    std::getline(output, header);
    require(header.find("cavity-dynamics") != std::string::npos,
            "result file is missing its metadata header");
    require(std::filesystem::exists(snapshots), "snapshot result file was not written");
    std::filesystem::remove(path);
    std::filesystem::remove(snapshots);
}

void test_pump_mode_validation()
{
    cavdyn::Parameters params;
    params.pump.power = 1.0;
    params.pump.pulse_duration = std::numeric_limits<double>::infinity();
    params.pump.initial_inversion = 1e23;
    bool rejected = false;
    try {
        params.validate();
    } catch (const cavdyn::ParameterError&) {
        rejected = true;
    }
    require(rejected, "mutually exclusive pump modes were not rejected");
}

void test_gain_conservation_and_cavity_coupling()
{
    cavdyn::GainMediumParams gain_params;
    cavdyn::GainMedium gain(gain_params);
    const double length = 1e-4;
    double inversion = 1e23;
    double photons = 1e18;
    const double conserved_before = inversion * length + photons;
    gain.amplify(photons, inversion, length);
    const double conserved_after = inversion * length + photons;
    require(std::abs(conserved_after - conserved_before) / conserved_before < 1e-14,
            "Frantz-Nodvik amplification did not conserve photons plus inversion");
    require(inversion >= 0.0, "Frantz-Nodvik amplification made inversion negative");

    cavdyn::CavityParams cavity_params;
    cavity_params.r_hr = 0.9;
    cavity_params.r_oc = 0.8;
    cavity_params.mode_area = 2.0;
    const cavdyn::Cavity cavity(cavity_params);
    require(cavity.reflect_at_high_reflector(10.0) == 9.0,
            "high-reflector coupling is incorrect");
    require(cavity.reflect_at_output_coupler(10.0) == 8.0,
            "output-coupler reflection is incorrect");
    require(std::abs(cavity.output_photons(10.0) - 4.0) < 1e-14,
            "output coupling is incorrect");
}

void test_pump_energy_conversion()
{
    cavdyn::PumpParams params;
    params.pulse_energy = 1e-6;
    params.pulse_duration = 0.0;
    params.initial_inversion.reset();
    const double area = 1e-8;
    const cavdyn::Pump pump(params, area);
    const double actual = pump.deposition_density(0.0, 1e-12);
    const double photon_energy =
        cavdyn::planck_constant * cavdyn::speed_of_light / params.wavelength;
    const double expected = params.efficiency * params.absorbed_fraction *
        *params.pulse_energy / (photon_energy * params.length * area);
    require(std::abs(actual - expected) / expected < 1e-14,
            "pump energy was not converted to inversion density correctly");
}

double inversion_after_one_step(const cavdyn::Parameters& params)
{
    cavdyn::Simulator simulator(params);
    simulator.step();
    return simulator.results().steps().back().total_inversion;
}

void test_gaussian_pump_at_start_is_not_lost()
{
    // A pulse centred on t = 0 must deposit all of its energy, not only the
    // half that arrives after the simulation starts.
    cavdyn::Parameters params;
    params.pump.pulse_energy = 1e-6;
    params.gain_medium.beta = 0.0;
    params.gain_medium.lifetime = std::numeric_limits<double>::infinity();
    params.simulation.round_trips = 1.0;
    const double photon_energy =
        cavdyn::planck_constant * cavdyn::speed_of_light / params.pump.wavelength;
    const double expected = params.pump.efficiency * params.pump.absorbed_fraction *
                            *params.pump.pulse_energy / photon_energy;
    const double actual = inversion_after_one_step(params);
    require(std::abs(actual - expected) / expected < 1e-9,
            "pump energy before t = 0 was lost");
}

void test_initial_inversion_scales_with_pump_fraction()
{
    // A stripe of 29.33 gain cells must hold N_0 · L_p · A, not N_0 in 30 full cells.
    cavdyn::Parameters params;
    params.pump.length = 4.4e-3;
    params.gain_medium.beta = 0.0;
    params.gain_medium.lifetime = std::numeric_limits<double>::infinity();
    params.simulation.round_trips = 1.0;
    const double expected = 1e23 * params.pump.length * params.cavity.mode_area;
    const double actual = inversion_after_one_step(params);
    require(std::abs(actual - expected) / expected < 1e-9,
            "initial inversion over-filled partially pumped cells");
}

void test_initial_inversion_waits_for_delay()
{
    cavdyn::Parameters params;
    params.pump.delay = 5e-12;
    params.gain_medium.beta = 0.0;
    params.gain_medium.lifetime = std::numeric_limits<double>::infinity();
    params.simulation.duration = 1e-11;
    cavdyn::Simulator simulator(params);
    simulator.run();
    const double dt = simulator.grid().time_step();
    const double expected = 1e23 * params.pump.length * params.cavity.mode_area;
    for (const auto& step : simulator.results().steps()) {
        const double interval_end = step.time;
        if (interval_end <= params.pump.delay) {
            require(step.total_inversion == 0.0, "inversion appeared before pump.delay");
        } else if (interval_end - dt > params.pump.delay) {
            require(std::abs(step.total_inversion - expected) / expected < 1e-9,
                    "initial inversion was not set at pump.delay");
        }
    }
}

void test_pulse_train_is_split_per_half_round_trip()
{
    // The default cavity emits one pulse every half round trip; the valleys
    // between them stay well above 5 % of the peak, which the old detector
    // merged into a single pulse.
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
            const double spacing = pulses[i].peak_time - pulses[i - 1].peak_time;
            require(std::abs(spacing - half_round_trip) < 0.1 * half_round_trip,
                    "pulse spacing is not half a round trip");
        }
        require(pulses[i].fwhm < half_round_trip, "pulse FWHM spans a whole period");
    }
    const double total = simulator.results().total_output_energy();
    require(std::abs(energy - total) / total < 1e-9,
            "pulse energies do not add up to the total output energy");
}

} // namespace

int main()
{
    try {
        test_config_round_trip();
        test_invalid_config_rejected();
        test_grid_and_decay();
        test_emission_and_snapshot();
        test_observer_cancellation_and_writing();
        test_pump_mode_validation();
        test_gain_conservation_and_cavity_coupling();
        test_pump_energy_conversion();
        test_gaussian_pump_at_start_is_not_lost();
        test_initial_inversion_scales_with_pump_fraction();
        test_initial_inversion_waits_for_delay();
        test_pulse_train_is_split_per_half_round_trip();
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
    std::cout << "All cavdyn tests passed.\n";
    return 0;
}
