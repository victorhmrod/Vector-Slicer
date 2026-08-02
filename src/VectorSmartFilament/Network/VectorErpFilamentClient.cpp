#include "VectorErpFilamentClient.hpp"

#include <exception>
#include <utility>

#include <curl/curl.h>

namespace VectorSmartFilament {

namespace {

size_t append_response(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* response = static_cast<std::string*>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

} // namespace

VectorErpFilamentClient::VectorErpFilamentClient(std::string base_url, std::string access_token, std::string organization_id)
    : m_base_url(std::move(base_url)), m_access_token(std::move(access_token)), m_organization_id(std::move(organization_id))
{
    while (!m_base_url.empty() && m_base_url.back() == '/')
        m_base_url.pop_back();
}

Result<void> VectorErpFilamentClient::save(const FilamentSpool& spool)
{
    HttpResponse response = request("PUT", "/api/v1/smart-filament/spools/" + escaped(spool.uuid), to_json(spool));
    if (!response.error.empty())
        return {response.error};

    if (response.status < 200 || response.status >= 300)
        return {"ERP retornou HTTP " + std::to_string(response.status)};

    return {""};
}

Result<FilamentSpool> VectorErpFilamentClient::find_by_uuid(const std::string& uuid)
{
    HttpResponse response = request("GET", "/api/v1/smart-filament/spools/" + escaped(uuid));
    if (!response.error.empty())
        return {{}, response.error};

    if (response.status == 404)
        return {{}, "Bobina nao encontrada no ERP"};

    if (response.status < 200 || response.status >= 300)
        return {{}, "ERP retornou HTTP " + std::to_string(response.status)};

    try {
        return {filament_spool_from_json(response.body), ""};
    } catch (const std::exception& e) {
        return {{}, e.what()};
    }
}

bool VectorErpFilamentClient::online() const
{
    HttpResponse response = request("GET", "/api/health");
    return response.error.empty() && response.status >= 200 && response.status < 500;
}

VectorErpFilamentClient::HttpResponse VectorErpFilamentClient::request(const std::string& method,
                                                                       const std::string& path,
                                                                       const std::string& body) const
{
    HttpResponse response;
    CURL*        curl = curl_easy_init();
    if (!curl) {
        response.error = "Nao foi possivel inicializar libcurl";
        return response;
    }

    curl_slist* headers = nullptr;
    headers             = curl_slist_append(headers, "Content-Type: application/json");
    headers             = curl_slist_append(headers, "Accept: application/json");

    const std::string org_header = "X-Organization-Id: " + m_organization_id;
    if (!m_organization_id.empty())
        headers = curl_slist_append(headers, org_header.c_str());

    const std::string auth_header = "Authorization: Bearer " + m_access_token;
    if (!m_access_token.empty())
        headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url_for(path).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    if (method == "PUT" || method == "POST" || method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    } else if (method != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    }

    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
        response.error = curl_easy_strerror(code);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

std::string VectorErpFilamentClient::url_for(const std::string& path) const { return m_base_url + path; }

std::string VectorErpFilamentClient::escaped(const std::string& value) const
{
    CURL* curl = curl_easy_init();
    if (!curl)
        return value;

    char*       encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result  = encoded ? encoded : value;
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

} // namespace VectorSmartFilament
