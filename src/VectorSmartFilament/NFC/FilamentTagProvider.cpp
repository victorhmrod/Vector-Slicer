#include "FilamentTagProvider.hpp"

#include <utility>

namespace VectorSmartFilament {

MemoryTagProvider::MemoryTagProvider(std::string provider_name) : m_name(std::move(provider_name)) {}

std::string MemoryTagProvider::name() const { return m_name; }

Result<void> MemoryTagProvider::write(const FilamentTag& tag)
{
    m_tag     = tag;
    m_has_tag = true;
    return {""};
}

Result<FilamentTag> MemoryTagProvider::read()
{
    if (!m_has_tag)
        return {{}, "Tag vazia"};
    return {m_tag, ""};
}

} // namespace VectorSmartFilament
