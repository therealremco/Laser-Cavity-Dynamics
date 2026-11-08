#include "ConfigReader.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
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

/// Shortest decimal text that reads back as exactly the same double.
std::string format_number(double value)
{
    if (std::isinf(value)) {
        return value > 0.0 ? "inf" : "-inf";
    }
    std::string text;
    for (int precision = 1; precision <= 17; ++precision) {
        std::ostringstream stream;
        stream << std::setprecision(precision) << value;
        text = stream.str();
        if (std::stod(text) == value) {
            break;
        }
    }
    return text;
}

void write_line(std::ostream& output, bool commented, const std::string& key,
                const std::string& value, const std::string& unit)
{
    std::ostringstream line;
    line << (commented ? "# " : "") << key << " = " << value;
    output << line.str();
    if (!unit.empty()) {
        const std::size_t column = 30;
        const std::size_t length = line.str().size();
        output << std::string(length < column ? column - length : 1, ' ') << "# " << unit;
    }
    output << '\n';
}

void write_entry(std::ostream& output, const std::string& key, double value,
                 const std::string& unit)
{
    write_line(output, false, key, format_number(value), unit);
}

void write_size_entry(std::ostream& output, const std::string& key, std::size_t value,
                      const std::string& unit)
{
    write_line(output, false, key, std::to_string(value), unit);
}

/// An unset optional is written commented out, with its fallback value when it has one.
void write_optional_entry(std::ostream& output, const std::string& key,
                          const std::optional<double>& value,
                          const std::optional<double>& fallback, const std::string& unit)
{
    if (value) {
        write_entry(output, key, *value, unit);
        return;
    }
    write_line(output, true, key, fallback ? format_number(*fallback) : "", unit);
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
    // Every value comes from the defaults in Parameters.h.
    const Parameters defaults;
    const auto& cavity = defaults.cavity;
    const auto& gain = defaults.gain_medium;
    const auto& pump = defaults.pump;
    const auto& simulation = defaults.simulation;

    output << "# Laser cavity dynamics configuration (SI units).\n"
           << "# These are the built-in defaults. Keys left out of a configuration\n"
           << "# keep these values, so a file only needs the keys it changes.\n"
           << "\n[cavity]\n";
    write_entry(output, "length", cavity.length, "m");
    write_entry(output, "r_hr", cavity.r_hr, "");
    write_entry(output, "r_oc", cavity.r_oc, "");
    write_entry(output, "n_background", cavity.n_background, "");
    write_entry(output, "mode_area", cavity.mode_area, "m^2");

    output << "\n[gain_medium]\n";
    write_entry(output, "start", gain.start, "m");
    write_entry(output, "length", gain.length, "m");
    write_entry(output, "n_medium", gain.n_medium, "");
    write_entry(output, "cross_section", gain.cross_section, "m^2");
    write_entry(output, "lifetime", gain.lifetime, "s");
    write_entry(output, "quantum_yield", gain.quantum_yield, "");
    write_entry(output, "beta", gain.beta, "");
    write_entry(output, "internal_loss", gain.internal_loss, "m^-1");
    write_entry(output, "molecule_density", gain.molecule_density, "m^-3, 0 = off");
    write_entry(output, "emission_wavelength", gain.emission_wavelength, "m");

    output << "\n[pump]\n";
    write_entry(output, "wavelength", pump.wavelength, "m");
    write_entry(output, "start", pump.start, "m");
    write_entry(output, "length", pump.length, "m");
    write_entry(output, "pulse_duration", pump.pulse_duration, "s, 0 = instantaneous, inf = continuous");
    write_entry(output, "delay", pump.delay, "s");
    write_entry(output, "absorbed_fraction", pump.absorbed_fraction, "");
    write_entry(output, "efficiency", pump.efficiency, "");
    output << "# Pump mode: set at most one of these. With none, initial_inversion\n"
           << "# takes the value below.\n";
    write_optional_entry(output, "pulse_energy", pump.pulse_energy, std::nullopt, "J");
    write_optional_entry(output, "power", pump.power, std::nullopt,
                         "W, needs pulse_duration = inf");
    write_optional_entry(output, "initial_inversion", pump.initial_inversion,
                         Parameters::default_initial_inversion, "m^-3");

    output << "\n[simulation]\n";
    write_size_entry(output, "cells_in_gain", simulation.cells_in_gain, "");
    write_optional_entry(output, "duration", simulation.duration, std::nullopt,
                         "s, replaces round_trips");
    write_entry(output, "round_trips", simulation.round_trips, "");
    write_size_entry(output, "snapshot_interval", simulation.snapshot_interval,
                     "steps, 0 = off");
    if (!output) {
        throw std::runtime_error("Failed writing default configuration");
    }
}

} // namespace cavdyn
