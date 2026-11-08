#pragma once

#include "cavdyn/Parameters.h"

#include <iosfwd>
#include <string>

namespace cavdyn {

Parameters read_config(std::istream& input, const std::string& source = "<stream>");
Parameters read_config_file(const std::string& path);
void write_default_config(std::ostream& output);

} // namespace cavdyn
