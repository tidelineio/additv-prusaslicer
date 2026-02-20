#ifndef slic3r_AdditvConfig_hpp_
#define slic3r_AdditvConfig_hpp_

#include <string>

namespace Slic3r { namespace GUI { namespace Additv {

// Persists Additv settings in PrusaSlicer's AppConfig under the [additv] section.
class AdditvConfig {
public:
    // Server URL (Supabase project URL)
    static std::string get_server_url();
    static void        set_server_url(const std::string &url);

    // OAuth client ID (registered in Supabase OAuth Applications)
    static std::string get_client_id();
    static void        set_client_id(const std::string &id);

    // OAuth client secret
    static std::string get_client_secret();
    static void        set_client_secret(const std::string &secret);

    // OAuth tokens
    static std::string get_access_token();
    static void        set_access_token(const std::string &token);
    static std::string get_refresh_token();
    static void        set_refresh_token(const std::string &token);

    // Cached user info
    static std::string get_user_email();
    static void        set_user_email(const std::string &email);

    // Convenience
    static bool is_logged_in();
    static void clear_auth();

private:
    static constexpr const char *SECTION    = "additv";
    static constexpr const char *KEY_URL    = "server_url";
    static constexpr const char *KEY_CLIENT_ID = "client_id";
    static constexpr const char *DEFAULT_CLIENT_ID =
        "dd1952be-4737-4761-90c1-cb8a40c7292d";
    static constexpr const char *KEY_CLIENT_SECRET = "client_secret";
    static constexpr const char *DEFAULT_CLIENT_SECRET =
        "CY3RqfAIDU-34k4WBamH4H8-8UUloUfXW5za1B4Bej8";
    static constexpr const char *KEY_ACCESS = "access_token";
    static constexpr const char *KEY_REFRESH = "refresh_token";
    static constexpr const char *KEY_EMAIL  = "user_email";

    static constexpr const char *DEFAULT_URL =
        "https://wqpwnlwdxrymqqvzvwdp.supabase.co";
};

}}} // namespace Slic3r::GUI::Additv

#endif // slic3r_AdditvConfig_hpp_
