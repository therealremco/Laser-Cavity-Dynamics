#pragma once

#include "cavdyn/Parameters.h"
#include "cavdyn/Results.h"
#include "cavdyn/Grid.h"

#include <string>

namespace cavdyn {

void write_results(const std::string& path, const Parameters& params,
                   const Grid& grid, const Results& results);

} // namespace cavdyn
