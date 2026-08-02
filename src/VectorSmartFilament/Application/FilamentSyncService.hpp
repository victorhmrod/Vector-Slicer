#pragma once

#include <memory>

#include "Interfaces.hpp"

namespace VectorSmartFilament {

class FilamentSyncService final : public IFilamentSyncService
{
public:
    FilamentSyncService(std::shared_ptr<IFilamentCache> cache, std::shared_ptr<IFilamentErpClient> erp_client);

    Result<void> sync() override;

private:
    std::shared_ptr<IFilamentCache>     m_cache;
    std::shared_ptr<IFilamentErpClient> m_erp_client;
};

} // namespace VectorSmartFilament
