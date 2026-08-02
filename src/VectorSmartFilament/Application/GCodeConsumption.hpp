#pragma once

#include <string>

#include "../Core/Result.hpp"

namespace VectorSmartFilament {

Result<double> consumed_filament_grams_from_gcode(const std::string& gcode);

} // namespace VectorSmartFilament
