#include "VectorSmartFilamentDashboard.hpp"

#include <sstream>
#include <utility>

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace Slic3r { namespace GUI {

namespace {

wxString grams(double value)
{
    std::ostringstream stream;
    stream << value << " g";
    return wxString::FromUTF8(stream.str());
}

wxString money(double value)
{
    std::ostringstream stream;
    stream << "R$ " << value;
    return wxString::FromUTF8(stream.str());
}

} // namespace

VectorSmartFilamentDashboardDialog::VectorSmartFilamentDashboardDialog(wxWindow*                                            parent,
                                                                       std::shared_ptr<VectorSmartFilament::IFilamentCache> cache)
    : wxDialog(parent, wxID_ANY, "Vector Smart Filament", wxDefaultPosition, wxSize(980, 560), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_cache(std::move(cache))
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* header = new wxBoxSizer(wxHORIZONTAL);
    header->Add(new wxStaticText(this, wxID_ANY, "Bobinas"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

    wxButton* refresh = new wxButton(this, wxID_ANY, "Atualizar");
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { reload(); });
    header->AddStretchSpacer();
    header->Add(refresh, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(header, 0, wxEXPAND | wxALL, 12);

    m_spool_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_spool_list->AppendColumn("UUID", wxLIST_FORMAT_LEFT, 120);
    m_spool_list->AppendColumn("Cor", wxLIST_FORMAT_LEFT, 90);
    m_spool_list->AppendColumn("Peso", wxLIST_FORMAT_RIGHT, 90);
    m_spool_list->AppendColumn("Material", wxLIST_FORMAT_LEFT, 90);
    m_spool_list->AppendColumn("Fornecedor", wxLIST_FORMAT_LEFT, 130);
    m_spool_list->AppendColumn("Lote", wxLIST_FORMAT_LEFT, 90);
    m_spool_list->AppendColumn("Preco", wxLIST_FORMAT_RIGHT, 90);
    m_spool_list->AppendColumn("Perfil", wxLIST_FORMAT_LEFT, 170);
    m_spool_list->AppendColumn("Impressora", wxLIST_FORMAT_LEFT, 120);
    m_spool_list->AppendColumn("AMS", wxLIST_FORMAT_LEFT, 70);
    m_spool_list->AppendColumn("Slot", wxLIST_FORMAT_LEFT, 60);
    root->Add(m_spool_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    SetSizer(root);
    reload();
}

void VectorSmartFilamentDashboardDialog::reload()
{
    m_spool_list->DeleteAllItems();
    if (!m_cache)
        return;

    VectorSmartFilament::Result<std::vector<VectorSmartFilament::FilamentSpool>> result = m_cache->all();
    if (!result.ok())
        return;

    long row = 0;
    for (const VectorSmartFilament::FilamentSpool& spool : result.value) {
        row = m_spool_list->InsertItem(row, wxString::FromUTF8(spool.uuid));
        m_spool_list->SetItem(row, 1, wxString::FromUTF8(spool.color));
        m_spool_list->SetItem(row, 2, grams(spool.current_weight));
        m_spool_list->SetItem(row, 3, wxString::FromUTF8(spool.material));
        m_spool_list->SetItem(row, 4, wxString::FromUTF8(spool.supplier));
        m_spool_list->SetItem(row, 5, wxString::FromUTF8(spool.lot_number));
        m_spool_list->SetItem(row, 6, money(spool.purchase_price));
        m_spool_list->SetItem(row, 7, wxString::FromUTF8(spool.recommended_profile));
        m_spool_list->SetItem(row, 8, wxString::FromUTF8(spool.current_printer));
        m_spool_list->SetItem(row, 9, wxString::FromUTF8(spool.current_ams));
        m_spool_list->SetItem(row, 10, wxString::FromUTF8(spool.current_slot));
        ++row;
    }
}

}} // namespace Slic3r::GUI
