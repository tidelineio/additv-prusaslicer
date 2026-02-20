#include "AdditvConfig.hpp"
#include "libslic3r/AppConfig.hpp"
#include "slic3r/GUI/GUI_App.hpp"

namespace Slic3r { namespace GUI { namespace Additv {

static AppConfig &app_cfg()
{
    return *wxGetApp().app_config;
}

std::string AdditvConfig::get_server_url()
{
    std::string val = app_cfg().get(SECTION, KEY_URL);
    return val.empty() ? DEFAULT_URL : val;
}

void AdditvConfig::set_server_url(const std::string &url)
{
    app_cfg().set(SECTION, KEY_URL, url);
}

std::string AdditvConfig::get_access_token()
{
    return app_cfg().get(SECTION, KEY_ACCESS);
}

void AdditvConfig::set_access_token(const std::string &token)
{
    app_cfg().set(SECTION, KEY_ACCESS, token);
}

std::string AdditvConfig::get_refresh_token()
{
    return app_cfg().get(SECTION, KEY_REFRESH);
}

void AdditvConfig::set_refresh_token(const std::string &token)
{
    app_cfg().set(SECTION, KEY_REFRESH, token);
}

std::string AdditvConfig::get_user_email()
{
    return app_cfg().get(SECTION, KEY_EMAIL);
}

void AdditvConfig::set_user_email(const std::string &email)
{
    app_cfg().set(SECTION, KEY_EMAIL, email);
}

bool AdditvConfig::is_logged_in()
{
    return !get_access_token().empty();
}

void AdditvConfig::clear_auth()
{
    set_access_token("");
    set_refresh_token("");
    set_user_email("");
}

}}} // namespace Slic3r::GUI::Additv
