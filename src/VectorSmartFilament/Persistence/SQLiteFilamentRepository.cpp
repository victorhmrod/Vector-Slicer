#include "SQLiteFilamentRepository.hpp"

#include <utility>

#include <sqlite3.h>

namespace VectorSmartFilament {

namespace {

std::string sqlite_error(sqlite3* database) { return database ? sqlite3_errmsg(database) : "SQLite indisponivel"; }

Result<void> exec(sqlite3* database, const char* sql)
{
    char* error = nullptr;
    if (sqlite3_exec(database, sql, nullptr, nullptr, &error) == SQLITE_OK)
        return {""};

    const std::string message = error ? error : sqlite_error(database);
    sqlite3_free(error);
    return {message};
}

} // namespace

SQLiteFilamentRepository::SQLiteFilamentRepository(std::string database_path) : m_database_path(std::move(database_path)) {}

SQLiteFilamentRepository::~SQLiteFilamentRepository()
{
    if (m_database)
        sqlite3_close(m_database);
}

Result<void> SQLiteFilamentRepository::open()
{
    if (m_database)
        return {""};

    if (sqlite3_open(m_database_path.c_str(), &m_database) != SQLITE_OK)
        return {sqlite_error(m_database)};

    return migrate();
}

Result<void> SQLiteFilamentRepository::migrate()
{
    return exec(m_database, "CREATE TABLE IF NOT EXISTS filament_spools ("
                            "uuid TEXT PRIMARY KEY NOT NULL,"
                            "payload TEXT NOT NULL,"
                            "dirty INTEGER NOT NULL DEFAULT 0,"
                            "updated_at TEXT NOT NULL"
                            ");");
}

Result<void> SQLiteFilamentRepository::save(const FilamentSpool& spool)
{
    Result<void> opened = open();
    if (!opened.ok())
        return opened;

    sqlite3_stmt* statement = nullptr;
    const char*   sql =
        "INSERT INTO filament_spools(uuid, payload, dirty, updated_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(uuid) DO UPDATE SET payload = excluded.payload, dirty = excluded.dirty, updated_at = excluded.updated_at;";
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK)
        return {sqlite_error(m_database)};

    const std::string payload = to_json(spool);
    sqlite3_bind_text(statement, 1, spool.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, spool.dirty ? 1 : 0);
    sqlite3_bind_text(statement, 4, spool.updated_at.c_str(), -1, SQLITE_TRANSIENT);

    const int step = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (step != SQLITE_DONE)
        return {sqlite_error(m_database)};

    return {""};
}

Result<FilamentSpool> SQLiteFilamentRepository::find_by_uuid(const std::string& uuid)
{
    Result<void> opened = open();
    if (!opened.ok())
        return {{}, opened.error};

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_database, "SELECT payload FROM filament_spools WHERE uuid = ?;", -1, &statement, nullptr) != SQLITE_OK)
        return {{}, sqlite_error(m_database)};

    sqlite3_bind_text(statement, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return {{}, "Bobina nao encontrada"};
    }

    const unsigned char* text  = sqlite3_column_text(statement, 0);
    FilamentSpool        spool = filament_spool_from_json(reinterpret_cast<const char*>(text));
    sqlite3_finalize(statement);
    return {spool, ""};
}

Result<std::vector<FilamentSpool>> SQLiteFilamentRepository::list()
{
    Result<void> opened = open();
    if (!opened.ok())
        return {{}, opened.error};

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_database, "SELECT payload FROM filament_spools ORDER BY updated_at DESC;", -1, &statement, nullptr) !=
        SQLITE_OK)
        return {{}, sqlite_error(m_database)};

    std::vector<FilamentSpool> spools;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(statement, 0);
        spools.push_back(filament_spool_from_json(reinterpret_cast<const char*>(text)));
    }

    sqlite3_finalize(statement);
    return {spools, ""};
}

} // namespace VectorSmartFilament
