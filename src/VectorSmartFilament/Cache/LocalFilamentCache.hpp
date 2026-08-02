#pragma once

#include "../Application/Interfaces.hpp"

namespace VectorSmartFilament {

class LocalFilamentCache final : public IFilamentCache
{
public:
    explicit LocalFilamentCache(std::shared_ptr<IFilamentRepository> repository);

    Result<void>                       put(const FilamentSpool& spool) override;
    Result<FilamentSpool>              get(const std::string& uuid) override;
    Result<std::vector<FilamentSpool>> all() override;

private:
    std::shared_ptr<IFilamentRepository> m_repository;
};

} // namespace VectorSmartFilament
