#include "FilamentTag.hpp"

#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace VectorSmartFilament {

namespace {

std::string signature_payload(const FilamentTag& tag) { return tag.uuid + ":" + std::to_string(tag.version); }

std::string to_hex(const unsigned char* bytes, unsigned int length)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i)
        stream << std::setw(2) << static_cast<int>(bytes[i]);
    return stream.str();
}

} // namespace

std::string to_json(const FilamentTag& tag)
{
    nlohmann::json value = {
        {"uuid", tag.uuid},
        {"version", tag.version},
    };
    if (!tag.signature.empty())
        value["signature"] = tag.signature;
    return value.dump();
}

FilamentTag filament_tag_from_json(const std::string& text)
{
    const nlohmann::json value = nlohmann::json::parse(text);
    FilamentTag          tag;
    tag.uuid      = value.value("uuid", "");
    tag.version   = value.value("version", 1);
    tag.signature = value.value("signature", "");
    return tag;
}

std::string sign_tag(const FilamentTag& tag, const std::string& secret)
{
    unsigned int      length = 0;
    unsigned char     digest[EVP_MAX_MD_SIZE];
    const std::string payload = signature_payload(tag);
    HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()), reinterpret_cast<const unsigned char*>(payload.data()),
         payload.size(), digest, &length);
    return to_hex(digest, length);
}

bool verify_tag_signature(const FilamentTag& tag, const std::string& secret)
{
    if (tag.signature.empty())
        return false;
    return tag.signature == sign_tag(tag, secret);
}

} // namespace VectorSmartFilament
