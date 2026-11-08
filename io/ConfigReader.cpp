#include "ConfigReader.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace cavdyn {
namespace {

std::string trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

double parse_double(const std::string& text, const std::string& where)
{
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception&) {
        throw ParameterError(where + ": expected a number, got '" + text + "'");
    }
    if (consumed != text.size()) {
        throw ParameterError(where + ": unexpected characters in '" + text + "'");
    }
    return value;
}

std::size_t parse_size(const std::string& text, const std::string& where)
{
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
        throw ParameterError(where + ": expected a non-negative integer, got '" + text + "'");
    }
    std::size_t consumed = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(text, &consumed);
    } catch (const std::exception&) {
        throw ParameterError(where + ": expected a non-negative integer, got '" + text + "'");
    }
    if (consumed != text.size() ||
        value > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        throw ParameterError(where + ": invalid integer '" + text + "'");
    }
    return static_cast<std::size_t>(value);
}

void assign(Parameters& params, const std::string& section, const std::string& key,
            const std::string& value, const std::string& where)
{
    const std::string full = section + "." + key;
    const double number = key == "cells_in_gain" || key == "snapshot_interval"
        ? 0.0 : parse_double(value, where);
    if (section == "cavity") {
        if (key == "length") params.cavity.length = number;
        else if (key == "r_hr") params.cavity.r_hr = number;
        else if (key == "r_oc") params.cavity.r_oc = number;
        else if (key == "n_background") params.cavity.n_background = number;
        else if (key == "mode_area") params.cavity.mode_area = number;
        else throw ParameterError(where + ": unknown key '" + full + "'");
    } else if (section == "gain_medium") {
        if (key == "start") params.gain_medium.start = number;
        else if (key == "length") params.gain_medium.length = number;
        else if (key == "n_medium") params.gain_medium.n_medium = number;
        else if (key == "cross_section") params.gain_medium.cross_section = number;
        else if (key == "lifetime") params.gain_medium.lifetime = number;
        else if (key == "quantum_yield") params.gain_medium.quantum_yield = number;
        else if (key == "beta") params.gain_medium.beta = number;
        else if (key == "internal_loss") params.gain_medium.internal_loss = number;
        else if (key == "molecule_density") params.gain_medium.molecule_density = number;
        else if (key == "emission_wavelength") params.gain_medium.emission_wavelength = number;
        else throw ParameterError(where + ": unknown key '" + full + "'");
    } else if (section == "pump") {
        if (key == "wavelength") params.pump.wavelength = number;
        else if (key == "start") params.pump.start = number;
        else if (key == "length") params.pump.length = number;
        else if (key == "pulse_energy") params.pump.pulse_energy = number;
        else if (key == "pulse_duration") params.pump.pulse_duration = number;
        else if (key == "power") params.pump.power = number;
        else if (key == "delay") params.pump.delay = number;
        else if (key == "absorbed_fraction") params.pump.absorbed_fraction = number;
        else if (key == "efficiency") params.pump.efficiency = number;
        else if (key == "initial_inversion") params.pump.initial_inversion = number;
        else throw ParameterError(where + ": unknown key '" + full + "'");
    } else if (section == "simulation") {
        if (key == "cells_in_gain") params.simulation.cells_in_gain = parse_size(value, where);
        else if (key == "duration") params.simulation.duration = number;
        else if (key == "round_trips") params.simulation.round_trips = number;
        else if (key == "snapshot_interval") params.simulation.snapshot_interval = parse_size(value, where);
        else throw ParameterError(where + ": unknown key '" + full + "'");
    } else {
        throw ParameterError(where + ": unknown section '" + section + "'");
    }
}

} // namespace

Parameters read_config(std::istream& input, const std::string& source)
{
    Parameters params;
    std::map<std::string, std::size_t> seen;
    std::string section;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        const std::string where = source + ":" + std::to_string(line_number);
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            if (section != "cavity" && section != "gain_medium" &&
                section != "pump" && section != "simulation") {
                throw ParameterError(where + ": unknown section '" + section + "'");
            }
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos || section.empty()) {
            throw ParameterError(where + ": expected a section and 'key = value'");
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            throw ParameterError(where + ": key and value must not be empty");
        }
        const std::string full = section + "." + key;
        if (++seen[full] > 1) {
            throw ParameterError(where + ": duplicate key '" + full + "'");
        }
        assign(params, section, key, value, where);
    }
    if (input.bad()) {
        throw std::runtime_error("Failed reading configuration from " + source);
    }
    params.validate();
    return params;
}

Parameters read_config_file(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Could not open configuration file: " + path);
    }
    return read_config(input, path);
}

void write_default_config(std::ostream& output)
{
    output << std::setprecision(17)
        << "# Laser cavity dynamics configuration (SI units)\n"
        << "[cavity]\n"
        << "length = 0.015\n"
        << "r_hr = 1\n"
        << "r_oc = 0.95\n"
        << "n_background = 1\n"
        << "mode_area = 1e-8\n\n"
        << "[gain_medium]\n"
        << "start = 0.003\n"
        << "length = 0.009\n"
        << "n_medium = 1.5\n"
        << "cross_section = 1e-20\n"
        << "lifetime = 1e-9\n"
        << "quantum_yield = 1\n"
        << "beta = 1e-4\n"
        << "internal_loss = 0\n"
        << "molecule_density = 0\n"
        << "emission_wavelength = 5.5e-7\n\n"
        << "[pump]\n"
        << "# Choose at most one pump mode: pulse_energy (J), power (W, with\n"
        << "# pulse_duration = inf) or initial_inversion (m^-3). With none,\n"
        << "# initial_inversion = 1e23 is used.\n"
        << "wavelength = 4e-7\n"
        << "start = 0.003\n"
        << "length = 0.009\n"
        << "pulse_duration = 1e-13\n"
        << "delay = 0\n"
        << "absorbed_fraction = 1\n"
        << "efficiency = 0.8\n"
        << "# pulse_energy = 1e-6\n"
        << "# power = 1\n"
        << "# initial_inversion = 1e23\n\n"
        << "[simulation]\n"
        << "cells_in_gain = 60\n"
        << "round_trips = 25\n"
        << "snapshot_interval = 0\n";
    if (!output) {
        throw std::runtime_error("Failed writing default configuration");
    }
}

} // namespace cavdyn
