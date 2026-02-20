#ifndef slic3r_AdditvClient_hpp_
#define slic3r_AdditvClient_hpp_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Additv {

struct FilamentInfo {
    int64_t     id;
    std::string plastic_type;
    std::string color_name;
    std::string color_hex;
    std::string manufacturer;
    std::string manufacturer_sku;
};

struct OrderInfo {
    int64_t     id;
    std::string description;
    std::string shopify_order_number;
    std::string status;
};

struct UploadResult {
    int64_t     gcode_id;
    std::string storage_path;
};

struct JobsResult {
    std::vector<int64_t> job_ids;
    int                  count;
};

struct UserInfo {
    std::string email;
    std::string display_name;
};

// Synchronous HTTP client for the slicer-api edge function.
// Uses PrusaSlicer's Http class (libcurl wrapper) internally.
// All methods block until the request completes.
class AdditvClient {
public:
    // User info
    static bool get_me(UserInfo &out, std::string &error);

    // Dropdown data
    static bool get_filaments(std::vector<FilamentInfo> &out,
                              const std::string         &type_filter,
                              std::string               &error);
    static bool get_orders(std::vector<OrderInfo> &out,
                           std::string            &error);

    // Upload gcode file (with optional progress callback)
    // upload_name: filename to use on the server (defaults to local filename if empty)
    static bool upload_gcode(const std::string              &file_path,
                             const std::string              &upload_name,
                             const std::string              &printer_model,
                             const std::string              &filament_type,
                             int                             estimated_time_seconds,
                             UploadResult                   &out,
                             std::string                    &error,
                             std::function<void(float)>      progress_fn = nullptr);

    // Create print jobs (quantity = number of independent jobs to create)
    static bool create_jobs(int64_t            gcode_id,
                            int64_t            filament_id,
                            int                quantity,
                            int64_t            order_id, // 0 = no order
                            JobsResult        &out,
                            std::string       &error);

    // Refresh access token using stored refresh token
    static bool refresh_access_token(std::string &error);

private:
    static std::string build_url(const std::string &path);
    static std::string auth_header();
};

}}} // namespace Slic3r::GUI::Additv

#endif // slic3r_AdditvClient_hpp_
