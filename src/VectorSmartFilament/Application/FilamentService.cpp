#include "FilamentService.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

namespace VectorSmartFilament {

namespace {

std::string now_utc()
{
    const auto        now  = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm           tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::string generate_uuid()
{
    std::random_device                 device;
    std::mt19937                       generator(device());
    std::uniform_int_distribution<int> distribution(0, 15);
    std::ostringstream                 stream;
    stream << "FIL-";
    for (int i = 0; i < 8; ++i)
        stream << std::uppercase << std::hex << distribution(generator);
    return stream.str();
}

} // namespace

FilamentService::FilamentService(std::shared_ptr<IFilamentCache>     cache,
                                 std::shared_ptr<IFilamentErpClient> erp_client,
                                 std::shared_ptr<FilamentEventBus>   events,
                                 std::string                         tag_secret)
    : m_cache(std::move(cache)), m_erp_client(std::move(erp_client)), m_events(std::move(events)), m_tag_secret(std::move(tag_secret))
{}

Result<FilamentSpool> FilamentService::register_spool(FilamentSpool spool, ITagWriter& tag_writer)
{
    if (spool.uuid.empty())
        spool.uuid = generate_uuid();

    const std::string timestamp = now_utc();
    if (spool.created_at.empty())
        spool.created_at = timestamp;
    spool.updated_at    = timestamp;
    spool.cost_per_gram = spool.initial_weight > 0.0 ? spool.purchase_price / spool.initial_weight : 0.0;
    spool.dirty         = true;

    FilamentTag tag{spool.uuid, 1, ""};
    if (!m_tag_secret.empty())
        tag.signature = sign_tag(tag, m_tag_secret);

    const Result<void> tag_result = tag_writer.write(tag);
    if (!tag_result.ok())
        return {spool, tag_result.error};

    if (m_erp_client && m_erp_client->online()) {
        const Result<void> erp_result = m_erp_client->save(spool);
        if (erp_result.ok())
            spool.dirty = false;
    }

    const Result<void> cache_result = m_cache->put(spool);
    if (!cache_result.ok())
        return {spool, cache_result.error};

    if (m_events)
        m_events->publish({"FilamentSpoolRegistered", spool.uuid});
    return {spool, ""};
}

Result<FilamentSpool> FilamentService::read_spool(ITagReader& tag_reader)
{
    const Result<FilamentTag> tag_result = tag_reader.read();
    if (!tag_result.ok())
        return {{}, tag_result.error};

    if (!m_tag_secret.empty() && !verify_tag_signature(tag_result.value, m_tag_secret))
        return {{}, "Tag invalida ou clonada"};

    Result<FilamentSpool> cached = m_cache->get(tag_result.value.uuid);
    if (cached.ok())
        return cached;

    if (!m_erp_client || !m_erp_client->online())
        return {{}, "Bobina nao encontrada no cache local"};

    Result<FilamentSpool> remote = m_erp_client->find_by_uuid(tag_result.value.uuid);
    if (!remote.ok())
        return remote;

    m_cache->put(remote.value);
    if (m_events)
        m_events->publish({"FilamentSpoolRead", remote.value.uuid});
    return remote;
}

Result<FilamentSpool> FilamentService::update_consumed_weight(const std::string& uuid, double consumed_grams)
{
    Result<FilamentSpool> result = m_cache->get(uuid);
    if (!result.ok())
        return result;

    result.value.current_weight = std::max(0.0, result.value.current_weight - consumed_grams);
    result.value.updated_at     = now_utc();
    result.value.dirty          = true;

    if (m_erp_client && m_erp_client->online()) {
        const Result<void> erp_result = m_erp_client->save(result.value);
        if (erp_result.ok())
            result.value.dirty = false;
    }

    const Result<void> cache_result = m_cache->put(result.value);
    if (!cache_result.ok())
        return {result.value, cache_result.error};

    if (m_events)
        m_events->publish({"FilamentSpoolWeightUpdated", uuid});
    return result;
}

} // namespace VectorSmartFilament
