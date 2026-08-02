#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../Core/FilamentSpool.hpp"
#include "../Core/FilamentTag.hpp"
#include "../Core/Result.hpp"

namespace VectorSmartFilament {

struct FilamentPresetUpdate
{
    std::string filament_preset;
    std::string nozzle_temperature;
    std::string bed_temperature;
    std::string cooling;
    std::string pressure_advance;
    std::string retraction_distance;
    std::string retraction_speed;
    std::string max_volumetric_speed;
    std::string preview_color;
    std::string material;
    std::string spool_name;
};

class IFilamentRepository
{
public:
    virtual ~IFilamentRepository()                                                   = default;
    virtual Result<void>                       save(const FilamentSpool& spool)      = 0;
    virtual Result<FilamentSpool>              find_by_uuid(const std::string& uuid) = 0;
    virtual Result<std::vector<FilamentSpool>> list()                                = 0;
};

class IFilamentCache
{
public:
    virtual ~IFilamentCache()                                                  = default;
    virtual Result<void>                       put(const FilamentSpool& spool) = 0;
    virtual Result<FilamentSpool>              get(const std::string& uuid)    = 0;
    virtual Result<std::vector<FilamentSpool>> all()                           = 0;
};

class ITagWriter
{
public:
    virtual ~ITagWriter()                              = default;
    virtual Result<void> write(const FilamentTag& tag) = 0;
};

class ITagReader
{
public:
    virtual ~ITagReader()              = default;
    virtual Result<FilamentTag> read() = 0;
};

class IFilamentTagProvider : public ITagWriter, public ITagReader
{
public:
    ~IFilamentTagProvider() override = default;
    virtual std::string name() const = 0;
};

class IFilamentErpClient
{
public:
    virtual ~IFilamentErpClient()                                       = default;
    virtual Result<void>          save(const FilamentSpool& spool)      = 0;
    virtual Result<FilamentSpool> find_by_uuid(const std::string& uuid) = 0;
    virtual bool                  online() const                        = 0;
};

class IFilamentSyncService
{
public:
    virtual ~IFilamentSyncService() = default;
    virtual Result<void> sync()     = 0;
};

class IFilamentProfileService
{
public:
    virtual ~IFilamentProfileService()                                                 = default;
    virtual FilamentPresetUpdate build_preset_update(const FilamentSpool& spool) const = 0;
};

class IFilamentIdentificationProvider
{
public:
    virtual ~IFilamentIdentificationProvider() = default;
    virtual Result<FilamentTag> identify()     = 0;
};

class IFilamentService
{
public:
    virtual ~IFilamentService()                                                                          = default;
    virtual Result<FilamentSpool> register_spool(FilamentSpool spool, ITagWriter& tag_writer)            = 0;
    virtual Result<FilamentSpool> read_spool(ITagReader& tag_reader)                                     = 0;
    virtual Result<FilamentSpool> update_consumed_weight(const std::string& uuid, double consumed_grams) = 0;
};

} // namespace VectorSmartFilament
