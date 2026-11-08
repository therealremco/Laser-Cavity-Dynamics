#include "cavdyn/Simulator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace cavdyn {
namespace {

Snapshot make_snapshot(double time, StateView state)
{
    return {time, state.inversion, state.photons_right, state.photons_left};
}

} // namespace

Simulator::Simulator(const Parameters& params)
    : params_(params.resolved()), grid_(params_),
      cavity_(params_.cavity), gain_medium_(params_.gain_medium),
      pump_(params_.pump, params_.cavity.mode_area),
      inversion_(grid_.size(), 0.0), photons_right_(grid_.size(), 0.0),
      photons_left_(grid_.size(), 0.0)
{
    const double duration = params_.simulation.duration.value_or(
        params_.simulation.round_trips * grid_.round_trip_time());
    const double step_count = std::ceil(duration / grid_.time_step());
    if (!std::isfinite(step_count) ||
        step_count > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw ParameterError("simulation duration exceeds the supported step count");
    }
    requested_steps_ = static_cast<std::size_t>(step_count);
    results_.time_step_ = grid_.time_step();
}

void Simulator::set_photons(std::size_t cell, double right_moving, double left_moving)
{
    if (cell >= grid_.size()) {
        throw std::out_of_range("cell " + std::to_string(cell) +
                                " is outside the grid of " +
                                std::to_string(grid_.size()) + " cells");
    }
    if (!(right_moving >= 0.0) || !(left_moving >= 0.0) ||
        !std::isfinite(right_moving) || !std::isfinite(left_moving)) {
        throw std::invalid_argument("photon densities must be finite and non-negative");
    }
    photons_right_[cell] = right_moving;
    photons_left_[cell] = left_moving;
}

void Simulator::step()
{
    const double dt = grid_.time_step();
    const double t0 = static_cast<double>(step_index_) * dt;
    const auto& gain = params_.gain_medium;
    const std::size_t count = grid_.size();

    // 1. Pump.
    const double deposited_density = pump_.deposition_density(t0, dt);
    for (std::size_t i = 0; i < count; ++i) {
        const double pump_fraction = grid_.cells()[i].pump_fraction;
        if (pump_fraction > 0.0 && deposited_density > 0.0) {
            inversion_[i] += deposited_density * pump_fraction;
        }
        if (gain.molecule_density > 0.0) {
            inversion_[i] = std::min(inversion_[i], gain.molecule_density);
        }
    }

    // 2–3. Gain with depletion, alternating which direction goes first, then the seed.
    const bool right_first = step_index_ % 2 == 0;
    for (std::size_t i = 0; i < count; ++i) {
        const Cell& cell = grid_.cells()[i];
        if (!cell.in_gain) {
            continue;
        }
        double& first = right_first ? photons_right_[i] : photons_left_[i];
        double& second = right_first ? photons_left_[i] : photons_right_[i];
        gain_medium_.amplify(first, inversion_[i], cell.length);
        gain_medium_.amplify(second, inversion_[i], cell.length);
        const double seed = gain_medium_.spontaneous_seed(inversion_[i], dt, cell.length);
        photons_right_[i] += seed;
        photons_left_[i] += seed;
    }

    // 4–5. Internal loss and inversion decay.
    const double decay = gain_medium_.decay_factor(dt);
    for (std::size_t i = 0; i < count; ++i) {
        const Cell& cell = grid_.cells()[i];
        const double transmission = cavity_.internal_transmission(cell);
        photons_right_[i] *= transmission;
        photons_left_[i] *= transmission;
        if (cell.in_gain) {
            inversion_[i] *= decay;
        }
    }

    // 6–7. Propagation by one cell, with reflection at both mirrors.
    const double at_output_coupler = photons_right_.back();
    const double at_high_reflector = photons_left_.front();
    const double emitted_photons = cavity_.output_photons(at_output_coupler);
    std::copy_backward(photons_right_.begin(), photons_right_.end() - 1,
                       photons_right_.end());
    std::copy(photons_left_.begin() + 1, photons_left_.end(), photons_left_.begin());
    photons_right_.front() = cavity_.reflect_at_high_reflector(at_high_reflector);
    photons_left_.back() = cavity_.reflect_at_output_coupler(at_output_coupler);

    ++step_index_;
    StepResult result{
        time(),
        emitted_photons,
        emitted_photons * gain_medium_.photon_energy() / dt,
        cavity_photons(),
        total_inversion()
    };
    results_.steps_.push_back(result);
    if (params_.simulation.snapshot_interval > 0 &&
        step_index_ % params_.simulation.snapshot_interval == 0) {
        results_.snapshots_.push_back(make_snapshot(time(), state()));
    }
}

void Simulator::run(Observer* observer)
{
    while (step_index_ < requested_steps_) {
        step();
        if (observer && !observer->on_step(results_.steps_.back(), state())) {
            break;
        }
    }
}

double Simulator::time() const noexcept
{
    return static_cast<double>(step_index_) * grid_.time_step();
}

StateView Simulator::state() const noexcept
{
    return {inversion_, photons_right_, photons_left_};
}

double Simulator::total_inversion() const noexcept
{
    double total = 0.0;
    for (std::size_t i = 0; i < grid_.size(); ++i) {
        total += inversion_[i] * grid_.cells()[i].length;
    }
    return total * params_.cavity.mode_area;
}

double Simulator::cavity_photons() const noexcept
{
    double total = 0.0;
    for (std::size_t i = 0; i < grid_.size(); ++i) {
        total += photons_right_[i] + photons_left_[i];
    }
    return total * params_.cavity.mode_area;
}

} // namespace cavdyn
