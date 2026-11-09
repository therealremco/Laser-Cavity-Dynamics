#include "ResultWriter.h"

#include "cavdyn/Constants.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#ifndef CAVDYN_VERSION
#define CAVDYN_VERSION "unknown"
#endif

namespace cavdyn {
namespace {

std::string timestamp()
{
    const auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm time{};
#if defined(_WIN32)
    gmtime_s(&time, &now);
#else
    gmtime_r(&now, &time);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &time);
    return buffer;
}

std::string snapshot_path(const std::string& path)
{
    const auto dot = path.find_last_of('.');
    const auto slash = path.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return path.substr(0, dot) + ".snapshots.dat";
    }
    return path + ".snapshots.dat";
}

} // namespace

void write_results(const std::string& path, const Parameters& params,
                   const Grid& grid, const Results& results)
{
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Could not open output file: " + path);
    }
    // A pump pulse is reported as the energy incident in each step, which sums to
    // pump.pulse_energy; the other modes report the incident power.
    const bool pulsed_pump = params.pump.pulse_energy.has_value();
    output << std::scientific << std::setprecision(10)
        << "# cavity-dynamics " << CAVDYN_VERSION << '\n'
        << "# generated_utc = " << timestamp() << '\n'
        << "# cavity.length = " << params.cavity.length << '\n'
        << "# cavity.r_hr = " << params.cavity.r_hr << '\n'
        << "# cavity.r_oc = " << params.cavity.r_oc << '\n'
        << "# cavity.n_background = " << params.cavity.n_background << '\n'
        << "# cavity.mode_area = " << params.cavity.mode_area << '\n'
        << "# gain_medium.start = " << params.gain_medium.start << '\n'
        << "# gain_medium.length = " << params.gain_medium.length << '\n'
        << "# gain_medium.n_medium = " << params.gain_medium.n_medium << '\n'
        << "# gain_medium.cross_section = " << params.gain_medium.cross_section << '\n'
        << "# gain_medium.lifetime = " << params.gain_medium.lifetime << '\n'
        << "# gain_medium.quantum_yield = " << params.gain_medium.quantum_yield << '\n'
        << "# gain_medium.beta = " << params.gain_medium.beta << '\n'
        << "# gain_medium.internal_loss = " << params.gain_medium.internal_loss << '\n'
        << "# gain_medium.molecule_density = " << params.gain_medium.molecule_density << '\n'
        << "# gain_medium.emission_wavelength = " << params.gain_medium.emission_wavelength << '\n'
        << "# pump.wavelength = " << params.pump.wavelength << '\n'
        << "# pump.start = " << params.pump.start << '\n'
        << "# pump.length = " << params.pump.length << '\n'
        << "# pump.pulse_energy = ";
    if (params.pump.pulse_energy) output << *params.pump.pulse_energy;
    else output << "unset";
    output << "\n# pump.pulse_duration = " << params.pump.pulse_duration
        << "\n# pump.power = ";
    if (params.pump.power) output << *params.pump.power;
    else output << "unset";
    output << "\n# pump.delay = " << params.pump.delay
        << "\n# pump.absorbed_fraction = " << params.pump.absorbed_fraction
        << "\n# pump.efficiency = " << params.pump.efficiency
        << "\n# pump.initial_inversion = ";
    if (params.pump.initial_inversion) output << *params.pump.initial_inversion;
    else output << "unset";
    output << "\n# simulation.cells_in_gain = " << params.simulation.cells_in_gain
        << "\n# simulation.duration = ";
    if (params.simulation.duration) output << *params.simulation.duration;
    else output << "unset";
    output << "\n# simulation.round_trips = " << params.simulation.round_trips
        << "\n# simulation.snapshot_interval = " << params.simulation.snapshot_interval
        << "\n# grid.time_step = " << grid.time_step()
        << "\n# grid.cells = " << grid.size()
        << "\n# grid.left_background_cells = " << grid.left_background_cells()
        << "\n# grid.gain_cells = " << grid.gain_cells()
        << "\n# grid.right_background_cells = " << grid.right_background_cells()
        << "\n# grid.actual_length = " << grid.actual_length()
        << "\n# grid.round_trip_time = " << grid.round_trip_time()
        << "\n# time_s\toutput_photons\toutput_power_W\tcavity_photons\ttotal_inversion\t"
        << (pulsed_pump ? "pump_energy_J" : "pump_power_W") << '\n';
    for (const StepResult& result : results.steps()) {
        output << result.time << '\t' << result.output_photons << '\t'
               << result.output_power << '\t' << result.cavity_photons << '\t'
               << result.total_inversion << '\t'
               << (pulsed_pump ? result.pump_power * grid.time_step() : result.pump_power)
               << '\n';
    }
    output.close();
    if (!output) {
        throw std::runtime_error("Failed while writing output file: " + path);
    }

    if (!results.snapshots().empty()) {
        const std::string path_snapshots = snapshot_path(path);
        std::ofstream snapshots(path_snapshots);
        if (!snapshots) {
            throw std::runtime_error("Could not open snapshots file: " + path_snapshots);
        }
        snapshots << std::scientific << std::setprecision(10);
        for (const Snapshot& snapshot : results.snapshots()) {
            snapshots << "# time_s = " << snapshot.time << '\n'
                      << "# z_m\tN\tJ_plus\tJ_minus\n";
            for (std::size_t i = 0; i < grid.size(); ++i) {
                snapshots << grid.cells()[i].z_start + 0.5 * grid.cells()[i].length
                          << '\t' << snapshot.inversion[i] << '\t'
                          << snapshot.photons_right[i] << '\t'
                          << snapshot.photons_left[i] << '\n';
            }
            snapshots << '\n';
        }
        snapshots.close();
        if (!snapshots) {
            throw std::runtime_error("Failed while writing snapshots file: " + path_snapshots);
        }
    }
}

} // namespace cavdyn
