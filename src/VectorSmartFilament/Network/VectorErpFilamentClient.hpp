#pragma once

#include <string>

#include "../Application/Interfaces.hpp"

namespace VectorSmartFilament {

class VectorErpFilamentClient final : public IFilamentErpClient
{
public:
    VectorErpFilamentClient(std::string base_url, std::string access_token, std::string organization_id);

    Result<void>          save(const FilamentSpool& spool) override;
    Result<FilamentSpool> find_by_uuid(const std::string& uuid) override;
    bool                  online() const override;

private:
    struct HttpResponse
    {
        long        status = 0;
        std::string body;
        std::string error;
    };

    HttpResponse request(const std::string& method, const std::string& path, const std::string& body = {}) const;
    std::string  url_for(const std::string& path) const;
    std::string  escaped(const std::string& value) const;

    std::string m_base_url;
    std::string m_access_token;
    std::string m_organization_id;
};

} // namespace VectorSmartFilament
