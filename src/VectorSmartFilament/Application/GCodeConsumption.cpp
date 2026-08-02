#include "GCodeConsumption.hpp"

#include <regex>

namespace VectorSmartFilament {

Result<double> consumed_filament_grams_from_gcode(const std::string& gcode)
{
    const std::regex pattern(R"(filament\s+used\s*\[g\]\s*=\s*([0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    std::smatch      match;
    if (!std::regex_search(gcode, match, pattern))
        return {0.0, "Peso consumido nao encontrado no G-code"};

    return {std::stod(match[1].str()), ""};
}

} // namespace VectorSmartFilament
