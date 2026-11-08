#include "ConfigReader.h"
#include "ResultWriter.h"

#include "cavdyn/Simulator.h"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void usage(std::ostream& output)
{
    output << "Usage:\n"
           << "  cavity-dynamics defaults\n"
           << "  cavity-dynamics run config.txt [-o output.dat]\n";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "defaults") {
            cavdyn::write_default_config(std::cout);
            return 0;
        }
        if (argc < 3 || std::string(argv[1]) != "run") {
            usage(std::cerr);
            return 2;
        }
        std::string output_path = "output.dat";
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-o" && i + 1 < argc) {
                output_path = argv[++i];
            } else {
                throw std::invalid_argument("Unknown or incomplete option: " +
                                            std::string(argv[i]));
            }
        }

        const cavdyn::Parameters params = cavdyn::read_config_file(argv[2]);
        cavdyn::Simulator simulator(params);
        std::cout << std::scientific << std::setprecision(6)
                  << "Time step: " << simulator.grid().time_step() << " s\n"
                  << "Cells: " << simulator.grid().size()
                  << " (" << simulator.grid().left_background_cells() << " background, "
                  << simulator.grid().gain_cells() << " gain, "
                  << simulator.grid().right_background_cells() << " background)\n"
                  << "Actual cavity length: " << simulator.grid().actual_length() << " m\n"
                  << "Round-trip time: " << simulator.grid().round_trip_time() << " s\n";
        simulator.run();
        cavdyn::write_results(output_path, simulator.parameters(), simulator.grid(), simulator.results());

        const auto pulses = simulator.results().pulses();
        std::cout << "Wrote " << simulator.results().steps().size()
                  << " steps to " << output_path << '\n'
                  << "Detected pulses: " << pulses.size() << '\n';
        for (std::size_t i = 0; i < pulses.size(); ++i) {
            std::cout << "  " << (i + 1) << ": peak=" << pulses[i].peak_time
                      << " s, power=" << pulses[i].peak_power
                      << " W, energy=" << pulses[i].energy
                      << " J, FWHM=" << pulses[i].fwhm << " s\n";
            if (i > 0) {
                std::cout << "     spacing=" << pulses[i].peak_time -
                    pulses[i - 1].peak_time << " s\n";
            }
        }
        std::cout << "Total output energy: "
                  << simulator.results().total_output_energy() << " J\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cavity-dynamics: " << error.what() << '\n';
        return 1;
    }
}
