#include "FlashforgeLan.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "libslic3r/PrintConfig.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "Http.hpp"
#include "nlohmann/json.hpp"

namespace fs = boost::filesystem;
using json = nlohmann::json;

namespace Slic3r {

namespace {

const char *FF_LAN_NAME     = "FlashForge LAN";
const int   FF_HTTP_PORT    = 8898;
const char *FF_TCP_PORT     = "8899";
const long  FF_CONNECT_TIMEOUT = 5;   // seconds
const long  FF_REQUEST_TIMEOUT = 15;  // seconds, control requests only (not upload)

// Split "host[:port]" into host and port (default FF_HTTP_PORT).
void split_host_port(const std::string &in, std::string &host, int &port)
{
    host = in;
    port = FF_HTTP_PORT;
    // Do not confuse IPv6 literals with a port separator.
    const auto colon = in.rfind(':');
    if (colon != std::string::npos && in.find(':') == colon) {
        const std::string port_str = in.substr(colon + 1);
        if (!port_str.empty() && std::all_of(port_str.begin(), port_str.end(), [](unsigned char c) { return std::isdigit(c); })) {
            host = in.substr(0, colon);
            port = std::stoi(port_str);
        }
    }
}

} // namespace

FlashforgeLan::FlashforgeLan(DynamicPrintConfig *config)
    : m_host(config->opt_string("print_host"))
    , m_serial(config->opt_string("printhost_user"))
    , m_check_code(config->opt_string("printhost_apikey"))
{
    boost::trim(m_host);
    boost::trim(m_serial);
    boost::trim(m_check_code);
}

const char *FlashforgeLan::get_name() const { return FF_LAN_NAME; }

wxString FlashforgeLan::validate_config(const std::string &host, const std::string &serial, const std::string &check_code)
{
    std::string h = host;
    boost::trim(h);
    if (h.empty())
        return _L("The printer IP address or hostname is not set.");
    if (h.find("http://") == 0 || h.find("https://") == 0)
        h = h.substr(h.find("//") + 2);
    if (h.find('/') != std::string::npos)
        return _L("The printer address must be a plain IP or hostname, without a path.");
    std::string hostname;
    int         port = 0;
    split_host_port(h, hostname, port);
    if (hostname.empty())
        return _L("The printer IP address or hostname is not set.");
    if (port <= 0 || port > 65535)
        return _L("The port number is invalid.");
    std::string cc = check_code;
    boost::trim(cc);
    if (cc.empty())
        return _L("The CheckCode is not set. Enable LAN mode on the printer screen and enter the code shown there.");
    // The serial number may be left empty - it is then auto-discovered.
    (void) serial;
    return wxString();
}

std::string FlashforgeLan::make_url(const std::string &path) const
{
    std::string h = m_host;
    if (h.find("http://") == 0 || h.find("https://") == 0)
        h = h.substr(h.find("//") + 2);
    std::string hostname;
    int         port;
    split_host_port(h, hostname, port);
    return (boost::format("http://%1%:%2%%3%") % hostname % port % path).str();
}

std::string FlashforgeLan::auth_body() const
{
    json j;
    j["serialNumber"] = m_serial;
    j["checkCode"]    = m_check_code;
    return j.dump();
}

bool FlashforgeLan::parse_code_message(const std::string &body, int &code, std::string &message)
{
    try {
        const json j = json::parse(body);
        code    = j.value("code", -1);
        message = j.value("message", std::string());
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

// Query the machine serial number over the legacy TCP console (~M601/~M115).
// This lets the user configure only IP + CheckCode.
std::string FlashforgeLan::discover_serial(wxString &msg) const
{
    namespace asio = boost::asio;
    using asio::ip::tcp;

    std::string h = m_host;
    if (h.find("http://") == 0 || h.find("https://") == 0)
        h = h.substr(h.find("//") + 2);
    std::string hostname;
    int         unused_port;
    split_host_port(h, hostname, unused_port);

    try {
        asio::io_context io;
        tcp::resolver    resolver(io);
        auto             endpoints = resolver.resolve(hostname, FF_TCP_PORT);
        tcp::socket      socket(io);

        boost::system::error_code connect_ec = asio::error::would_block;
        asio::async_connect(socket, endpoints, [&](const boost::system::error_code &ec, const tcp::endpoint &) { connect_ec = ec; });
        io.restart();
        io.run_for(std::chrono::seconds(FF_CONNECT_TIMEOUT));
        if (connect_ec) {
            msg = _L("Connection timed out while querying the serial number. You can enter the serial number manually.");
            return {};
        }

        char buffer[1024];
        auto exchange = [&](const std::string &cmd, std::string &response) -> bool {
            boost::system::error_code write_ec;
            asio::write(socket, asio::buffer(cmd), write_ec);
            if (write_ec)
                return false;
            response.clear();
            bool                            done = false;
            std::function<void()>           do_read;
            do_read = [&]() {
                socket.async_read_some(asio::buffer(buffer), [&](const boost::system::error_code &ec, std::size_t n) {
                    if (ec)
                        return;
                    response.append(buffer, n);
                    if (response.find("ok") != std::string::npos) {
                        done = true;
                        return;
                    }
                    do_read();
                });
            };
            io.restart();
            do_read();
            io.run_for(std::chrono::seconds(FF_CONNECT_TIMEOUT));
            if (!done)
                socket.cancel(write_ec);
            return done;
        };

        std::string response;
        if (!exchange("~M601 S1\r\n", response)) {
            msg = _L("The printer did not accept the control connection on port 8899.");
            return {};
        }
        if (!exchange("~M115\r\n", response)) {
            msg = _L("The printer did not answer the machine-information query (M115).");
            return {};
        }
        // Best effort logout; ignore the result.
        std::string ignored;
        exchange("~M602\r\n", ignored);

        std::istringstream iss(response);
        std::string        line;
        while (std::getline(iss, line)) {
            const auto pos = line.find("SN:");
            if (pos != std::string::npos) {
                std::string sn = line.substr(pos + 3);
                boost::trim(sn);
                if (!sn.empty()) {
                    BOOST_LOG_TRIVIAL(info) << FF_LAN_NAME << ": discovered serial number over TCP console";
                    return sn;
                }
            }
        }
        msg = _L("Could not find the serial number in the printer's M115 reply. Please enter it manually.");
    } catch (const std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << FF_LAN_NAME << ": serial discovery failed: " << e.what();
        msg = GUI::format_wxstr(_L("Serial number discovery failed: %1%"), e.what());
    }
    return {};
}

bool FlashforgeLan::query_detail(wxString &msg, std::string *printer_name, std::string *status, std::string &serial_in_use) const
{
    const wxString cfg_error = validate_config(m_host, m_serial, m_check_code);
    if (!cfg_error.empty()) {
        msg = cfg_error;
        return false;
    }

    serial_in_use = m_serial;
    if (serial_in_use.empty()) {
        serial_in_use = discover_serial(msg);
        if (serial_in_use.empty())
            return false;
    }

    json body_json;
    body_json["serialNumber"] = serial_in_use;
    body_json["checkCode"]    = m_check_code;

    const std::string url = make_url("/detail");
    BOOST_LOG_TRIVIAL(info) << FF_LAN_NAME << ": testing connection at " << url;

    bool res = false;
    auto http = Http::post(url);
    http.header("Content-Type", "application/json")
        .set_post_body(body_json.dump())
        .timeout_connect(FF_CONNECT_TIMEOUT)
        .timeout_max(FF_REQUEST_TIMEOUT)
        .on_error([&](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: /detail failed: %2%, HTTP %3%") % FF_LAN_NAME % error % http_status;
            msg = format_error(body, error, http_status);
        })
        .on_complete([&](std::string body, unsigned) {
            int         code = -1;
            std::string message;
            if (!parse_code_message(body, code, message)) {
                msg = _L("The printer sent an invalid response. Check that the address points to a FlashForge printer in LAN mode.");
                return;
            }
            if (code != 0) {
                if (boost::icontains(message, "check") || boost::icontains(message, "auth"))
                    msg = _L("Authentication failed. Check the CheckCode shown on the printer screen (LAN mode).");
                else
                    msg = GUI::format_wxstr(_L("The printer refused the request: %1% (code %2%)"), message, code);
                return;
            }
            try {
                const json j = json::parse(body);
                if (j.contains("detail")) {
                    if (printer_name != nullptr)
                        *printer_name = j["detail"].value("name", std::string());
                    if (status != nullptr)
                        *status = j["detail"].value("status", std::string());
                }
            } catch (const std::exception &) {
                // The code/message envelope was already validated; details are optional.
            }
            res = true;
        })
        .perform_sync();

    return res;
}

bool FlashforgeLan::test(wxString &msg) const
{
    std::string name, status, serial;
    return query_detail(msg, &name, &status, serial);
}

wxString FlashforgeLan::get_test_ok_msg() const
{
    return _L("Connection to the FlashForge printer works correctly.");
}

wxString FlashforgeLan::get_test_failed_msg(wxString &msg) const
{
    return GUI::format_wxstr("%1%: %2%", _L("Could not connect to the FlashForge printer"), msg);
}

std::string FlashforgeLan::sanitize_remote_filename(const std::string &filename)
{
    // Keep only the basename - the printer stores uploads flat under /data.
    std::string name = fs::path(filename).filename().string();
    // Drop any character that could upset the firmware's storage layer.
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) != 0 ||
                        c == '.' || c == '_' || c == '-' || c == '+' || c == '(' || c == ')' || c == ' ' || c == '#';
        out += ok ? c : '_';
    }
    // No hidden/relative names.
    while (!out.empty() && (out.front() == '.' || out.front() == ' '))
        out.erase(out.begin());
    // Collapse any ".." sequences left over.
    size_t pos;
    while ((pos = out.find("..")) != std::string::npos)
        out.replace(pos, 2, ".");
    if (out.size() > 120) {
        const std::string ext = fs::path(out).extension().string();
        out = out.substr(0, 120 - ext.size()) + ext;
    }
    if (out.empty() || out == "." || fs::path(out).stem().string().empty())
        out = "print.gcode";
    return out;
}

bool FlashforgeLan::upload(PrintHostUpload upload_data, ProgressFn progress_fn, ErrorFn error_fn, InfoFn info_fn) const
{
    wxString    msg;
    std::string printer_name, status, serial;
    if (!query_detail(msg, &printer_name, &status, serial)) {
        error_fn(std::move(msg));
        return false;
    }
    info_fn(L"connected", GUI::from_u8(printer_name));

    const bool print_now = upload_data.post_action == PrintHostPostUploadAction::StartPrint;
    if (print_now && (boost::iequals(status, "printing") || boost::iequals(status, "busy") || boost::iequals(status, "pause"))) {
        error_fn(_L("The printer is busy and cannot start a new print now."));
        return false;
    }

    boost::system::error_code size_ec;
    const auto file_size = fs::file_size(upload_data.source_path, size_ec);
    if (size_ec) {
        error_fn(_L("Could not read the sliced file from disk."));
        return false;
    }

    const std::string remote_name = sanitize_remote_filename(upload_data.upload_path.string());
    const std::string url         = make_url("/uploadGcode");

    BOOST_LOG_TRIVIAL(info) << boost::format("%1%: uploading %2% (%3% bytes) as %4%, print now: %5%")
        % FF_LAN_NAME % upload_data.source_path % file_size % remote_name % (print_now ? "yes" : "no");

    bool res = true;
    auto http = Http::post(url);
    http.header("serialNumber", serial)
        .header("checkCode", m_check_code)
        .header("fileSize", std::to_string(file_size))
        .header("printNow", print_now ? "true" : "false")
        .header("levelingBeforePrint", "false")
        .form_add_file("gcodeFile", upload_data.source_path, remote_name)
        .timeout_connect(FF_CONNECT_TIMEOUT)
        .on_complete([&](std::string body, unsigned http_status) {
            int         code = -1;
            std::string message;
            if (!parse_code_message(body, code, message)) {
                BOOST_LOG_TRIVIAL(error) << FF_LAN_NAME << ": invalid upload response, HTTP " << http_status;
                error_fn(_L("The printer sent an invalid response to the upload."));
                res = false;
                return;
            }
            if (code != 0) {
                BOOST_LOG_TRIVIAL(error) << boost::format("%1%: upload rejected: %2% (code %3%)") % FF_LAN_NAME % message % code;
                if (boost::icontains(message, "check") || boost::icontains(message, "auth"))
                    error_fn(_L("Authentication failed. Check the CheckCode shown on the printer screen (LAN mode)."));
                else if (boost::icontains(message, "busy") || boost::icontains(message, "printing"))
                    error_fn(_L("The printer is busy and cannot start a new print now."));
                else
                    error_fn(GUI::format_wxstr(_L("The printer refused the upload: %1% (code %2%)"), message, code));
                res = false;
                return;
            }
            BOOST_LOG_TRIVIAL(info) << FF_LAN_NAME << ": upload finished successfully";
            if (print_now)
                info_fn(L"print_started", GUI::from_u8(remote_name));
        })
        .on_error([&](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: upload failed: %2%, HTTP %3%") % FF_LAN_NAME % error % http_status;
            error_fn(format_error(body, error, http_status));
            res = false;
        })
        .on_progress([&](Http::Progress progress, bool &cancel) {
            progress_fn(std::move(progress), cancel);
            if (cancel) {
                BOOST_LOG_TRIVIAL(info) << FF_LAN_NAME << ": upload canceled";
                res = false;
            }
        })
        .perform_sync();

    return res;
}

} // namespace Slic3r
