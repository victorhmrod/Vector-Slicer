#include "FilamentProfileService.hpp"

#include <sstream>

namespace VectorSmartFilament {

namespace {

std::string to_string(double value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

} // namespace

FilamentPresetUpdate FilamentProfileService::build_preset_update(const FilamentSpool& spool) const
{
    return {
        spool.recommended_profile,
        std::to_string(spool.nozzle_temperature),
        std::to_string(spool.bed_temperature),
        std::to_string(spool.cooling),
        to_string(spool.pressure_advance),
        to_string(spool.retraction_distance),
        to_string(spool.retraction_speed),
        to_string(spool.max_volumetric_speed),
        spool.color_hex,
        spool.material,
        spool.brand.empty() ? spool.uuid : spool.brand + " " + spool.material + " " + spool.color,
    };
}

} // namespace VectorSmartFilament
