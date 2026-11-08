// Configuration reading and result writing.

#include "ConfigReader.h"
#include "ResultWriter.h"
#include "TestSupport.h"

#include "cavdyn/Simulator.h"

#include <filesystem>
#include <fstream>
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

void defaults_round_trip()
{
    const auto params = parse(default_config());
    require(params.cavity.length == 0.015, "cavity.length was not parsed");
    require(params.simulation.cells_in_gain == 60, "simulation.cells_in_gain was not parsed");
    require(!params.pump.initial_inversion && !params.pump.pulse_energy && !params.pump.power,
            "the default configuration should not select a pump mode");
    const cavdyn::Simulator simulator(params);
    require(simulator.parameters().pump.initial_inversion ==
                cavdyn::Parameters::default_initial_inversion,
            "the simulator did not apply the default initial inversion");
}

void defaults_accept_another_pump_mode()
{
    // Enabling a pump mode in the generated file must not clash with a default.
    std::string text = default_config();
    text.replace(text.find("# pulse_energy = 1e-6"), 21, "pulse_energy = 1e-6");
    const auto pulsed = parse(text);
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

    std::size_t rows = 0;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.front() != '#') {
            ++rows;
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

} // namespace

int main(int argc, char** argv)
{
    return run_tests(argc, argv, {
        {"defaults_round_trip", defaults_round_trip},
        {"defaults_accept_another_pump_mode", defaults_accept_another_pump_mode},
        {"unknown_key_rejected", unknown_key_rejected},
        {"duplicate_and_malformed_rejected", duplicate_and_malformed_rejected},
        {"infinity_parsed", infinity_parsed},
        {"writer_header_and_snapshots", writer_header_and_snapshots},
    });
}
