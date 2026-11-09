// WebAssembly entry point for the browser version (built with Emscripten).
//
// JavaScript calls simulate() with a plain object of the adjustable parameters.
// Everything else keeps the defaults from Parameters.h, with two fixed choices
// for the web version:
//   - the gain medium is 5 mm long and stays centred in the cavity, and the
//     pump covers the whole gain medium;
//   - no snapshots are recorded.

#include "cavdyn/Simulator.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cmath>
#include <exception>
#include <string>
#include <vector>

namespace {

using emscripten::val;

constexpr double gain_length = 5.0e-3; // m

double number(const val& input, const char* key)
{
    const val value = input[key];
    if (!value.isNumber()) {
        throw cavdyn::ParameterError(std::string(key) + " must be a number");
    }
    return value.as<double>();
}

cavdyn::Parameters to_parameters(const val& input)
{
    cavdyn::Parameters params;

    params.cavity.length = number(input, "cavity_length");
    if (params.cavity.length < gain_length) {
        throw cavdyn::ParameterError("cavity length must be at least the 5 mm gain medium");
    }

    params.gain_medium.length = gain_length;
    params.gain_medium.start = 0.5 * (params.cavity.length - gain_length);
    params.gain_medium.cross_section = number(input, "cross_section");
    params.gain_medium.internal_loss = number(input, "internal_loss");

    params.pump.start = params.gain_medium.start;
    params.pump.length = gain_length;
    params.pump.wavelength = number(input, "pump_wavelength");
    params.pump.delay = number(input, "pump_delay");
    params.pump.absorbed_fraction = number(input, "absorbed_fraction");
    params.pump.efficiency = number(input, "efficiency");

    const std::string mode = input["pump_mode"].as<std::string>();
    if (mode == "pulse") {
        params.pump.pulse_energy = number(input, "pulse_energy");
        params.pump.pulse_duration = number(input, "pulse_duration");
    } else if (mode == "continuous") {
        params.pump.power = number(input, "power");
        params.pump.pulse_duration = INFINITY;
    } else if (mode == "initial_inversion") {
        params.pump.initial_inversion = number(input, "initial_inversion");
    } else {
        throw cavdyn::ParameterError("unknown pump mode: " + mode);
    }

    params.simulation.round_trips = number(input, "round_trips");
    params.simulation.snapshot_interval = 0;
    return params;
}

template <typename Field>
val column(const std::vector<cavdyn::StepResult>& steps, Field field)
{
    std::vector<double> values;
    values.reserve(steps.size());
    for (const auto& step : steps) {
        values.push_back(step.*field);
    }
    // Constructing a Float64Array from the memory view copies the data out of
    // the WebAssembly heap, so the result stays valid after this function returns.
    return val::global("Float64Array")
        .new_(emscripten::typed_memory_view(values.size(), values.data()));
}

/// Returns {ok: true, ...results} or {ok: false, error: "..."}.
val simulate(const val& input)
{
    val output = val::object();
    try {
        cavdyn::Simulator simulator(to_parameters(input));
        simulator.run();

        const auto& grid = simulator.grid();
        const auto& results = simulator.results();
        const auto& steps = results.steps();

        output.set("ok", true);
        output.set("time_step", grid.time_step());
        output.set("cells", static_cast<double>(grid.size()));
        output.set("actual_length", grid.actual_length());
        output.set("round_trip_time", grid.round_trip_time());
        output.set("gain_start", simulator.parameters().gain_medium.start);

        // The parameters the page does not let you change, for display.
        const auto& used = simulator.parameters();
        val fixed = val::object();
        fixed.set("r_hr", used.cavity.r_hr);
        fixed.set("r_oc", used.cavity.r_oc);
        fixed.set("n_background", used.cavity.n_background);
        fixed.set("mode_area", used.cavity.mode_area);
        fixed.set("gain_length", used.gain_medium.length);
        fixed.set("n_medium", used.gain_medium.n_medium);
        fixed.set("lifetime", used.gain_medium.lifetime);
        fixed.set("quantum_yield", used.gain_medium.quantum_yield);
        fixed.set("beta", used.gain_medium.beta);
        fixed.set("molecule_density", used.gain_medium.molecule_density);
        fixed.set("emission_wavelength", used.gain_medium.emission_wavelength);
        fixed.set("cells_in_gain", static_cast<double>(used.simulation.cells_in_gain));
        output.set("fixed", fixed);

        output.set("time", column(steps, &cavdyn::StepResult::time));
        output.set("output_power", column(steps, &cavdyn::StepResult::output_power));
        output.set("pump_power", column(steps, &cavdyn::StepResult::pump_power));
        output.set("cavity_photons", column(steps, &cavdyn::StepResult::cavity_photons));
        output.set("total_inversion", column(steps, &cavdyn::StepResult::total_inversion));

        val pulses = val::array();
        for (const auto& pulse : results.pulses()) {
            val item = val::object();
            item.set("peak_time", pulse.peak_time);
            item.set("peak_power", pulse.peak_power);
            item.set("energy", pulse.energy);
            item.set("fwhm", pulse.fwhm);
            pulses.call<void>("push", item);
        }
        output.set("pulses", pulses);
        output.set("total_output_energy", results.total_output_energy());
    } catch (const std::exception& error) {
        output.set("ok", false);
        output.set("error", std::string(error.what()));
    }
    return output;
}

} // namespace

EMSCRIPTEN_BINDINGS(cavdyn)
{
    emscripten::function("simulate", &simulate);
}
