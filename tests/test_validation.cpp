// The validation cases from CONTEXT.md. Each runs as its own CTest test:
//   cavdyn-validation <name>

#include "TestSupport.h"

#include "cavdyn/Constants.h"
#include "cavdyn/Simulator.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace cavdyn_test;

namespace {

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.14159265358979323846;

/// A cavity completely filled with uniformly pumped gain: the lumped limit.
cavdyn::Parameters filled_cavity(double length, double r_oc, std::size_t cells)
{
    cavdyn::Parameters params;
    params.cavity.length = length;
    params.cavity.r_hr = 1.0;
    params.cavity.r_oc = r_oc;
    params.gain_medium.start = 0.0;
    params.gain_medium.length = length;
    params.pump.start = 0.0;
    params.pump.length = length;
    params.simulation.cells_in_gain = cells;
    return params;
}

/// Siegman's lumped constants for a filled cavity, with N and n as totals.
struct Lumped {
    double K;            // s⁻¹ per inversion
    double gamma_c;      // s⁻¹
    double gamma_2;      // s⁻¹
    double threshold;    // inversion density at threshold, m⁻³
    double volume;       // m³
};

Lumped lumped_constants(const cavdyn::Parameters& params)
{
    const auto& gain = params.gain_medium;
    const double c = cavdyn::speed_of_light;
    const double length = params.cavity.length;
    Lumped lumped{};
    lumped.volume = length * params.cavity.mode_area;
    lumped.K = gain.cross_section * c / (gain.n_medium * lumped.volume);
    lumped.gamma_c = c / (2.0 * gain.n_medium * length) *
                         std::log(1.0 / (params.cavity.r_hr * params.cavity.r_oc)) +
                     gain.internal_loss * c / gain.n_medium;
    lumped.gamma_2 = 1.0 / gain.lifetime;
    lumped.threshold = lumped.gamma_c / (lumped.K * lumped.volume);
    return lumped;
}

/// Integrates Siegman's equations with RK4, with the spontaneous term scaled
/// by β·Φ as in the distributed model. Returns n(t) and N(t) at every step.
struct LumpedSeries {
    std::vector<double> photons;
    std::vector<double> inversion;
};

LumpedSeries integrate_lumped(const Lumped& lumped, double seed_fraction,
                              double initial_inversion, double pump_rate,
                              double time_step, std::size_t steps)
{
    const auto derivative = [&](double n, double N, double& dn, double& dN) {
        dn = lumped.K * N * n + seed_fraction * lumped.gamma_2 * N - lumped.gamma_c * n;
        dN = pump_rate - lumped.gamma_2 * N - lumped.K * N * n;
    };
    LumpedSeries series;
    series.photons.reserve(steps);
    series.inversion.reserve(steps);
    double n = 0.0;
    double N = initial_inversion;
    const double h = time_step;
    for (std::size_t i = 0; i < steps; ++i) {
        double k1n, k1N, k2n, k2N, k3n, k3N, k4n, k4N;
        derivative(n, N, k1n, k1N);
        derivative(n + 0.5 * h * k1n, N + 0.5 * h * k1N, k2n, k2N);
        derivative(n + 0.5 * h * k2n, N + 0.5 * h * k2N, k3n, k3N);
        derivative(n + h * k3n, N + h * k3N, k4n, k4N);
        n += h / 6.0 * (k1n + 2.0 * k2n + 2.0 * k3n + k4n);
        N += h / 6.0 * (k1N + 2.0 * k2N + 2.0 * k3N + k4N);
        series.photons.push_back(n);
        series.inversion.push_back(N);
    }
    return series;
}

std::size_t steps_per_round_trip(const cavdyn::Simulator& simulator)
{
    return 2 * simulator.grid().size();
}

// 1. Lumped limit ------------------------------------------------------------

void lumped_limit_threshold()
{
    // Without seed or decay, the photon number grows by exactly exp((K·N − γ_c)·T)
    // per round trip, so it falls below N_th = γ_c/K and grows above it.
    for (const double ratio : {0.9, 1.1}) {
        auto params = filled_cavity(1e-2, 0.99, 40);
        params.gain_medium.internal_loss = 0.5;
        params.gain_medium.beta = 0.0;
        params.gain_medium.lifetime = infinity;
        const Lumped lumped = lumped_constants(params);
        params.pump.initial_inversion = ratio * lumped.threshold;
        params.simulation.round_trips = 40.0;
        cavdyn::Simulator simulator(params);
        for (std::size_t i = 0; i < simulator.grid().size(); ++i) {
            simulator.set_photons(i, 1.0, 1.0);
        }
        simulator.run();
        const auto& steps = simulator.results().steps();
        const std::size_t period = steps_per_round_trip(simulator);
        const auto& early = steps[20 * period - 1];
        const auto& late = steps[40 * period - 1];
        const double rate = std::log(late.cavity_photons / early.cavity_photons) /
                            (late.time - early.time);
        const double expected = lumped.K * early.total_inversion - lumped.gamma_c;
        require(std::abs(rate - expected) < 1e-6 * lumped.gamma_c,
                "net gain at N = " + std::to_string(ratio) + "·N_th: got " +
                std::to_string(rate) + " s⁻¹, expected " + std::to_string(expected));
        require(ratio < 1.0 ? rate < 0.0 : rate > 0.0,
                "threshold is not at N_th = γ_c/K");
    }
}

void lumped_limit_dynamics()
{
    // A step inversion of three times threshold builds up from spontaneous
    // emission into one large pulse. The photon number and inversion must
    // follow the ODE solution of Siegman's equations.
    auto params = filled_cavity(1e-2, 0.99, 40);
    params.gain_medium.internal_loss = 0.5;
    params.gain_medium.lifetime = 1e-6;
    const Lumped lumped = lumped_constants(params);
    params.pump.initial_inversion = 3.0 * lumped.threshold;
    params.simulation.duration = 1.5e-7;
    cavdyn::Simulator simulator(params);
    simulator.run();
    const auto& steps = simulator.results().steps();

    const auto ode = integrate_lumped(
        lumped, params.gain_medium.beta * params.gain_medium.quantum_yield,
        *params.pump.initial_inversion * lumped.volume, 0.0, simulator.grid().time_step(),
        steps.size());

    const auto model_peak = std::max_element(
        steps.begin(), steps.end(), [](const auto& a, const auto& b) {
            return a.cavity_photons < b.cavity_photons;
        });
    const auto ode_peak = std::max_element(ode.photons.begin(), ode.photons.end());
    const std::size_t model_index = static_cast<std::size_t>(model_peak - steps.begin());
    const std::size_t ode_index = static_cast<std::size_t>(ode_peak - ode.photons.begin());
    require(model_index > 0 && model_index + 1 < steps.size(),
            "the pulse peak is not inside the simulated interval");

    require_close(model_peak->cavity_photons, *ode_peak, 0.005, "peak photon number");
    require_close(steps[model_index].time, steps[ode_index].time, 0.002, "peak time");
    require_close(steps.back().total_inversion, ode.inversion.back(), 0.005,
                  "inversion after the pulse");
    // Compare the whole curve where the photon number is significant.
    for (std::size_t i = 0; i < steps.size(); ++i) {
        if (ode.photons[i] > 0.1 * *ode_peak) {
            require(std::abs(steps[i].cavity_photons - ode.photons[i]) < 0.01 * *ode_peak,
                    "photon number deviates from the ODE at t = " +
                    std::to_string(steps[i].time));
        }
    }
}

// 2. Relaxation oscillations ---------------------------------------------------

void relaxation_oscillations()
{
    auto params = filled_cavity(5e-2, 0.95, 10);
    params.gain_medium.lifetime = 1e-6;
    const Lumped lumped = lumped_constants(params);
    const double pump_ratio = 3.0;
    const double pump_photon_energy =
        cavdyn::planck_constant * cavdyn::speed_of_light / params.pump.wavelength;
    // Pump rate density R_p = r·γ_2·N_th, converted to a pump power.
    params.pump.pulse_duration = infinity;
    params.pump.power = pump_ratio * lumped.gamma_2 * lumped.threshold *
                        pump_photon_energy * params.pump.length *
                        params.cavity.mode_area /
                        (params.pump.efficiency * params.pump.absorbed_fraction);
    const double omega =
        std::sqrt((pump_ratio - 1.0) * lumped.gamma_c * lumped.gamma_2);
    const double period = 2.0 * pi / omega;
    params.simulation.duration = 8.0 * period;
    cavdyn::Simulator simulator(params);
    simulator.run();

    // Average over each round trip to remove the structure within it.
    const auto& steps = simulator.results().steps();
    const std::size_t block = steps_per_round_trip(simulator);
    std::vector<double> average;
    std::vector<double> time;
    for (std::size_t start = 0; start + block <= steps.size(); start += block) {
        double sum = 0.0;
        for (std::size_t i = start; i < start + block; ++i) {
            sum += steps[i].cavity_photons;
        }
        average.push_back(sum / static_cast<double>(block));
        time.push_back(steps[start + block / 2].time);
    }

    // Maxima that are the largest value within a quarter period either side.
    const auto window = static_cast<std::size_t>(
        0.25 * period / (static_cast<double>(block) * simulator.grid().time_step()));
    std::vector<double> peaks;
    for (std::size_t i = window; i + window < average.size(); ++i) {
        bool is_peak = true;
        for (std::size_t j = i - window; j <= i + window && is_peak; ++j) {
            is_peak = j == i || average[j] < average[i];
        }
        if (is_peak) {
            peaks.push_back(time[i]);
        }
    }
    require(peaks.size() >= 6, "found only " + std::to_string(peaks.size()) +
                               " oscillation maxima");
    // The first spikes are nonlinear; the frequency approaches ω for the later ones.
    const double measured_period =
        (peaks.back() - peaks[2]) / static_cast<double>(peaks.size() - 3);
    require_close(2.0 * pi / measured_period, omega, 0.02,
                  "relaxation oscillation frequency");
}

// 3. Conservation --------------------------------------------------------------

void conservation()
{
    // Lossless mirrors, no internal loss, no decay: photons plus inversion are
    // constant while a seeded packet saturates the inhomogeneous gain.
    cavdyn::Parameters params;
    params.cavity.r_hr = 1.0;
    params.cavity.r_oc = 1.0;
    params.gain_medium.internal_loss = 0.0;
    params.gain_medium.lifetime = infinity;
    params.pump.length = 4.4e-3;
    params.pump.start = 4e-3;
    params.simulation.round_trips = 25.0;
    cavdyn::Simulator simulator(params);
    simulator.step();  // deposits the inversion
    const double initial = simulator.results().steps().back().total_inversion;
    simulator.set_photons(simulator.grid().left_background_cells() + 20, 1e10, 0.0);
    const double total = initial + 1e10 * params.cavity.mode_area;
    simulator.run();
    for (const auto& step : simulator.results().steps()) {
        if (step.time > simulator.grid().time_step()) {
            require_close(step.cavity_photons + step.total_inversion, total, 1e-12,
                          "photons plus inversion at t = " + std::to_string(step.time));
        }
    }
    require(simulator.results().steps().back().total_inversion < 0.5 * initial,
            "the gain was not depleted, so the test exercised nothing");
}

// 4. Propagation ---------------------------------------------------------------

void propagation()
{
    cavdyn::Parameters params;
    params.cavity.r_hr = 0.9;
    params.cavity.r_oc = 0.8;
    params.pump.initial_inversion = 0.0;
    params.gain_medium.beta = 0.0;
    cavdyn::Simulator simulator(params);
    const std::size_t cells = simulator.grid().size();
    for (const std::size_t start : {std::size_t{0}, std::size_t{5}, cells / 2, cells - 1}) {
        cavdyn::Simulator run(params);
        run.set_photons(start, 1.0, 0.0);
        for (std::size_t step = 1; step <= 2 * cells; ++step) {
            run.step();
            const double back = run.state().photons_right[start];
            if (step < 2 * cells) {
                require(back == 0.0, "packet returned early, after " +
                                     std::to_string(step) + " steps");
            }
        }
        const auto state = run.state();
        require_close(state.photons_right[start], 0.9 * 0.8, 1e-14,
                      "packet after one round trip from cell " + std::to_string(start));
        for (std::size_t i = 0; i < cells; ++i) {
            require(state.photons_left[i] == 0.0 &&
                        (i == start || state.photons_right[i] == 0.0),
                    "packet spread to other cells");
        }
    }
    double optical_length = 0.0;
    for (const auto& cell : simulator.grid().cells()) {
        optical_length += cell.refractive_index * cell.length;
    }
    require_close(simulator.grid().round_trip_time(),
                  2.0 * optical_length / cavdyn::speed_of_light, 1e-12,
                  "round-trip time");
}

// 5. Convergence ---------------------------------------------------------------

std::vector<cavdyn::Pulse> pulses_with_cells(std::size_t cells, double& time_step)
{
    // Background lengths are whole multiples of the cell length at 60 and at
    // 120 cells, so both grids have exactly the same cavity length.
    cavdyn::Parameters params;
    params.cavity.length = 13.5e-3;
    params.gain_medium.start = 2.25e-3;
    params.pump.start = 2.25e-3;
    params.simulation.cells_in_gain = cells;
    params.simulation.round_trips = 25.0;
    cavdyn::Simulator simulator(params);
    simulator.run();
    time_step = simulator.grid().time_step();
    return simulator.results().pulses();
}

void convergence()
{
    // The scheme converges at first order. With these parameters the largest
    // change from 60 to 120 cells is 0.95 % (the first pulse, which builds up
    // from the spontaneous seed); pulses in the middle of the train change by
    // about 0.004 %.
    double coarse_step = 0.0;
    double fine_step = 0.0;
    const auto coarse = pulses_with_cells(60, coarse_step);
    const auto fine = pulses_with_cells(120, fine_step);
    require(coarse.size() == fine.size(),
            "pulse count changed from " + std::to_string(coarse.size()) + " to " +
            std::to_string(fine.size()));
    require(coarse.size() >= 20, "too few pulses to compare");
    for (std::size_t i = 0; i < coarse.size(); ++i) {
        const std::string which = "pulse " + std::to_string(i + 1);
        require(std::abs(coarse[i].peak_time - fine[i].peak_time) < coarse_step,
                which + " peak moved by " +
                std::to_string((fine[i].peak_time - coarse[i].peak_time) / coarse_step) +
                " steps");
        require_close(fine[i].energy, coarse[i].energy, 0.01, which + " energy");
    }
}

// 6. Legacy comparison ---------------------------------------------------------

void legacy_comparison()
{
    // The 1999 program: 1.5 cm cavity at index 1.5 throughout, 100 slices of
    // 150 µm, inversion 1e23 m⁻³ in slices 20–79, K = 2e-12 (σ = 1e-20 m²).
    cavdyn::Parameters params;
    params.cavity.n_background = 1.5;
    params.pump.initial_inversion = 1e23;
    params.simulation.cells_in_gain = 60;
    params.simulation.round_trips = 25.0;
    cavdyn::Simulator simulator(params);
    const auto& grid = simulator.grid();
    require(grid.size() == 100 && grid.left_background_cells() == 20 &&
                grid.right_background_cells() == 20,
            "grid does not reproduce the legacy 100 slices");
    require_close(grid.round_trip_time(),
                  2.0 * 1.5 * params.cavity.length / cavdyn::speed_of_light, 1e-12,
                  "legacy round-trip time");
    simulator.run();
    const auto pulses = simulator.results().pulses();
    require(pulses.size() >= 40, "found only " + std::to_string(pulses.size()) + " pulses");
    // The left- and right-moving trains are symmetric, so a pulse leaves the
    // output coupler every half round trip: every 100 steps.
    const double dt = grid.time_step();
    for (std::size_t i = 1; i < pulses.size(); ++i) {
        const double spacing = (pulses[i].peak_time - pulses[i - 1].peak_time) / dt;
        require(std::abs(spacing - 100.0) <= 1.0 + 1e-9,
                "pulse " + std::to_string(i + 1) + " follows after " +
                std::to_string(spacing) + " steps instead of 100");
    }
}

} // namespace

int main(int argc, char** argv)
{
    return run_tests(argc, argv, {
        {"lumped_limit_threshold", lumped_limit_threshold},
        {"lumped_limit_dynamics", lumped_limit_dynamics},
        {"relaxation_oscillations", relaxation_oscillations},
        {"conservation", conservation},
        {"propagation", propagation},
        {"convergence", convergence},
        {"legacy_comparison", legacy_comparison},
    });
}
