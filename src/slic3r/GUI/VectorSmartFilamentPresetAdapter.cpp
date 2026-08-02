#include "VectorSmartFilamentPresetAdapter.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI {

namespace {

std::vector<std::string> strings_with_value(const DynamicPrintConfig& config, const std::string& key, size_t index, const std::string& value)
{
    std::vector<std::string> values;
    if (const ConfigOptionStrings* existing = config.option<ConfigOptionStrings>(key))
        values = existing->values;

    values.resize(std::max(values.size(), index + 1));
    values[index] = value;
    return values;
}

std::vector<int> ints_with_value(const DynamicPrintConfig& config, const std::string& key, size_t index, int value)
{
    std::vector<int> values;
    if (const ConfigOptionInts* existing = config.option<ConfigOptionInts>(key))
        values = existing->values;

    values.resize(std::max(values.size(), index + 1));
    values[index] = value;
    return values;
}

std::vector<double> floats_with_value(const DynamicPrintConfig& config, const std::string& key, size_t index, double value)
{
    std::vector<double> values;
    if (const ConfigOptionFloats* existing = config.option<ConfigOptionFloats>(key))
        values = existing->values;

    values.resize(std::max(values.size(), index + 1));
    values[index] = value;
    return values;
}

std::vector<unsigned char> bools_with_value(const DynamicPrintConfig& config, const std::string& key, size_t index, bool value)
{
    std::vector<unsigned char> values;
    if (const ConfigOptionBools* existing = config.option<ConfigOptionBools>(key))
        values = existing->values;

    values.resize(std::max(values.size(), index + 1));
    values[index] = static_cast<unsigned char>(value);
    return values;
}

} // namespace

bool VectorSmartFilamentPresetAdapter::apply(const VectorSmartFilament::FilamentPresetUpdate& update,
                                             DynamicPrintConfig&                              config,
                                             size_t                                           filament_index) const
{
    if (update.spool_name.empty())
        return false;

    config.set_key_value("filament_settings_id",
                         new ConfigOptionStrings(strings_with_value(config, "filament_settings_id", filament_index, update.spool_name)));
    config.set_key_value("filament_colour",
                         new ConfigOptionStrings(strings_with_value(config, "filament_colour", filament_index, update.preview_color)));
    config.set_key_value("default_filament_colour", new ConfigOptionStrings(strings_with_value(config, "default_filament_colour",
                                                                                               filament_index, update.preview_color)));
    config.set_key_value("filament_type",
                         new ConfigOptionStrings(strings_with_value(config, "filament_type", filament_index, update.material)));
    config.set_key_value("nozzle_temperature", new ConfigOptionInts(ints_with_value(config, "nozzle_temperature", filament_index,
                                                                                    std::atoi(update.nozzle_temperature.c_str()))));
    config.set_key_value("nozzle_temperature_initial_layer",
                         new ConfigOptionInts(ints_with_value(config, "nozzle_temperature_initial_layer", filament_index,
                                                              std::atoi(update.nozzle_temperature.c_str()))));
    config.set_key_value("bed_temperature", new ConfigOptionInts(ints_with_value(config, "bed_temperature", filament_index,
                                                                                 std::atoi(update.bed_temperature.c_str()))));
    config.set_key_value("bed_temperature_initial_layer",
                         new ConfigOptionInts(ints_with_value(config, "bed_temperature_initial_layer", filament_index,
                                                              std::atoi(update.bed_temperature.c_str()))));
    config.set_key_value("fan_always_on", new ConfigOptionBools(bools_with_value(config, "fan_always_on", filament_index,
                                                                                 std::atoi(update.cooling.c_str()) > 0)));
    config.set_key_value("pressure_advance", new ConfigOptionFloats(floats_with_value(config, "pressure_advance", filament_index,
                                                                                      std::atof(update.pressure_advance.c_str()))));
    config.set_key_value("retraction_length", new ConfigOptionFloats(floats_with_value(config, "retraction_length", filament_index,
                                                                                       std::atof(update.retraction_distance.c_str()))));
    config.set_key_value("retraction_speed", new ConfigOptionFloats(floats_with_value(config, "retraction_speed", filament_index,
                                                                                      std::atof(update.retraction_speed.c_str()))));
    config.set_key_value("filament_max_volumetric_speed",
                         new ConfigOptionFloats(floats_with_value(config, "filament_max_volumetric_speed", filament_index,
                                                                  std::atof(update.max_volumetric_speed.c_str()))));

    return true;
}

}} // namespace Slic3r::GUI
