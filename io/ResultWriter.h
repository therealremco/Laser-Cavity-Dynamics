#pragma once

#include "cavdyn/Parameters.h"
#include "cavdyn/Results.h"
#include "cavdyn/Grid.h"

#include <string>

namespace cavdyn {

/// Writes the time series to path and, when snapshots were recorded, the
/// spatial profiles to <path without extension>.snapshots.dat. Pass the
/// simulator's parameters() so the header shows the values actually used.
void write_results(const std::string& path, const Parameters& params,
                   const Grid& grid, const Results& results);

} // namespace cavdyn
