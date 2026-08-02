#pragma once

#include <string>

#include "../Application/Interfaces.hpp"

struct sqlite3;

namespace VectorSmartFilament {

class SQLiteFilamentRepository final : public IFilamentRepository
{
public:
    explicit SQLiteFilamentRepository(std::string database_path);
    ~SQLiteFilamentRepository() override;

    Result<void>                       save(const FilamentSpool& spool) override;
    Result<FilamentSpool>              find_by_uuid(const std::string& uuid) override;
    Result<std::vector<FilamentSpool>> list() override;

private:
    Result<void> open();
    Result<void> migrate();

    std::string m_database_path;
    sqlite3*    m_database = nullptr;
};

} // namespace VectorSmartFilament
