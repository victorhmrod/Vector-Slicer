#ifndef slic3r_FlashforgeLan_hpp_
#define slic3r_FlashforgeLan_hpp_

#include <string>
#include <wx/string.h>
#include "PrintHost.hpp"

namespace Slic3r {

class DynamicPrintConfig;
class Http;

// LAN print host for FlashForge Adventurer 5M / 5M Pro (and compatible models)
// running the stock firmware. Talks to the printer's local HTTP API on port
// 8898, authenticated by the machine serial number + "CheckCode" shown on the
// printer screen when LAN mode is enabled. The serial number can be
// auto-discovered through the legacy TCP console on port 8899 (M601/M115).
//
// Protocol reference: https://github.com/Parallel-7/flashforge-api-docs
// (endpoints_5m_3.2.7.yaml), cross-checked against the Orca-Flashforge fork.
// This is an independent implementation; no FlashForge binaries are used.
class FlashforgeLan : public PrintHost
{
public:
    explicit FlashforgeLan(DynamicPrintConfig *config);
    ~FlashforgeLan() override = default;

    const char *get_name() const override;

    bool                       test(wxString &curl_msg) const override;
    wxString                   get_test_ok_msg() const override;
    wxString                   get_test_failed_msg(wxString &msg) const override;
    bool                       upload(PrintHostUpload upload_data, ProgressFn progress_fn, ErrorFn error_fn, InfoFn info_fn) const override;
    bool                       has_auto_discovery() const override { return false; }
    bool                       can_test() const override { return true; }
    PrintHostPostUploadActions get_post_upload_actions() const override { return PrintHostPostUploadAction::StartPrint; }
    PrintHostCapabilities      get_capabilities() const override
    {
        PrintHostCapabilities caps;
        caps.can_upload       = true;
        caps.can_start_print  = true;
        caps.can_query_status = true;
        return caps;
    }
    std::string get_host() const override { return m_host; }

    // Validates host/serial/check-code syntax before any network I/O.
    // Returns an empty string when the configuration is usable.
    static wxString validate_config(const std::string &host, const std::string &serial, const std::string &check_code);

    // Strips any directory components and characters the printer's /data
    // storage cannot digest. Kept public (and static) for unit testing.
    static std::string sanitize_remote_filename(const std::string &filename);

    // Parses the {code, message} envelope every HTTP endpoint answers with.
    // Returns false when the body is not valid JSON. Public for unit testing.
    static bool parse_code_message(const std::string &body, int &code, std::string &message);

private:
    std::string m_host;        // IP or hostname, optional ":port" (default 8898)
    std::string m_serial;      // printer serial number ("printhost_user")
    std::string m_check_code;  // LAN-mode check code ("printhost_apikey")

    std::string make_url(const std::string &path) const;
    std::string auth_body() const;
    // Fetch the serial number over the legacy TCP console (port 8899, ~M115).
    // Returns empty string on failure; `msg` then carries the reason.
    std::string discover_serial(wxString &msg) const;
    // POST /detail and validate the code/message envelope.
    bool query_detail(wxString &msg, std::string *printer_name, std::string *status, std::string &serial_in_use) const;
};

} // namespace Slic3r

#endif
