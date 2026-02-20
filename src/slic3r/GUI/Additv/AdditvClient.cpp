#include "AdditvClient.hpp"
#include "AdditvConfig.hpp"
#include "slic3r/Utils/Http.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <sstream>

namespace Slic3r { namespace GUI { namespace Additv {

namespace pt = boost::property_tree;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string AdditvClient::build_url(const std::string &path)
{
    std::string base = AdditvConfig::get_server_url();
    if (!base.empty() && base.back() == '/')
        base.pop_back();
    return base + "/functions/v1/slicer-api" + path;
}

std::string AdditvClient::auth_header()
{
    return "Bearer " + AdditvConfig::get_access_token();
}

static bool parse_json(const std::string &body,
                       pt::ptree         &tree,
                       std::string       &error)
{
    try {
        std::istringstream ss(body);
        pt::read_json(ss, tree);
        return true;
    } catch (const std::exception &e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

// Attempt token refresh. Returns true if caller should retry the request.
static bool try_refresh_on_401(unsigned status, std::string &error)
{
    if (status != 401) return false;
    if (AdditvClient::refresh_access_token(error))
        return true; // caller retries
    error = "Authentication expired. Please log in again.";
    return false;
}

// ---------------------------------------------------------------------------
// Token refresh
// ---------------------------------------------------------------------------

bool AdditvClient::refresh_access_token(std::string &error)
{
    std::string refresh = AdditvConfig::get_refresh_token();
    if (refresh.empty()) {
        error = "No refresh token available";
        return false;
    }

    std::string base = AdditvConfig::get_server_url();
    if (!base.empty() && base.back() == '/')
        base.pop_back();
    std::string url = base + "/auth/v1/token?grant_type=refresh_token";

    std::string body_str = "{\"refresh_token\":\"" + refresh + "\"}";

    std::string  resp_body;
    unsigned     resp_status = 0;

    Http::post(url)
        .header("Content-Type", "application/json")
        .set_post_body(body_str)
        .on_complete([&](std::string body, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
        })
        .on_error([&](std::string body, std::string err, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
            error       = std::move(err);
        })
        .perform_sync();

    if (resp_status != 200) {
        if (error.empty())
            error = "Token refresh failed (status " +
                    std::to_string(resp_status) + ")";
        return false;
    }

    pt::ptree tree;
    if (!parse_json(resp_body, tree, error)) return false;

    auto access     = tree.get_optional<std::string>("access_token");
    auto new_refresh = tree.get_optional<std::string>("refresh_token");
    if (!access) {
        error = "No access token in refresh response";
        return false;
    }

    AdditvConfig::set_access_token(*access);
    if (new_refresh)
        AdditvConfig::set_refresh_token(*new_refresh);
    return true;
}

// ---------------------------------------------------------------------------
// GET /me
// ---------------------------------------------------------------------------

bool AdditvClient::get_me(UserInfo &out, std::string &error)
{
    std::string  resp_body;
    unsigned     resp_status = 0;

    Http::get(build_url("/me"))
        .header("Authorization", auth_header())
        .on_complete([&](std::string body, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
        })
        .on_error([&](std::string body, std::string err, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
            error       = std::move(err);
        })
        .perform_sync();

    if (try_refresh_on_401(resp_status, error))
        return get_me(out, error);

    if (resp_status != 200) {
        if (error.empty())
            error = "Server returned status " + std::to_string(resp_status);
        return false;
    }

    pt::ptree tree;
    if (!parse_json(resp_body, tree, error)) return false;

    out.email        = tree.get<std::string>("email", "");
    out.display_name = tree.get<std::string>("display_name", "");
    return true;
}

// ---------------------------------------------------------------------------
// GET /filaments
// ---------------------------------------------------------------------------

bool AdditvClient::get_filaments(std::vector<FilamentInfo> &out,
                                  const std::string         &type_filter,
                                  std::string               &error)
{
    std::string url = build_url("/filaments");
    if (!type_filter.empty())
        url += "?type=" + Http::url_encode(type_filter);

    std::string  resp_body;
    unsigned     resp_status = 0;

    Http::get(url)
        .header("Authorization", auth_header())
        .on_complete([&](std::string body, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
        })
        .on_error([&](std::string body, std::string err, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
            error       = std::move(err);
        })
        .perform_sync();

    if (try_refresh_on_401(resp_status, error))
        return get_filaments(out, type_filter, error);

    if (resp_status != 200) {
        if (error.empty())
            error = "Server returned status " + std::to_string(resp_status);
        return false;
    }

    pt::ptree tree;
    if (!parse_json(resp_body, tree, error)) return false;

    out.clear();
    for (const auto &item : tree.get_child("filaments")) {
        FilamentInfo f;
        f.id               = item.second.get<int64_t>("id", 0);
        f.plastic_type     = item.second.get<std::string>("plastic_type", "");
        f.color_name       = item.second.get<std::string>("color_name", "");
        f.color_hex        = item.second.get<std::string>("color_hex", "");
        f.manufacturer     = item.second.get<std::string>("manufacturer", "");
        f.manufacturer_sku = item.second.get<std::string>("manufacturer_sku", "");
        out.push_back(std::move(f));
    }
    return true;
}

// ---------------------------------------------------------------------------
// GET /orders
// ---------------------------------------------------------------------------

bool AdditvClient::get_orders(std::vector<OrderInfo> &out,
                               std::string            &error)
{
    std::string  resp_body;
    unsigned     resp_status = 0;

    Http::get(build_url("/orders"))
        .header("Authorization", auth_header())
        .on_complete([&](std::string body, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
        })
        .on_error([&](std::string body, std::string err, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
            error       = std::move(err);
        })
        .perform_sync();

    if (try_refresh_on_401(resp_status, error))
        return get_orders(out, error);

    if (resp_status != 200) {
        if (error.empty())
            error = "Server returned status " + std::to_string(resp_status);
        return false;
    }

    pt::ptree tree;
    if (!parse_json(resp_body, tree, error)) return false;

    out.clear();
    for (const auto &item : tree.get_child("orders")) {
        OrderInfo o;
        o.id                    = item.second.get<int64_t>("id", 0);
        o.description           = item.second.get<std::string>("description", "");
        o.shopify_order_number  = item.second.get<std::string>("shopify_order_number", "");
        o.status                = item.second.get<std::string>("status", "");
        out.push_back(std::move(o));
    }
    return true;
}

// ---------------------------------------------------------------------------
// POST /upload
// ---------------------------------------------------------------------------

bool AdditvClient::upload_gcode(const std::string              &file_path,
                                 const std::string              &upload_name,
                                 const std::string              &printer_model,
                                 const std::string              &filament_type,
                                 int                             estimated_time_seconds,
                                 UploadResult                   &out,
                                 std::string                    &error,
                                 std::function<void(float)>      progress_fn)
{
    // Build metadata JSON
    std::ostringstream meta;
    meta << "{\"printer_model\":\"" << printer_model
         << "\",\"filament_type\":\"" << filament_type
         << "\",\"estimated_print_time_seconds\":" << estimated_time_seconds
         << "}";

    std::string  resp_body;
    unsigned     resp_status = 0;

    auto http = Http::post(build_url("/upload"));
    http.header("Authorization", auth_header());
    // Use custom upload name if provided, otherwise local filename
    if (!upload_name.empty())
        http.form_add_file("file", file_path, upload_name);
    else
        http.form_add_file("file", file_path);
    http.form_add("metadata", meta.str());

    if (progress_fn) {
        http.on_progress([&progress_fn](Http::Progress p, bool & /*cancel*/) {
            if (p.ultotal > 0)
                progress_fn(static_cast<float>(p.ulnow) /
                            static_cast<float>(p.ultotal));
        });
    }

    http.on_complete([&](std::string body, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
        })
        .on_error([&](std::string body, std::string err, unsigned status) {
            resp_body   = std::move(body);
            resp_status = status;
            error       = std::move(err);
        })
        .perform_sync();

    if (try_refresh_on_401(resp_status, error))
        return upload_gcode(file_path, upload_name, printer_model, filament_type,
                            estimated_time_seconds, out, error, progress_fn);

    if (resp_status != 201) {
        if (error.empty()) {
            pt::ptree tree;
            if (parse_json(resp_body, tree, error))
                error = tree.get<std::string>("error", "Upload failed");
            else
                error = "Upload failed (status " +
                        std::to_string(resp_status) + ")";
        }
        return false;
    }

    pt::ptree tree;
    if (!parse_json(resp_body, tree, error)) return false;

    out.gcode_id     = tree.get<int64_t>("gcode_id", 0);
    out.storage_path = tree.get<std::string>("storage_path", "");
    return true;
}

// ---------------------------------------------------------------------------
// POST /jobs
// ---------------------------------------------------------------------------

bool AdditvClient::create_jobs(int64_t       gcode_id,
                                int64_t       filament_id,
                                int           quantity,
                                int64_t       order_id,
                                JobsResult   &out,
                                std::string  &error)
{
    std::ostringstream body;
    body << "{\"gcode_id\":" << gcode_id
         << ",\"filament_id\":" << filament_id
         << ",\"quantity\":" << quantity;
    if (order_id > 0)
        body << ",\"order_id\":" << order_id;
    body << "}";

    std::string  resp_body;
    unsigned     resp_status = 0;

    Http::post(build_url("/jobs"))
        .header("Authorization", auth_header())
        .header("Content-Type", "application/json")
        .set_post_body(body.str())
        .on_complete([&](std::string b, unsigned status) {
            resp_body   = std::move(b);
            resp_status = status;
        })
        .on_error([&](std::string b, std::string err, unsigned status) {
            resp_body   = std::move(b);
            resp_status = status;
            error       = std::move(err);
        })
        .perform_sync();

    if (try_refresh_on_401(resp_status, error))
        return create_jobs(gcode_id, filament_id, quantity, order_id, out, error);

    if (resp_status != 201) {
        if (error.empty()) {
            pt::ptree tree;
            if (parse_json(resp_body, tree, error))
                error = tree.get<std::string>("error", "Job creation failed");
            else
                error = "Job creation failed (status " +
                        std::to_string(resp_status) + ")";
        }
        return false;
    }

    pt::ptree tree;
    if (!parse_json(resp_body, tree, error)) return false;

    out.count = tree.get<int>("count", 0);
    out.job_ids.clear();
    for (const auto &item : tree.get_child("job_ids"))
        out.job_ids.push_back(item.second.get_value<int64_t>());
    return true;
}

}}} // namespace Slic3r::GUI::Additv
