#ifndef slic3r_AdditvPanel_hpp_
#define slic3r_AdditvPanel_hpp_

#include "AdditvClient.hpp"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/gauge.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Additv {

// Panel that lives as a tab in PrusaSlicer's top bar (like Printables).
// Provides login, gcode upload, and job creation for the Additv farm.
class AdditvPanel : public wxPanel {
public:
    explicit AdditvPanel(wxWindow *parent);

    // Called before sending — set the gcode file info
    void set_gcode_path(const std::string &path);
    void set_filament_type_hint(const std::string &type);
    void set_printer_model(const std::string &model);
    void set_estimated_time(int seconds);

    // Refresh UI state (call when tab becomes active)
    void on_activate();

private:
    void build_ui();
    void update_login_state();
    void populate_dropdowns();
    void enable_form(bool enable);

    void on_login(wxCommandEvent &evt);
    void on_logout(wxCommandEvent &evt);
    void on_send(wxCommandEvent &evt);
    void on_save_url(wxCommandEvent &evt);

    static std::string format_time(int seconds);
    static std::string format_size(uintmax_t bytes);

    // Server config
    wxTextCtrl   *m_url_input{nullptr};
    wxTextCtrl   *m_client_id_input{nullptr};
    wxButton     *m_save_url_btn{nullptr};

    // Connection
    wxStaticText *m_status_label{nullptr};
    wxButton     *m_login_btn{nullptr};
    wxButton     *m_logout_btn{nullptr};

    // GCode info
    wxStaticText *m_file_label{nullptr};
    wxStaticText *m_size_label{nullptr};
    wxStaticText *m_time_label{nullptr};

    // Job config
    wxChoice   *m_filament_choice{nullptr};
    wxSpinCtrl *m_quantity_spin{nullptr};
    wxChoice   *m_order_choice{nullptr};

    // Actions
    wxButton *m_send_btn{nullptr};

    // Progress
    wxGauge      *m_progress_bar{nullptr};
    wxStaticText *m_progress_label{nullptr};

    // Data
    std::string               m_gcode_path;
    std::string               m_filament_type_hint;
    std::string               m_printer_model;
    int                       m_estimated_time{0};
    std::vector<FilamentInfo> m_filaments;
    std::vector<OrderInfo>    m_orders;
};

}}} // namespace Slic3r::GUI::Additv

#endif // slic3r_AdditvPanel_hpp_
