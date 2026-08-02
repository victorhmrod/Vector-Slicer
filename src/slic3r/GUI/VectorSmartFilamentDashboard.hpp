#ifndef slic3r_GUI_VectorSmartFilamentDashboard_hpp_
#define slic3r_GUI_VectorSmartFilamentDashboard_hpp_

#include <memory>

#include <wx/dialog.h>
#include <wx/listctrl.h>

#include "VectorSmartFilament/Application/Interfaces.hpp"

namespace Slic3r { namespace GUI {

class VectorSmartFilamentDashboardDialog : public wxDialog
{
public:
    VectorSmartFilamentDashboardDialog(wxWindow* parent, std::shared_ptr<VectorSmartFilament::IFilamentCache> cache);

    void reload();

private:
    std::shared_ptr<VectorSmartFilament::IFilamentCache> m_cache;
    wxListCtrl*                                          m_spool_list = nullptr;
};

}} // namespace Slic3r::GUI

#endif
