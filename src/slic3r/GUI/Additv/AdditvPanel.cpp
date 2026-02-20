#include "AdditvPanel.hpp"
#include "AdditvConfig.hpp"
#include "AdditvOAuth.hpp"
#include "slic3r/GUI/I18N.hpp"

#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>

#include <iomanip>
#include <sstream>
#include <thread>

namespace Slic3r { namespace GUI { namespace Additv {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string AdditvPanel::format_time(int seconds)
{
    if (seconds <= 0) return "Unknown";
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    std::ostringstream ss;
    if (h > 0) ss << h << "h ";
    ss << m << "m";
    return ss.str();
}

std::string AdditvPanel::format_size(uintmax_t bytes)
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

AdditvPanel::AdditvPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY)
{
    build_ui();
}

// ---------------------------------------------------------------------------
// Public setters
// ---------------------------------------------------------------------------

void AdditvPanel::set_gcode_path(const std::string &path)
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
    enable_form(AdditvConfig::is_logged_in());
}

void AdditvPanel::set_filament_type_hint(const std::string &type)
{
    m_filament_type_hint = type;
}

void AdditvPanel::set_printer_model(const std::string &model)
{
    m_printer_model = model;
}

void AdditvPanel::set_estimated_time(int seconds)
{
    m_estimated_time = seconds;
    if (m_time_label)
        m_time_label->SetLabel(format_time(seconds));
}

void AdditvPanel::on_activate()
{
    update_login_state();
}

// ---------------------------------------------------------------------------
// UI layout
// ---------------------------------------------------------------------------

void AdditvPanel::build_ui()
{
    // Outer wrapper to center the content with a max width
    auto *outer_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto *content_panel = new wxPanel(this, wxID_ANY);
    content_panel->SetMaxSize(wxSize(500, -1));
    auto *main_sizer = new wxBoxSizer(wxVERTICAL);

    // --- Server Settings ---
    auto *url_box = new wxStaticBoxSizer(wxVERTICAL, content_panel,
                                          _L("Server"));
    auto *grid_cfg = new wxFlexGridSizer(2, 5, 10);
    grid_cfg->AddGrowableCol(1);

    grid_cfg->Add(new wxStaticText(content_panel, wxID_ANY, _L("URL:")),
                  0, wxALIGN_CENTER_VERTICAL);
    m_url_input = new wxTextCtrl(content_panel, wxID_ANY,
                                  AdditvConfig::get_server_url());
    grid_cfg->Add(m_url_input, 1, wxEXPAND);

    grid_cfg->Add(new wxStaticText(content_panel, wxID_ANY, _L("Client ID:")),
                  0, wxALIGN_CENTER_VERTICAL);
    m_client_id_input = new wxTextCtrl(content_panel, wxID_ANY,
                                        AdditvConfig::get_client_id());
    grid_cfg->Add(m_client_id_input, 1, wxEXPAND);

    url_box->Add(grid_cfg, 0, wxEXPAND | wxALL, 5);
    m_save_url_btn = new wxButton(content_panel, wxID_ANY, _L("Save Settings"));
    url_box->Add(m_save_url_btn, 0, wxALIGN_RIGHT | wxALL, 5);
    main_sizer->Add(url_box, 0, wxEXPAND | wxALL, 10);

    // --- Connection ---
    auto *conn_box = new wxStaticBoxSizer(wxVERTICAL, content_panel,
                                           _L("Connection"));
    auto *conn_row = new wxBoxSizer(wxHORIZONTAL);

    m_status_label = new wxStaticText(content_panel, wxID_ANY,
                                       _L("Not connected"));
    m_login_btn  = new wxButton(content_panel, wxID_ANY, _L("Login to Additv"));
    m_logout_btn = new wxButton(content_panel, wxID_ANY, _L("Logout"));

    conn_row->Add(m_status_label, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    conn_row->Add(m_login_btn, 0, wxRIGHT, 5);
    conn_row->Add(m_logout_btn, 0);
    conn_box->Add(conn_row, 0, wxEXPAND | wxALL, 5);
    main_sizer->Add(conn_box, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // --- GCode info ---
    auto *gcode_box = new wxStaticBoxSizer(wxVERTICAL, content_panel,
                                            _L("GCode"));

    auto add_info_row = [&](const wxString &label, wxStaticText *&out) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(content_panel, wxID_ANY, label),
                 0, wxRIGHT, 5);
        out = new wxStaticText(content_panel, wxID_ANY, "-");
        row->Add(out, 1);
        gcode_box->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    };

    add_info_row(_L("File:"),      m_file_label);
    add_info_row(_L("Size:"),      m_size_label);
    add_info_row(_L("Est. time:"), m_time_label);
    main_sizer->Add(gcode_box, 0, wxEXPAND | wxALL, 10);

    // --- Job Configuration ---
    auto *job_box = new wxStaticBoxSizer(wxVERTICAL, content_panel,
                                          _L("Job Configuration"));
    auto *grid = new wxFlexGridSizer(2, 5, 10);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(content_panel, wxID_ANY, _L("Filament:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_filament_choice = new wxChoice(content_panel, wxID_ANY);
    grid->Add(m_filament_choice, 1, wxEXPAND);

    grid->Add(new wxStaticText(content_panel, wxID_ANY, _L("Quantity:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_quantity_spin = new wxSpinCtrl(content_panel, wxID_ANY, "1",
                                     wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 1, 100, 1);
    grid->Add(m_quantity_spin, 1, wxEXPAND);

    grid->Add(new wxStaticText(content_panel, wxID_ANY, _L("Order:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_order_choice = new wxChoice(content_panel, wxID_ANY);
    m_order_choice->Append(_L("(none)"));
    m_order_choice->SetSelection(0);
    grid->Add(m_order_choice, 1, wxEXPAND);

    job_box->Add(grid, 1, wxEXPAND | wxALL, 5);
    main_sizer->Add(job_box, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // --- Send button ---
    main_sizer->AddSpacer(10);
    m_send_btn = new wxButton(content_panel, wxID_ANY, _L("Send to Farm"));
    m_send_btn->SetMinSize(wxSize(-1, 40));
    main_sizer->Add(m_send_btn, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // --- Progress (hidden initially) ---
    main_sizer->AddSpacer(5);
    m_progress_label = new wxStaticText(content_panel, wxID_ANY, "");
    m_progress_bar   = new wxGauge(content_panel, wxID_ANY, 100);
    m_progress_label->Hide();
    m_progress_bar->Hide();
    main_sizer->Add(m_progress_label, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    main_sizer->Add(m_progress_bar, 0, wxEXPAND | wxALL, 10);

    content_panel->SetSizer(main_sizer);

    // Center the content panel
    outer_sizer->AddStretchSpacer();
    outer_sizer->Add(content_panel, 0, wxEXPAND | wxTOP, 10);
    outer_sizer->AddStretchSpacer();
    SetSizer(outer_sizer);

    // Events
    m_save_url_btn->Bind(wxEVT_BUTTON, &AdditvPanel::on_save_url, this);
    m_login_btn->Bind(wxEVT_BUTTON,    &AdditvPanel::on_login, this);
    m_logout_btn->Bind(wxEVT_BUTTON,   &AdditvPanel::on_logout, this);
    m_send_btn->Bind(wxEVT_BUTTON,     &AdditvPanel::on_send, this);

    // Initial state
    update_login_state();
}

// ---------------------------------------------------------------------------
// State management
// ---------------------------------------------------------------------------

void AdditvPanel::update_login_state()
{
    bool logged_in = AdditvConfig::is_logged_in();
    std::string email = AdditvConfig::get_user_email();

    if (logged_in && !email.empty())
        m_status_label->SetLabel(wxString::Format("Connected as %s", email));
    else if (logged_in)
        m_status_label->SetLabel(_L("Connected"));
    else
        m_status_label->SetLabel(_L("Not connected"));

    m_login_btn->Show(!logged_in);
    m_logout_btn->Show(logged_in);
    enable_form(logged_in);

    if (logged_in)
        populate_dropdowns();

    Layout();
}

void AdditvPanel::enable_form(bool enable)
{
    m_filament_choice->Enable(enable);
    m_quantity_spin->Enable(enable);
    m_order_choice->Enable(enable);
    m_send_btn->Enable(enable && !m_gcode_path.empty());
}

void AdditvPanel::populate_dropdowns()
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

void AdditvPanel::on_save_url(wxCommandEvent & /*evt*/)
{
    std::string url = m_url_input->GetValue().ToStdString();
    std::string client_id = m_client_id_input->GetValue().ToStdString();

    if (url.empty()) {
        wxMessageBox(_L("Please enter a server URL."),
                     _L("Additv"), wxOK | wxICON_WARNING, this);
        return;
    }
    // Normalize: ensure https:// prefix
    if (url.find("://") == std::string::npos)
        url = "https://" + url;
    // Remove trailing slash
    if (!url.empty() && url.back() == '/')
        url.pop_back();

    AdditvConfig::set_server_url(url);
    AdditvConfig::set_client_id(client_id);
    m_url_input->SetValue(url);

    wxMessageBox(_L("Settings saved."),
                 _L("Additv"), wxOK | wxICON_INFORMATION, this);
}

void AdditvPanel::on_login(wxCommandEvent & /*evt*/)
{
    m_login_btn->Disable();
    m_status_label->SetLabel(_L("Opening browser for login..."));
    Layout();

    std::thread([this]() {
        auto result = AdditvOAuth::login();

        wxTheApp->CallAfter([this, result]() {
            if (result.success) {
                AdditvConfig::set_access_token(result.access_token);
                AdditvConfig::set_refresh_token(result.refresh_token);

                UserInfo user;
                std::string err;
                if (AdditvClient::get_me(user, err))
                    AdditvConfig::set_user_email(user.email);
            } else {
                wxMessageBox(
                    wxString::Format(_L("Login failed: %s"), result.error),
                    _L("Additv Login"), wxOK | wxICON_ERROR, this);
            }
            m_login_btn->Enable();
            update_login_state();
        });
    }).detach();
}

void AdditvPanel::on_logout(wxCommandEvent & /*evt*/)
{
    AdditvOAuth::logout();
    update_login_state();
}

void AdditvPanel::on_send(wxCommandEvent & /*evt*/)
{
    if (m_gcode_path.empty()) return;

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

    enable_form(false);
    m_send_btn->Disable();
    m_progress_bar->Show();
    m_progress_label->Show();
    m_progress_label->SetLabel(_L("Uploading..."));
    m_progress_bar->SetValue(0);
    Layout();

    std::thread([this, filament_id, filament_type, quantity, order_id]() {
        std::string  error;
        UploadResult upload_result;

        bool ok = AdditvClient::upload_gcode(
            m_gcode_path, "", m_printer_model, filament_type, m_estimated_time,
            upload_result, error,
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
