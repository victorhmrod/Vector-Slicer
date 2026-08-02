#pragma once

#include <string>
#include <vector>

namespace VectorSmartFilament {

struct DryHistoryEntry
{
    std::string dried_at;
    double      duration_hours      = 0.0;
    double      temperature_celsius = 0.0;
    std::string notes;
};

struct FilamentSpool
{
    std::string                  uuid;
    std::string                  manufacturer;
    std::string                  brand;
    std::string                  material;
    std::string                  color;
    std::string                  color_hex;
    double                       diameter       = 1.75;
    double                       density        = 0.0;
    double                       current_weight = 0.0;
    double                       initial_weight = 0.0;
    double                       purchase_price = 0.0;
    std::string                  purchase_date;
    std::string                  supplier;
    std::string                  lot_number;
    double                       humidity = 0.0;
    std::vector<DryHistoryEntry> dry_history;
    std::string                  recommended_profile;
    int                          nozzle_temperature   = 0;
    int                          bed_temperature      = 0;
    double                       max_volumetric_speed = 0.0;
    double                       retraction_distance  = 0.0;
    double                       retraction_speed     = 0.0;
    double                       pressure_advance     = 0.0;
    int                          cooling              = 0;
    double                       cost_per_gram        = 0.0;
    double                       cost_per_hour        = 0.0;
    std::string                  image;
    std::string                  qr_code;
    std::string                  current_printer;
    std::string                  current_ams;
    std::string                  current_slot;
    std::string                  notes;
    std::string                  created_at;
    std::string                  updated_at;
    bool                         dirty = false;
};

std::string   to_json(const FilamentSpool& spool);
FilamentSpool filament_spool_from_json(const std::string& value);

} // namespace VectorSmartFilament
