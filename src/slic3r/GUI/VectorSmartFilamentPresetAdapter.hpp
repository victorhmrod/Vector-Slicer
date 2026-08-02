#ifndef slic3r_GUI_VectorSmartFilamentPresetAdapter_hpp_
#define slic3r_GUI_VectorSmartFilamentPresetAdapter_hpp_

#include <cstddef>

#include "libslic3r/Config.hpp"
#include "VectorSmartFilament/Application/Interfaces.hpp"

namespace Slic3r { namespace GUI {

class VectorSmartFilamentPresetAdapter
{
public:
    bool apply(const VectorSmartFilament::FilamentPresetUpdate& update, DynamicPrintConfig& config, size_t filament_index) const;
};

}} // namespace Slic3r::GUI

#endif
