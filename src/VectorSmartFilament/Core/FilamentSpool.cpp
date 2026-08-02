#include "FilamentSpool.hpp"

#include <nlohmann/json.hpp>

namespace VectorSmartFilament {

namespace {

nlohmann::json dry_history_to_json(const std::vector<DryHistoryEntry>& history)
{
    auto result = nlohmann::json::array();
    for (const DryHistoryEntry& entry : history) {
        result.push_back({
            {"driedAt", entry.dried_at},
            {"durationHours", entry.duration_hours},
            {"temperatureCelsius", entry.temperature_celsius},
            {"notes", entry.notes},
        });
    }
    return result;
}

std::vector<DryHistoryEntry> dry_history_from_json(const nlohmann::json& value)
{
    std::vector<DryHistoryEntry> result;
    if (!value.is_array())
        return result;

    for (const nlohmann::json& item : value) {
        DryHistoryEntry entry;
        entry.dried_at            = item.value("driedAt", "");
        entry.duration_hours      = item.value("durationHours", 0.0);
        entry.temperature_celsius = item.value("temperatureCelsius", 0.0);
        entry.notes               = item.value("notes", "");
        result.push_back(entry);
    }
    return result;
}

} // namespace

std::string to_json(const FilamentSpool& spool)
{
    nlohmann::json value = {
        {"uuid", spool.uuid},
        {"manufacturer", spool.manufacturer},
        {"brand", spool.brand},
        {"material", spool.material},
        {"color", spool.color},
        {"colorHex", spool.color_hex},
        {"diameter", spool.diameter},
        {"density", spool.density},
        {"currentWeight", spool.current_weight},
        {"initialWeight", spool.initial_weight},
        {"purchasePrice", spool.purchase_price},
        {"purchaseDate", spool.purchase_date.empty() ? nlohmann::json(nullptr) : nlohmann::json(spool.purchase_date)},
        {"supplier", spool.supplier},
        {"lotNumber", spool.lot_number},
        {"humidity", spool.humidity},
        {"dryHistory", dry_history_to_json(spool.dry_history)},
        {"recommendedProfile", spool.recommended_profile},
        {"nozzleTemperature", spool.nozzle_temperature},
        {"bedTemperature", spool.bed_temperature},
        {"maxVolumetricSpeed", spool.max_volumetric_speed},
        {"retractionDistance", spool.retraction_distance},
        {"retractionSpeed", spool.retraction_speed},
        {"pressureAdvance", spool.pressure_advance},
        {"cooling", spool.cooling},
        {"costPerGram", spool.cost_per_gram},
        {"costPerHour", spool.cost_per_hour},
        {"image", spool.image},
        {"qrCode", spool.qr_code},
        {"currentPrinter", spool.current_printer},
        {"currentAMS", spool.current_ams},
        {"currentSlot", spool.current_slot},
        {"notes", spool.notes},
        {"createdAt", spool.created_at},
        {"updatedAt", spool.updated_at},
        {"dirty", spool.dirty},
    };
    return value.dump();
}

FilamentSpool filament_spool_from_json(const std::string& text)
{
    const nlohmann::json value = nlohmann::json::parse(text);
    FilamentSpool        spool;
    spool.uuid           = value.value("uuid", "");
    spool.manufacturer   = value.value("manufacturer", "");
    spool.brand          = value.value("brand", "");
    spool.material       = value.value("material", "");
    spool.color          = value.value("color", "");
    spool.color_hex      = value.value("colorHex", "");
    spool.diameter       = value.value("diameter", 1.75);
    spool.density        = value.value("density", 0.0);
    spool.current_weight = value.value("currentWeight", 0.0);
    spool.initial_weight = value.value("initialWeight", 0.0);
    spool.purchase_price = value.value("purchasePrice", 0.0);
    if (value.contains("purchaseDate") && !value["purchaseDate"].is_null())
        spool.purchase_date = value.value("purchaseDate", "");
    spool.supplier             = value.value("supplier", "");
    spool.lot_number           = value.value("lotNumber", "");
    spool.humidity             = value.value("humidity", 0.0);
    spool.dry_history          = dry_history_from_json(value.value("dryHistory", nlohmann::json::array()));
    spool.recommended_profile  = value.value("recommendedProfile", "");
    spool.nozzle_temperature   = value.value("nozzleTemperature", 0);
    spool.bed_temperature      = value.value("bedTemperature", 0);
    spool.max_volumetric_speed = value.value("maxVolumetricSpeed", 0.0);
    spool.retraction_distance  = value.value("retractionDistance", 0.0);
    spool.retraction_speed     = value.value("retractionSpeed", 0.0);
    spool.pressure_advance     = value.value("pressureAdvance", 0.0);
    spool.cooling              = value.value("cooling", 0);
    spool.cost_per_gram        = value.value("costPerGram", 0.0);
    spool.cost_per_hour        = value.value("costPerHour", 0.0);
    spool.image                = value.value("image", "");
    spool.qr_code              = value.value("qrCode", "");
    spool.current_printer      = value.value("currentPrinter", "");
    spool.current_ams          = value.value("currentAMS", value.value("currentAms", ""));
    spool.current_slot         = value.value("currentSlot", "");
    spool.notes                = value.value("notes", "");
    spool.created_at           = value.value("createdAt", "");
    spool.updated_at           = value.value("updatedAt", "");
    spool.dirty                = value.value("dirty", false);
    return spool;
}

} // namespace VectorSmartFilament
