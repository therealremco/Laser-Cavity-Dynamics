#include "cavdyn/Simulator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cavdyn {
namespace {

Parameters normalized_parameters(const Parameters& params)
{
    params.validate();
    Parameters normalized = params;
    if (!normalized.pump.pulse_energy && !normalized.pump.power &&
        !normalized.pump.initial_inversion) {
        normalized.pump.initial_inversion = 1e23;
    }
    return normalized;
}

Snapshot make_snapshot(double time, StateView state)
{
    return {time, state.inversion, state.photons_right, state.photons_left};
}

double interpolated_crossing_time(const std::vector<StepResult>& steps,
                                  std::size_t left, std::size_t right,
                                  double threshold)
{
    const double y0 = steps[left].output_power;
    const double y1 = steps[right].output_power;
    if (y0 == y1) {
        return 0.5 * (steps[left].time + steps[right].time);
    }
    const double fraction = std::clamp((threshold - y0) / (y1 - y0), 0.0, 1.0);
    return steps[left].time + fraction * (steps[right].time - steps[left].time);
}

} // namespace

Simulator::Simulator(const Parameters& params)
    : params_(normalized_parameters(params)), grid_(params_),
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

void Simulator::step()
{
    const double dt = grid_.time_step();
    const double t0 = static_cast<double>(step_index_) * dt;
    const auto& gain = params_.gain_medium;

    const double deposited_density = pump_.deposition_density(t0, dt);

    for (std::size_t i = 0; i < grid_.size(); ++i) {
        const double pump_fraction = grid_.cells()[i].pump_fraction;
        if (pump_fraction > 0.0 && deposited_density > 0.0) {
            inversion_[i] += deposited_density * pump_fraction;
        }
        if (gain.molecule_density > 0.0) {
            inversion_[i] = std::min(inversion_[i], gain.molecule_density);
        }
    }

    const bool right_first = step_index_ % 2 == 0;
    for (std::size_t offset = 0; offset < grid_.size(); ++offset) {
        const std::size_t i = right_first ? offset : grid_.size() - 1 - offset;
        const Cell& cell = grid_.cells()[i];
        if (!cell.in_gain) {
            continue;
        }
        if (right_first) {
            gain_medium_.amplify(photons_right_[i], inversion_[i], cell.length);
            gain_medium_.amplify(photons_left_[i], inversion_[i], cell.length);
        } else {
            gain_medium_.amplify(photons_left_[i], inversion_[i], cell.length);
            gain_medium_.amplify(photons_right_[i], inversion_[i], cell.length);
        }
        const double seed = gain_medium_.spontaneous_seed(
            inversion_[i], dt, cell.length);
        photons_right_[i] += seed;
        photons_left_[i] += seed;
    }

    const double decay = gain_medium_.decay_factor(dt);
    for (std::size_t i = 0; i < grid_.size(); ++i) {
        const Cell& cell = grid_.cells()[i];
        const double transmission = cavity_.internal_transmission(cell);
        photons_right_[i] *= transmission;
        photons_left_[i] *= transmission;
        if (cell.in_gain) {
            inversion_[i] *= decay;
        }
    }

    const double emitted_photons = cavity_.output_photons(photons_right_.back());
    std::vector<double> next_right(grid_.size(), 0.0);
    std::vector<double> next_left(grid_.size(), 0.0);
    for (std::size_t i = 1; i < grid_.size(); ++i) {
        next_right[i] = photons_right_[i - 1];
    }
    for (std::size_t i = 0; i + 1 < grid_.size(); ++i) {
        next_left[i] = photons_left_[i + 1];
    }
    next_right.front() = cavity_.reflect_at_high_reflector(photons_left_.front());
    next_left.back() = cavity_.reflect_at_output_coupler(photons_right_.back());
    photons_right_.swap(next_right);
    photons_left_.swap(next_left);

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
        total += inversion_[i] * grid_.cells()[i].length *
                 params_.cavity.mode_area;
    }
    return total;
}

double Simulator::cavity_photons() const noexcept
{
    double total = 0.0;
    for (std::size_t i = 0; i < grid_.size(); ++i) {
        total += (photons_right_[i] + photons_left_[i]) *
                 params_.cavity.mode_area;
    }
    return total;
}

std::vector<Pulse> Results::pulses(double min_relative_peak, double valley_ratio) const
{
    std::vector<Pulse> detected;
    const std::size_t count = steps_.size();
    if (count == 0) {
        return detected;
    }
    const auto power = [this](std::size_t i) { return steps_[i].output_power; };
    double max_power = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        max_power = std::max(max_power, power(i));
    }
    if (max_power <= 0.0) {
        return detected;
    }
    const double floor = min_relative_peak * max_power;

    // Find local maxima above the floor. Two neighbouring maxima belong to the
    // same pulse unless the output between them drops below valley_ratio times
    // the smaller of the two; the larger maximum is kept as the peak.
    std::vector<std::size_t> peaks;
    double valley_since_peak = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < count; ++i) {
        const bool is_maximum = power(i) >= floor &&
            (i == 0 || power(i) > power(i - 1)) &&
            (i + 1 == count || power(i) >= power(i + 1));
        if (is_maximum) {
            if (peaks.empty()) {
                peaks.push_back(i);
                valley_since_peak = std::numeric_limits<double>::infinity();
                continue;
            }
            const std::size_t last = peaks.back();
            const double smaller = std::min(power(last), power(i));
            if (valley_since_peak < valley_ratio * smaller) {
                peaks.push_back(i);
                valley_since_peak = std::numeric_limits<double>::infinity();
                continue;
            }
            if (power(i) > power(last)) {
                peaks.back() = i;
                valley_since_peak = std::numeric_limits<double>::infinity();
                continue;
            }
        }
        if (!peaks.empty() && i != peaks.back()) {
            valley_since_peak = std::min(valley_since_peak, power(i));
        }
    }

    // Each pulse extends from the lowest point before its peak to the lowest
    // point after it, so the pulse energies add up to the total output energy.
    std::vector<std::size_t> boundaries{0};
    for (std::size_t k = 0; k + 1 < peaks.size(); ++k) {
        std::size_t lowest = peaks[k];
        for (std::size_t i = peaks[k] + 1; i < peaks[k + 1]; ++i) {
            if (power(i) < power(lowest)) {
                lowest = i;
            }
        }
        boundaries.push_back(lowest);
    }
    boundaries.push_back(count);

    for (std::size_t k = 0; k < peaks.size(); ++k) {
        const std::size_t begin = boundaries[k];
        const std::size_t end = boundaries[k + 1];
        const std::size_t peak = peaks[k];
        double energy = 0.0;
        for (std::size_t i = begin; i < end; ++i) {
            energy += power(i) * time_step_;
        }
        const double half_peak = power(peak) * 0.5;
        std::size_t left = peak;
        while (left > begin && power(left - 1) >= half_peak) {
            --left;
        }
        std::size_t right = peak;
        while (right + 1 < end && power(right + 1) >= half_peak) {
            ++right;
        }
        double left_time = steps_[left].time;
        double right_time = steps_[right].time;
        if (left > begin) {
            left_time = interpolated_crossing_time(steps_, left - 1, left, half_peak);
        }
        if (right + 1 < end) {
            right_time = interpolated_crossing_time(steps_, right, right + 1, half_peak);
        }
        detected.push_back({steps_[peak].time, power(peak), energy,
                            std::max(0.0, right_time - left_time)});
    }
    return detected;
}

double Results::total_output_energy() const noexcept
{
    double total = 0.0;
    for (const StepResult& result : steps_) {
        total += result.output_power * time_step_;
    }
    return total;
}

} // namespace cavdyn
