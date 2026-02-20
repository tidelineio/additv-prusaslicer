#ifndef slic3r_AdditvDialog_hpp_
#define slic3r_AdditvDialog_hpp_

#include "AdditvClient.hpp"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/gauge.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Additv {

class AdditvDialog : public wxDialog {
public:
    explicit AdditvDialog(wxWindow *parent);

    // Call these before ShowModal() to pre-populate the dialog.
    void set_gcode_path(const std::string &path);
    void set_gcode_name(const std::string &name);
    void set_filament_type_hint(const std::string &type);
    void set_printer_model(const std::string &model);
    void set_estimated_time(int seconds);

private:
    void build_ui();
    void populate_dropdowns();
    void enable_form(bool enable);

    void on_send(wxCommandEvent &evt);

    static std::string format_time(int seconds);
    static std::string format_size(uintmax_t bytes);

    // GCode info
    wxTextCtrl   *m_name_input{nullptr};
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

#endif // slic3r_AdditvDialog_hpp_
