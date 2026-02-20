#ifndef slic3r_AdditvOAuth_hpp_
#define slic3r_AdditvOAuth_hpp_

#include <string>

namespace Slic3r { namespace GUI { namespace Additv {

class AdditvOAuth {
public:
    struct LoginResult {
        bool        success{false};
        std::string access_token;
        std::string refresh_token;
        std::string error;
    };

    // Full OAuth 2.0 PKCE flow — blocks until login completes or times out.
    // 1. Generate PKCE verifier + challenge
    // 2. Open system browser to Supabase auth URL
    // 3. Listen on localhost for the callback
    // 4. Exchange code for tokens
    // Tokens are NOT saved to config — caller must do that.
    static LoginResult login(int timeout_seconds = 120);

    // Clear stored tokens
    static void logout();

private:
    // PKCE helpers
    static std::string generate_random_string(int length);
    static std::string sha256_base64url(const std::string &input);
    static std::string base64url_encode(const unsigned char *data, size_t len);

    // Platform-native SHA-256 (32 bytes output)
    static bool sha256(const std::string &input,
                       unsigned char      output[32]);

    // Open URL in default browser
    static void open_browser(const std::string &url);

    // Temporary localhost server for OAuth callback
    struct CallbackResult {
        bool        received{false};
        std::string code;
        std::string state;
    };
    static CallbackResult wait_for_callback(int                port,
                                            const std::string &expected_state,
                                            int                timeout_seconds);

    // Exchange auth code + verifier for tokens
    static LoginResult exchange_code(const std::string &code,
                                     const std::string &code_verifier,
                                     const std::string &redirect_uri);
};

}}} // namespace Slic3r::GUI::Additv

#endif // slic3r_AdditvOAuth_hpp_
