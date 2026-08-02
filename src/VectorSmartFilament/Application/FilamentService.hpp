#pragma once

#include <memory>
#include <string>

#include "EventBus.hpp"
#include "Interfaces.hpp"

namespace VectorSmartFilament {

class FilamentService final : public IFilamentService
{
public:
    FilamentService(std::shared_ptr<IFilamentCache>     cache,
                    std::shared_ptr<IFilamentErpClient> erp_client,
                    std::shared_ptr<FilamentEventBus>   events,
                    std::string                         tag_secret = {});

    Result<FilamentSpool> register_spool(FilamentSpool spool, ITagWriter& tag_writer) override;
    Result<FilamentSpool> read_spool(ITagReader& tag_reader) override;
    Result<FilamentSpool> update_consumed_weight(const std::string& uuid, double consumed_grams) override;

private:
    std::shared_ptr<IFilamentCache>     m_cache;
    std::shared_ptr<IFilamentErpClient> m_erp_client;
    std::shared_ptr<FilamentEventBus>   m_events;
    std::string                         m_tag_secret;
};

} // namespace VectorSmartFilament
