// Configuration reading and result writing.

#include "ConfigReader.h"
#include "ResultWriter.h"
#include "TestSupport.h"

#include "cavdyn/Simulator.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

using namespace cavdyn_test;

namespace {

cavdyn::Parameters parse(const std::string& text)
{
    std::istringstream input(text);
    return cavdyn::read_config(input, "test.conf");
}

std::string default_config()
{
    std::ostringstream output;
    cavdyn::write_default_config(output);
    return output.str();
}

/// Lists every field that differs between two parameter sets.
std::string differences(const cavdyn::Parameters& a, const cavdyn::Parameters& b)
{
    std::string result;
    const auto check = [&](const char* name, auto x, auto y) {
        if (!(x == y)) {
            result += std::string(" ") + name;
        }
    };
    check("cavity.length", a.cavity.length, b.cavity.length);
    check("cavity.r_hr", a.cavity.r_hr, b.cavity.r_hr);
    check("cavity.r_oc", a.cavity.r_oc, b.cavity.r_oc);
    check("cavity.n_background", a.cavity.n_background, b.cavity.n_background);
    check("cavity.mode_area", a.cavity.mode_area, b.cavity.mode_area);
    check("gain_medium.start", a.gain_medium.start, b.gain_medium.start);
    check("gain_medium.length", a.gain_medium.length, b.gain_medium.length);
    check("gain_medium.n_medium", a.gain_medium.n_medium, b.gain_medium.n_medium);
    check("gain_medium.cross_section", a.gain_medium.cross_section,
          b.gain_medium.cross_section);
    check("gain_medium.lifetime", a.gain_medium.lifetime, b.gain_medium.lifetime);
    check("gain_medium.quantum_yield", a.gain_medium.quantum_yield,
          b.gain_medium.quantum_yield);
    check("gain_medium.beta", a.gain_medium.beta, b.gain_medium.beta);
    check("gain_medium.internal_loss", a.gain_medium.internal_loss,
          b.gain_medium.internal_loss);
    check("gain_medium.molecule_density", a.gain_medium.molecule_density,
          b.gain_medium.molecule_density);
    check("gain_medium.emission_wavelength", a.gain_medium.emission_wavelength,
          b.gain_medium.emission_wavelength);
    check("pump.wavelength", a.pump.wavelength, b.pump.wavelength);
    check("pump.start", a.pump.start, b.pump.start);
    check("pump.length", a.pump.length, b.pump.length);
    check("pump.pulse_energy", a.pump.pulse_energy, b.pump.pulse_energy);
    check("pump.pulse_duration", a.pump.pulse_duration, b.pump.pulse_duration);
    check("pump.power", a.pump.power, b.pump.power);
    check("pump.delay", a.pump.delay, b.pump.delay);
    check("pump.absorbed_fraction", a.pump.absorbed_fraction, b.pump.absorbed_fraction);
    check("pump.efficiency", a.pump.efficiency, b.pump.efficiency);
    check("pump.initial_inversion", a.pump.initial_inversion, b.pump.initial_inversion);
    check("simulation.cells_in_gain", a.simulation.cells_in_gain,
          b.simulation.cells_in_gain);
    check("simulation.duration", a.simulation.duration, b.simulation.duration);
    check("simulation.round_trips", a.simulation.round_trips, b.simulation.round_trips);
    check("simulation.snapshot_interval", a.simulation.snapshot_interval,
          b.simulation.snapshot_interval);
    return result;
}

void defaults_match_parameters()
{
    // The generated file reads back as exactly the defaults in Parameters.h.
    const std::string diff = differences(parse(default_config()), cavdyn::Parameters{});
    require(diff.empty(), "defaults file differs from Parameters.h in:" + diff);

    // Missing keys would also read back as defaults, so check that every key is
    // written: set ones as "key = value", unset optional ones as "# key =".
    const std::set<std::string> all_keys = {
        "cavity.length", "cavity.r_hr", "cavity.r_oc", "cavity.n_background",
        "cavity.mode_area", "gain_medium.start", "gain_medium.length",
        "gain_medium.n_medium", "gain_medium.cross_section", "gain_medium.lifetime",
        "gain_medium.quantum_yield", "gain_medium.beta", "gain_medium.internal_loss",
        "gain_medium.molecule_density", "gain_medium.emission_wavelength",
        "pump.wavelength", "pump.start", "pump.length", "pump.pulse_energy",
        "pump.pulse_duration", "pump.power", "pump.delay", "pump.absorbed_fraction",
        "pump.efficiency", "pump.initial_inversion", "simulation.cells_in_gain",
        "simulation.duration", "simulation.round_trips", "simulation.snapshot_interval"};
    std::set<std::string> written;
    std::string section;
    std::istringstream lines(default_config());
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.front() == '[') {
            section = line.substr(1, line.find(']') - 1);
            continue;
        }
        const std::string body = line.rfind("# ", 0) == 0 ? line.substr(2) : line;
        const auto equals = body.find(" = ");
        if (equals != std::string::npos && body.find(' ') == equals) {
            written.insert(section + "." + body.substr(0, equals));
        }
    }
    require(written == all_keys, "defaults file does not list every parameter exactly once");
    const cavdyn::Simulator simulator(parse(default_config()));
    require(simulator.parameters().pump.initial_inversion ==
                cavdyn::Parameters::default_initial_inversion,
            "the simulator did not apply the default initial inversion");
}

void empty_config_gives_defaults()
{
    const std::string diff = differences(parse("# nothing here\n"), cavdyn::Parameters{});
    require(diff.empty(), "an empty configuration changed:" + diff);
}

void partial_config_overrides_only_its_keys()
{
    const auto params = parse("[gain_medium]\nlifetime = 1e-10\n"
                              "[simulation]\nround_trips = 10\n");
    cavdyn::Parameters expected;
    expected.gain_medium.lifetime = 1e-10;
    expected.simulation.round_trips = 10.0;
    const std::string diff = differences(params, expected);
    require(diff.empty(), "a partial configuration changed other values:" + diff);
}

void defaults_accept_another_pump_mode()
{
    // Enabling a pump mode in the generated file must not clash with a default.
    const auto pulsed = parse(default_config() + "[pump]\npulse_energy = 1e-6\n");
    require(pulsed.pump.pulse_energy == 1e-6, "pump.pulse_energy was not parsed");
}

void unknown_key_rejected()
{
    require_throws<cavdyn::ParameterError>(
        [] { parse("[cavity]\nunknown = 2\n"); }, "unknown key 'cavity.unknown'",
        "unknown key");
    require_throws<cavdyn::ParameterError>(
        [] { parse("[lasers]\n"); }, "unknown section", "unknown section");
}

void duplicate_and_malformed_rejected()
{
    require_throws<cavdyn::ParameterError>(
        [] { parse("[cavity]\nr_oc = 0.9\nr_oc = 0.8\n"); }, "duplicate key",
        "duplicate key");
    require_throws<cavdyn::ParameterError>(
        [] { parse("[cavity]\nr_oc = 0.9x\n"); }, "unexpected characters",
        "trailing characters");
    require_throws<cavdyn::ParameterError>(
        [] { parse("r_oc = 0.9\n"); }, "expected a section", "key outside a section");
    require_throws<cavdyn::ParameterError>(
        [] { parse("[simulation]\ncells_in_gain = -3\n"); }, "non-negative integer",
        "negative integer");
}

void infinity_parsed()
{
    const auto params = parse("[gain_medium]\nlifetime = inf\n[pump]\npower = 1\n"
                              "pulse_duration = inf\n");
    require(std::isinf(params.gain_medium.lifetime), "lifetime = inf was not parsed");
    require(std::isinf(params.pump.pulse_duration), "pulse_duration = inf was not parsed");
}

void writer_header_and_snapshots()
{
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 8;
    params.simulation.round_trips = 1.0;
    params.simulation.snapshot_interval = 4;
    cavdyn::Simulator simulator(params);
    simulator.run();

    const auto directory = std::filesystem::temp_directory_path();
    const auto path = directory / "cavdyn-test-output.dat";
    const auto snapshots = directory / "cavdyn-test-output.snapshots.dat";
    cavdyn::write_results(path.string(), simulator.parameters(), simulator.grid(),
                          simulator.results());

    std::ifstream output(path);
    std::stringstream contents;
    contents << output.rdbuf();
    const std::string text = contents.str();
    require(text.rfind("# cavity-dynamics " CAVDYN_TEST_VERSION "\n", 0) == 0,
            "header does not start with the project version");
    require(text.find("# pump.initial_inversion = 1.0000000000e+23") != std::string::npos,
            "header does not show the default initial inversion actually used");
    require(text.find("# grid.gain_cells = 8") != std::string::npos,
            "header does not list the cells per region");

    require(text.find("# time_s\toutput_photons\toutput_power_W\tcavity_photons\t"
                      "total_inversion\tpump_power_W\n") != std::string::npos,
            "header does not name the six columns");

    std::size_t rows = 0;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.front() != '#') {
            ++rows;
            require(std::count(line.begin(), line.end(), '\t') == 5,
                    "row does not have six columns: " + line);
        }
    }
    require(rows == simulator.results().steps().size(), "one row per step was not written");

    std::ifstream snapshot_file(snapshots);
    std::size_t blocks = 0;
    for (std::string line; std::getline(snapshot_file, line);) {
        if (line.rfind("# time_s", 0) == 0) {
            ++blocks;
        }
    }
    require(blocks == simulator.results().snapshots().size(),
            "snapshot file does not have one block per snapshot");
    std::filesystem::remove(path);
    std::filesystem::remove(snapshots);
}

void writer_reports_pump_pulse_energy()
{
    // With pump.pulse_energy the sixth column is the energy per step in J, and
    // the column sums to the pulse energy.
    cavdyn::Parameters params;
    params.simulation.cells_in_gain = 8;
    params.simulation.round_trips = 1.0;
    params.pump.pulse_energy = 1e-6;
    params.pump.pulse_duration = 2e-12;
    params.pump.delay = 3e-12;
    cavdyn::Simulator simulator(params);
    simulator.run();

    const auto path = std::filesystem::temp_directory_path() / "cavdyn-test-pulsed.dat";
    cavdyn::write_results(path.string(), simulator.parameters(), simulator.grid(),
                          simulator.results());
    std::ifstream output(path);
    std::string header;
    double energy = 0.0;
    for (std::string line; std::getline(output, line);) {
        if (line.rfind("# time_s", 0) == 0) {
            header = line;
        } else if (!line.empty() && line.front() != '#') {
            std::istringstream row(line);
            double column[6]{};
            for (double& value : column) {
                row >> value;
            }
            energy += column[5];
        }
    }
    std::filesystem::remove(path);
    require(header.size() > 14 &&
                header.compare(header.size() - 14, 14, "\tpump_energy_J") == 0,
            "header does not name the pump_energy_J column: " + header);
    require_close(energy, *params.pump.pulse_energy, 1e-6, "pump energy column sum");
}

} // namespace

int main(int argc, char** argv)
{
    return run_tests(argc, argv, {
        {"defaults_match_parameters", defaults_match_parameters},
        {"empty_config_gives_defaults", empty_config_gives_defaults},
        {"partial_config_overrides_only_its_keys", partial_config_overrides_only_its_keys},
        {"defaults_accept_another_pump_mode", defaults_accept_another_pump_mode},
        {"unknown_key_rejected", unknown_key_rejected},
        {"duplicate_and_malformed_rejected", duplicate_and_malformed_rejected},
        {"infinity_parsed", infinity_parsed},
        {"writer_header_and_snapshots", writer_header_and_snapshots},
        {"writer_reports_pump_pulse_energy", writer_reports_pump_pulse_energy},
    });
}
