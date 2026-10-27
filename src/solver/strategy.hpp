// Regret matching and the normalisation the solver applies to its tables.

#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

namespace cfr::solver {

namespace detail {

// Size of the per-node stack buffers; the solver asserts against it.
inline constexpr std::size_t kMaxActionsPerInfoset = 64;

}

// Totals at or below this count as empty and become uniform.
inline constexpr double kStrategySumEpsilon = 1e-12;

// Scales values to sum to 1 in place, or sets them uniform when
// the total is not above epsilon.
inline void normalize_or_uniform(std::span<double> values,
                                 double epsilon = kStrategySumEpsilon) {
    double total = 0.0;
    for (double value : values) total += value;

    if (total > epsilon) {
        for (double& value : values) value /= total;
        return;
    }
    double uniform_probability = 1.0 / static_cast<double>(values.size());
    for (double& value : values) value = uniform_probability;
}

// Negatives clamp to zero, then normalize. Uniform when nothing is positive.
inline void regret_matching_in_place(std::span<double> strategy) {
    for (double& value : strategy) value = std::max(value, 0.0);
    normalize_or_uniform(strategy, 0.0);
}

}
