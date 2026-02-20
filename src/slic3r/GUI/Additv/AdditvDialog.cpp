#include "AdditvDialog.hpp"
#include "AdditvConfig.hpp"
#include "AdditvOAuth.hpp"
#include "slic3r/GUI/I18N.hpp"

#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statbox.h>

#include <iomanip>
#include <sstream>
#include <thread>

namespace Slic3r { namespace GUI { namespace Additv {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string AdditvDialog::format_time(int seconds)
{
    if (seconds <= 0) return "Unknown";
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    std::ostringstream ss;
    if (h > 0) ss << h << "h ";
    ss << m << "m";
    return ss.str();
}

std::string AdditvDialog::format_size(uintmax_t bytes)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= 1024 * 1024)
        ss << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB";
    else if (bytes >= 1024)
        ss << static_cast<double>(bytes) / 1024.0 << " KB";
    else
        ss << bytes << " B";
    return ss.str();
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AdditvDialog::AdditvDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _L("Send to Additv"),
               wxDefaultPosition, wxSize(460, 480),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    build_ui();
    populate_dropdowns();
    enable_form(true);
}

// ---------------------------------------------------------------------------
// Public setters
// ---------------------------------------------------------------------------

void AdditvDialog::set_gcode_path(const std::string &path)
{
    m_gcode_path = path;
    if (m_file_label) {
        wxFileName fn(path);
        m_file_label->SetLabel(fn.GetFullName());
    }
    if (m_size_label) {
        wxFileName fn(path);
        if (fn.FileExists())
            m_size_label->SetLabel(format_size(fn.GetSize().GetValue()));
    }
}

void AdditvDialog::set_gcode_name(const std::string &name)
{
    if (m_name_input)
        m_name_input->SetValue(name);
}

void AdditvDialog::set_filament_type_hint(const std::string &type)
{
    m_filament_type_hint = type;
}

void AdditvDialog::set_printer_model(const std::string &model)
{
    m_printer_model = model;
}

void AdditvDialog::set_estimated_time(int seconds)
{
    m_estimated_time = seconds;
    if (m_time_label)
        m_time_label->SetLabel(format_time(seconds));
}

// ---------------------------------------------------------------------------
// UI layout
// ---------------------------------------------------------------------------

void AdditvDialog::build_ui()
{
    auto *main_sizer = new wxBoxSizer(wxVERTICAL);

    // --- GCode info ---
    auto *gcode_box = new wxStaticBoxSizer(wxVERTICAL, this, _L("GCode"));

    auto *name_row = new wxBoxSizer(wxHORIZONTAL);
    name_row->Add(new wxStaticText(this, wxID_ANY, _L("Name:")),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    m_name_input = new wxTextCtrl(this, wxID_ANY, "");
    name_row->Add(m_name_input, 1, wxEXPAND);
    gcode_box->Add(name_row, 0, wxEXPAND | wxALL, 5);

    auto add_info_row = [&](const wxString &label, wxStaticText *&out) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, label), 0, wxRIGHT, 5);
        out = new wxStaticText(this, wxID_ANY, "-");
        row->Add(out, 1);
        gcode_box->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    };

    add_info_row(_L("File:"),      m_file_label);
    add_info_row(_L("Size:"),      m_size_label);
    add_info_row(_L("Est. time:"), m_time_label);
    main_sizer->Add(gcode_box, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // --- Job Configuration ---
    auto *job_box = new wxStaticBoxSizer(wxVERTICAL, this,
                                          _L("Job Configuration"));
    auto *grid = new wxFlexGridSizer(2, 5, 10);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(this, wxID_ANY, _L("Filament:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_filament_choice = new wxChoice(this, wxID_ANY);
    grid->Add(m_filament_choice, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _L("Quantity:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_quantity_spin = new wxSpinCtrl(this, wxID_ANY, "1",
                                     wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 1, 100, 1);
    grid->Add(m_quantity_spin, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _L("Order:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_order_choice = new wxChoice(this, wxID_ANY);
    m_order_choice->Append(_L("(none)"));
    m_order_choice->SetSelection(0);
    grid->Add(m_order_choice, 1, wxEXPAND);

    job_box->Add(grid, 1, wxEXPAND | wxALL, 5);
    main_sizer->Add(job_box, 0, wxEXPAND | wxALL, 10);

    // --- Buttons ---
    auto *btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();
    auto *cancel_btn = new wxButton(this, wxID_CANCEL, _L("Cancel"));
    m_send_btn = new wxButton(this, wxID_ANY, _L("Send"));
    m_send_btn->SetDefault();
    btn_sizer->Add(cancel_btn, 0, wxRIGHT, 5);
    btn_sizer->Add(m_send_btn, 0);
    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // --- Progress (hidden initially) ---
    m_progress_label = new wxStaticText(this, wxID_ANY, "");
    m_progress_bar   = new wxGauge(this, wxID_ANY, 100);
    m_progress_label->Hide();
    m_progress_bar->Hide();
    main_sizer->Add(m_progress_label, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    main_sizer->Add(m_progress_bar, 0, wxEXPAND | wxALL, 10);

    SetSizerAndFit(main_sizer);

    // Events
    m_send_btn->Bind(wxEVT_BUTTON, &AdditvDialog::on_send, this);
}

// ---------------------------------------------------------------------------
// State management
// ---------------------------------------------------------------------------

void AdditvDialog::enable_form(bool enable)
{
    m_filament_choice->Enable(enable);
    m_quantity_spin->Enable(enable);
    m_order_choice->Enable(enable);
    m_send_btn->Enable(enable);
}

void AdditvDialog::populate_dropdowns()
{
    std::string error;

    // Filaments
    m_filament_choice->Clear();
    m_filaments.clear();
    if (AdditvClient::get_filaments(m_filaments, m_filament_type_hint, error)) {
        for (const auto &f : m_filaments) {
            wxString label = wxString::Format("%s %s \u2014 %s",
                                              f.plastic_type, f.color_name,
                                              f.manufacturer);
            m_filament_choice->Append(label);
        }
        if (!m_filaments.empty())
            m_filament_choice->SetSelection(0);
    }

    // Orders
    m_order_choice->Clear();
    m_order_choice->Append(_L("(none)"));
    m_orders.clear();
    if (AdditvClient::get_orders(m_orders, error)) {
        for (const auto &o : m_orders) {
            wxString label;
            if (!o.shopify_order_number.empty())
                label = wxString::Format("#%s \u2014 %s",
                                         o.shopify_order_number, o.description);
            else
                label = wxString::Format("#%lld \u2014 %s",
                                         static_cast<long long>(o.id),
                                         o.description);
            m_order_choice->Append(label);
        }
    }
    m_order_choice->SetSelection(0);
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void AdditvDialog::on_send(wxCommandEvent & /*evt*/)
{
    // If no gcode path set, prompt user to select a file
    if (m_gcode_path.empty()) {
        wxFileDialog dlg(this, _L("Select G-code file to upload"),
                         "", "", "G-code files (*.gcode;*.bgcode)|*.gcode;*.bgcode",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK) return;
        set_gcode_path(dlg.GetPath().ToStdString());
    }

    int sel = m_filament_choice->GetSelection();
    if (sel < 0 || sel >= static_cast<int>(m_filaments.size())) {
        wxMessageBox(_L("Please select a filament."),
                     _L("Additv"), wxOK | wxICON_WARNING, this);
        return;
    }

    int64_t     filament_id   = m_filaments[sel].id;
    std::string filament_type = m_filaments[sel].plastic_type;
    int         quantity      = m_quantity_spin->GetValue();

    int     order_sel = m_order_choice->GetSelection();
    int64_t order_id  = 0;
    if (order_sel > 0 && order_sel <= static_cast<int>(m_orders.size()))
        order_id = m_orders[order_sel - 1].id;

    // Disable form during upload
    enable_form(false);
    m_send_btn->Disable();
    m_progress_bar->Show();
    m_progress_label->Show();
    m_progress_label->SetLabel(_L("Uploading..."));
    m_progress_bar->SetValue(0);
    Layout();

    std::string upload_name = m_name_input->GetValue().ToStdString();

    std::thread([this, filament_id, filament_type, quantity, order_id, upload_name]() {
        std::string  error;
        UploadResult upload_result;

        // 1. Upload gcode
        bool ok = AdditvClient::upload_gcode(
            m_gcode_path, upload_name, m_printer_model, filament_type,
            m_estimated_time, upload_result, error,
            [this](float progress) {
                wxTheApp->CallAfter([this, progress]() {
                    m_progress_bar->SetValue(
                        static_cast<int>(progress * 100.0f));
                });
            });

        if (!ok) {
            wxTheApp->CallAfter([this, error]() {
                m_progress_bar->Hide();
                m_progress_label->Hide();
                enable_form(true);
                m_send_btn->Enable();
                Layout();
                wxMessageBox(
                    wxString::Format(_L("Upload failed: %s"), error),
                    _L("Additv"), wxOK | wxICON_ERROR, this);
            });
            return;
        }

        // 2. Create jobs
        wxTheApp->CallAfter([this]() {
            m_progress_label->SetLabel(_L("Creating print jobs..."));
            m_progress_bar->Pulse();
        });

        JobsResult jobs_result;
        ok = AdditvClient::create_jobs(upload_result.gcode_id, filament_id,
                                       quantity, order_id, jobs_result, error);

        wxTheApp->CallAfter([this, ok, error, jobs_result]() {
            m_progress_bar->Hide();
            m_progress_label->Hide();
            enable_form(true);
            m_send_btn->Enable();
            Layout();

            if (ok) {
                wxMessageBox(
                    wxString::Format(
                        _L("Successfully created %d print job(s)!"),
                        jobs_result.count),
                    _L("Additv"), wxOK | wxICON_INFORMATION, this);
                EndModal(wxID_OK);
            } else {
                wxMessageBox(
                    wxString::Format(
                        _L("Failed to create jobs: %s"), error),
                    _L("Additv"), wxOK | wxICON_ERROR, this);
            }
        });
    }).detach();
}

}}} // namespace Slic3r::GUI::Additv
