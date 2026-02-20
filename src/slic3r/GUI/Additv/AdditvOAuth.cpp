#include "AdditvOAuth.hpp"
#include "AdditvConfig.hpp"
#include "slic3r/Utils/Http.hpp"

#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <chrono>
#include <random>
#include <sstream>

// Platform-native SHA-256
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#elif _WIN32
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/sha.h>
#endif

// Platform-native browser open
#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#elif __APPLE__
#include <cstdlib>
#else
#include <cstdlib>
#endif

namespace Slic3r { namespace GUI { namespace Additv {

namespace pt  = boost::property_tree;
using boost::asio::ip::tcp;

// ---------------------------------------------------------------------------
// Crypto helpers
// ---------------------------------------------------------------------------

bool AdditvOAuth::sha256(const std::string &input,
                          unsigned char      output[32])
{
#ifdef __APPLE__
    CC_SHA256(input.data(), static_cast<CC_LONG>(input.size()), output);
    return true;
#elif _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, 0) != 0)
        return false;

    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    BCryptHashData(hHash,
                   reinterpret_cast<PUCHAR>(const_cast<char *>(input.data())),
                   static_cast<ULONG>(input.size()), 0);
    BCryptFinishHash(hHash, output, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return true;
#else
    SHA256(reinterpret_cast<const unsigned char *>(input.data()),
           input.size(), output);
    return true;
#endif
}

std::string AdditvOAuth::base64url_encode(const unsigned char *data,
                                           size_t               len)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(4 * ((len + 2) / 3));

    for (size_t i = 0; i < len; i += 3) {
        unsigned n = static_cast<unsigned>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<unsigned>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<unsigned>(data[i + 2]);

        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? table[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? table[n & 0x3F] : '=';
    }

    // base64 → base64url: + → -, / → _, strip trailing =
    for (auto &c : result) {
        if (c == '+')      c = '-';
        else if (c == '/') c = '_';
    }
    while (!result.empty() && result.back() == '=')
        result.pop_back();

    return result;
}

std::string AdditvOAuth::sha256_base64url(const std::string &input)
{
    unsigned char hash[32];
    if (!sha256(input, hash))
        return {};
    return base64url_encode(hash, 32);
}

std::string AdditvOAuth::generate_random_string(int length)
{
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    std::random_device                 rd;
    std::mt19937                       gen(rd());
    std::uniform_int_distribution<int> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i)
        result += charset[dist(gen)];
    return result;
}

// ---------------------------------------------------------------------------
// Browser
// ---------------------------------------------------------------------------

void AdditvOAuth::open_browser(const std::string &url)
{
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    std::string cmd = "open '" + url + "'";
    ::system(cmd.c_str());
#else
    std::string cmd = "xdg-open '" + url + "'";
    ::system(cmd.c_str());
#endif
}

// ---------------------------------------------------------------------------
// Localhost callback server
// ---------------------------------------------------------------------------

AdditvOAuth::CallbackResult
AdditvOAuth::wait_for_callback(int                port,
                                const std::string &expected_state,
                                int                timeout_seconds)
{
    CallbackResult result;

    try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), port));
        acceptor.set_option(boost::asio::socket_base::reuse_address(true));

        // Timeout timer
        boost::asio::steady_timer timer(io);
        timer.expires_after(std::chrono::seconds(timeout_seconds));
        timer.async_wait([&](const boost::system::error_code &) {
            acceptor.close();
        });

        tcp::socket socket(io);
        acceptor.async_accept(socket, [&](const boost::system::error_code &ec) {
            if (ec) return;

            // Read the HTTP request
            boost::asio::streambuf buf;
            boost::asio::read_until(socket, buf, "\r\n\r\n");
            std::string req(boost::asio::buffer_cast<const char *>(buf.data()),
                            buf.size());

            // Parse "GET /callback?code=xxx&state=yyy HTTP/1.1"
            auto qs_start = req.find('?');
            auto qs_end   = req.find(' ', qs_start);
            if (qs_start != std::string::npos && qs_end != std::string::npos) {
                std::string        qs = req.substr(qs_start + 1, qs_end - qs_start - 1);
                std::istringstream ss(qs);
                std::string        param;
                while (std::getline(ss, param, '&')) {
                    auto eq = param.find('=');
                    if (eq == std::string::npos) continue;
                    std::string key = param.substr(0, eq);
                    std::string val = param.substr(eq + 1);
                    if (key == "code")  result.code  = val;
                    if (key == "state") result.state = val;
                }
            }

            // Respond with a success page
            const char *html =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<html><body>"
                "<h2>Login successful!</h2>"
                "<p>You can close this tab and return to PrusaSlicer.</p>"
                "<script>window.close();</script>"
                "</body></html>";
            boost::asio::write(socket, boost::asio::buffer(html, std::strlen(html)));
            socket.close();
            timer.cancel();
        });

        io.run();

        if (!result.code.empty() && result.state == expected_state)
            result.received = true;
    } catch (...) {
        // Server failed or timed out
    }

    return result;
}

// ---------------------------------------------------------------------------
// Token exchange
// ---------------------------------------------------------------------------

AdditvOAuth::LoginResult
AdditvOAuth::exchange_code(const std::string &code,
                            const std::string &code_verifier,
                            const std::string &redirect_uri)
{
    LoginResult lr;

    std::string base = AdditvConfig::get_server_url();
    if (!base.empty() && base.back() == '/')
        base.pop_back();
    std::string url = base + "/auth/v1/oauth/token";
    std::string client_id     = AdditvConfig::get_client_id();
    std::string client_secret = AdditvConfig::get_client_secret();

    // Standard OAuth 2.1 token exchange (form-encoded, not JSON)
    std::string body_str =
        "grant_type=authorization_code"
        "&code="          + Http::url_encode(code) +
        "&redirect_uri="  + Http::url_encode(redirect_uri) +
        "&code_verifier=" + Http::url_encode(code_verifier) +
        "&client_id="     + client_id;

    std::string resp_body;
    unsigned    resp_status = 0;

    Http::post(url)
        .header("Content-Type", "application/x-www-form-urlencoded")
        .auth_basic(client_id, client_secret)
        .set_post_body(body_str)
        .on_complete([&](std::string b, unsigned status) {
            resp_body   = std::move(b);
            resp_status = status;
        })
        .on_error([&](std::string b, std::string err, unsigned status) {
            resp_body   = std::move(b);
            resp_status = status;
            lr.error    = std::move(err);
        })
        .perform_sync();

    if (resp_status != 200) {
        if (lr.error.empty())
            lr.error = "Token exchange failed (status " +
                       std::to_string(resp_status) + ")";
        return lr;
    }

    try {
        pt::ptree tree;
        std::istringstream ss(resp_body);
        pt::read_json(ss, tree);

        lr.access_token  = tree.get<std::string>("access_token", "");
        lr.refresh_token = tree.get<std::string>("refresh_token", "");
        lr.success       = !lr.access_token.empty();
        if (!lr.success)
            lr.error = "No access token in response";
    } catch (const std::exception &e) {
        lr.error = std::string("Failed to parse token response: ") + e.what();
    }

    return lr;
}

// ---------------------------------------------------------------------------
// Main login flow
// ---------------------------------------------------------------------------

AdditvOAuth::LoginResult AdditvOAuth::login(int timeout_seconds)
{
    // Step 1: Generate PKCE verifier and challenge
    std::string code_verifier  = generate_random_string(64);
    std::string code_challenge = sha256_base64url(code_verifier);
    std::string state          = generate_random_string(32);

    if (code_challenge.empty())
        return {false, {}, {}, "Failed to compute PKCE challenge"};

    // Step 2: Fixed port — must match the redirect_uri registered in Supabase
    constexpr int port = 19284;

    std::string redirect_uri = "http://localhost:" + std::to_string(port);

    // Step 3: Build auth URL (Supabase OAuth 2.1 endpoint)
    std::string base = AdditvConfig::get_server_url();
    if (!base.empty() && base.back() == '/')
        base.pop_back();
    std::string client_id = AdditvConfig::get_client_id();

    std::string auth_url =
        base + "/auth/v1/oauth/authorize"
        "?response_type=code"
        "&client_id="              + client_id +
        "&redirect_uri="           + Http::url_encode(redirect_uri) +
        "&scope="                  + Http::url_encode("email profile") +
        "&code_challenge="         + code_challenge +
        "&code_challenge_method=S256"
        "&state="                  + state;

    // Step 4: Open browser and wait for callback
    open_browser(auth_url);
    CallbackResult cb = wait_for_callback(port, state, timeout_seconds);

    if (!cb.received)
        return {false, {}, {}, "Login timed out or was cancelled"};

    // Step 5: Exchange code for tokens
    return exchange_code(cb.code, code_verifier, redirect_uri);
}

void AdditvOAuth::logout()
{
    AdditvConfig::clear_auth();
}

}}} // namespace Slic3r::GUI::Additv
