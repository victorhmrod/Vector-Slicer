#pragma once

#include "../Application/Interfaces.hpp"

namespace VectorSmartFilament {

class MemoryTagProvider final : public IFilamentTagProvider
{
public:
    explicit MemoryTagProvider(std::string provider_name);

    std::string         name() const override;
    Result<void>        write(const FilamentTag& tag) override;
    Result<FilamentTag> read() override;

private:
    std::string m_name;
    FilamentTag m_tag;
    bool        m_has_tag = false;
};

using TigerTagProvider         = MemoryTagProvider;
using NTAG213Provider          = MemoryTagProvider;
using NTAG215Provider          = MemoryTagProvider;
using NTAG216Provider          = MemoryTagProvider;
using MiFareUltralightProvider = MemoryTagProvider;

} // namespace VectorSmartFilament
