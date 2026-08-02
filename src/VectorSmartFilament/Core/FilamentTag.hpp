#pragma once

#include <string>

namespace VectorSmartFilament {

struct FilamentTag
{
    std::string uuid;
    int         version = 1;
    std::string signature;
};

std::string to_json(const FilamentTag& tag);
FilamentTag filament_tag_from_json(const std::string& text);
std::string sign_tag(const FilamentTag& tag, const std::string& secret);
bool        verify_tag_signature(const FilamentTag& tag, const std::string& secret);

} // namespace VectorSmartFilament
