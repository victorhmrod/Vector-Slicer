#include "LocalFilamentCache.hpp"

#include <utility>

namespace VectorSmartFilament {

LocalFilamentCache::LocalFilamentCache(std::shared_ptr<IFilamentRepository> repository) : m_repository(std::move(repository)) {}

Result<void> LocalFilamentCache::put(const FilamentSpool& spool) { return m_repository->save(spool); }

Result<FilamentSpool> LocalFilamentCache::get(const std::string& uuid) { return m_repository->find_by_uuid(uuid); }

Result<std::vector<FilamentSpool>> LocalFilamentCache::all() { return m_repository->list(); }

} // namespace VectorSmartFilament
