#pragma once

#include "Interfaces.hpp"

namespace VectorSmartFilament {

class FilamentProfileService final : public IFilamentProfileService
{
public:
    FilamentPresetUpdate build_preset_update(const FilamentSpool& spool) const override;
};

} // namespace VectorSmartFilament
