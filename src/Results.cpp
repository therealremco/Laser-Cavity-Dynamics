#include "cavdyn/Results.h"

#include <algorithm>
#include <limits>

namespace cavdyn {
namespace {

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
