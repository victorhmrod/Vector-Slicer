#include "FilamentSyncService.hpp"

#include <utility>

namespace VectorSmartFilament {

FilamentSyncService::FilamentSyncService(std::shared_ptr<IFilamentCache> cache, std::shared_ptr<IFilamentErpClient> erp_client)
    : m_cache(std::move(cache)), m_erp_client(std::move(erp_client))
{}

Result<void> FilamentSyncService::sync()
{
    if (!m_erp_client || !m_erp_client->online())
        return {"ERP offline"};

    Result<std::vector<FilamentSpool>> spools = m_cache->all();
    if (!spools.ok())
        return {spools.error};

    for (FilamentSpool& spool : spools.value) {
        if (!spool.dirty)
            continue;

        Result<void> saved = m_erp_client->save(spool);
        if (!saved.ok())
            return saved;

        spool.dirty         = false;
        Result<void> cached = m_cache->put(spool);
        if (!cached.ok())
            return cached;
    }

    return {""};
}

} // namespace VectorSmartFilament
