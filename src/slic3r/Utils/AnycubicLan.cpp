#include "AnycubicLan.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include "libslic3r/PrintConfig.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "Http.hpp"
#include "MQTT.hpp"
#include "nlohmann/json.hpp"

namespace fs = boost::filesystem;
using json = nlohmann::json;

namespace Slic3r {

namespace {

const char *AC_LAN_NAME     = "Anycubic LAN";
const int   AC_INFO_PORT    = 18910;
const int   AC_MQTT_PORT    = 9883;
const long  AC_CONNECT_TIMEOUT = 5;   // seconds
const long  AC_REQUEST_TIMEOUT = 15;  // seconds (control requests, not upload)
const int   AC_MQTT_ACK_TIMEOUT_S = 15;

const char *AC_TOPIC_ROOT = "anycubic/anycubicCloud/v1";

std::string random_string(const char *alphabet, size_t len)
{
    static thread_local std::mt19937 rng{std::random_device{}()};
    const size_t                     n = std::strlen(alphabet);
    std::uniform_int_distribution<size_t> dist(0, n - 1);
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i)
        out += alphabet[dist(rng)];
    return out;
}

std::string epoch_ms_string()
{
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    return std::to_string(ms);
}

std::string new_msgid()
{
    return boost::uuids::to_string(boost::uuids::random_generator()());
}

std::string b64_decode(const std::string &input)
{
    std::string out;
    if (input.empty())
        return out;
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO *mem = BIO_new_mem_buf(input.data(), int(input.size()));
    mem      = BIO_push(b64, mem);
    out.resize(input.size());
    const int n = BIO_read(mem, out.data(), int(input.size()));
    BIO_free_all(mem);
    if (n <= 0)
        return std::string();
    out.resize(size_t(n));
    return out;
}

// Unwrap the tolerant "/info" shape: either a bare object or {code, data:{...}}.
json unwrap_info(const json &j)
{
    if (j.contains("data") && j["data"].is_object())
        return j["data"];
    return j;
}

std::string md5_file_hex(const fs::path &path)
{
    boost::filesystem::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return {};
    MD5_CTX ctx;
    MD5_Init(&ctx);
    char buffer[64 * 1024];
    while (in.good()) {
        in.read(buffer, sizeof(buffer));
        const std::streamsize n = in.gcount();
        if (n > 0)
            MD5_Update(&ctx, buffer, size_t(n));
    }
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);
    std::ostringstream oss;
    for (unsigned char c : digest)
        oss << std::hex << std::setw(2) << std::setfill('0') << int(c);
    return oss.str();
}

} // namespace

AnycubicLan::AnycubicLan(DynamicPrintConfig *config)
    : m_host(config->opt_string("print_host"))
{
    boost::trim(m_host);
}

const char *AnycubicLan::get_name() const { return AC_LAN_NAME; }

wxString AnycubicLan::validate_config(const std::string &host)
{
    std::string h = host;
    boost::trim(h);
    if (h.empty())
        return _L("The printer IP address or hostname is not set.");
    if (h.find("http://") == 0 || h.find("https://") == 0)
        h = h.substr(h.find("//") + 2);
    if (h.find('/') != std::string::npos || h.find(':') != std::string::npos)
        return _L("Enter only the printer IP address or hostname. The Anycubic LAN ports are fixed by the protocol.");
    return wxString();
}

std::string AnycubicLan::host_only() const
{
    std::string h = m_host;
    if (h.find("http://") == 0 || h.find("https://") == 0)
        h = h.substr(h.find("//") + 2);
    const auto slash = h.find('/');
    if (slash != std::string::npos)
        h = h.substr(0, slash);
    const auto colon = h.find(':');
    if (colon != std::string::npos)
        h = h.substr(0, colon);
    return h;
}

std::string AnycubicLan::md5_hex(const std::string &data)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char *>(data.data()), data.size(), digest);
    std::ostringstream oss;
    for (unsigned char c : digest)
        oss << std::hex << std::setw(2) << std::setfill('0') << int(c);
    return oss.str();
}

std::string AnycubicLan::handshake_sign(const std::string &token, const std::string &ts, const std::string &nonce)
{
    // sign = md5( md5(token[:16]) + ts + nonce ); the inner digest is the
    // lowercase hex string, not raw bytes.
    return md5_hex(md5_hex(token.substr(0, 16)) + ts + nonce);
}

bool AnycubicLan::aes_cbc_decrypt(const std::string &ciphertext, const std::string &key16, const std::string &iv_source, std::string &plaintext)
{
    plaintext.clear();
    if (key16.size() != 16 || ciphertext.empty())
        return false;
    unsigned char iv[16] = {0};
    std::memcpy(iv, iv_source.data(), std::min<size_t>(16, iv_source.size()));

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr)
        return false;
    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
                                 reinterpret_cast<const unsigned char *>(key16.data()), iv) == 1;
    std::string out;
    if (ok) {
        out.resize(ciphertext.size() + 16);
        int len1 = 0;
        ok = EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(out.data()), &len1,
                               reinterpret_cast<const unsigned char *>(ciphertext.data()), int(ciphertext.size())) == 1;
        int len2 = 0;
        if (ok)
            ok = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(out.data()) + len1, &len2) == 1;
        if (ok)
            out.resize(size_t(len1 + len2));
    }
    EVP_CIPHER_CTX_free(ctx);
    if (ok)
        plaintext = std::move(out);
    return ok;
}

bool AnycubicLan::handshake(Identity &identity, wxString &msg) const
{
    const wxString cfg_error = validate_config(m_host);
    if (!cfg_error.empty()) {
        msg = cfg_error;
        return false;
    }

    const std::string host     = host_only();
    const std::string info_url = (boost::format("http://%1%:%2%/info") % host % AC_INFO_PORT).str();

    BOOST_LOG_TRIVIAL(info) << AC_LAN_NAME << ": querying printer identity at " << info_url;

    std::string info_body;
    bool        http_ok = false;
    Http::get(info_url)
        .timeout_connect(AC_CONNECT_TIMEOUT)
        .timeout_max(AC_REQUEST_TIMEOUT)
        .on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: /info failed: %2%, HTTP %3%") % AC_LAN_NAME % error % status;
            msg = format_error(body, error, status);
            msg += "\n" + _L("Make sure LAN Mode is enabled on the printer screen (Settings > Network > LAN Mode). A sleeping printer does not answer.");
        })
        .on_complete([&](std::string body, unsigned) {
            info_body = std::move(body);
            http_ok   = true;
        })
        .perform_sync();
    if (!http_ok)
        return false;

    std::string token, ctrl_url;
    try {
        const json info = unwrap_info(json::parse(info_body));
        token                    = info.value("token", std::string());
        ctrl_url                 = info.value("ctrlInfoUrl", std::string());
        identity.model_name      = info.value("modelName", info.value("deviceName", std::string()));
        identity.model_id        = info.value("modelId", std::string());
        identity.serial          = info.value("cn", std::string());
        identity.file_upload_url = info.value("fileUploadurl", std::string());
    } catch (const std::exception &) {
        msg = _L("The printer sent an invalid identity response. Check that the address points to an Anycubic printer in LAN mode.");
        return false;
    }
    if (token.size() < 32) {
        msg = _L("The printer did not provide a LAN-mode token. Enable LAN Mode on the printer screen and try again.");
        return false;
    }
    if (ctrl_url.empty())
        ctrl_url = (boost::format("http://%1%:%2%/ctrl") % host % AC_INFO_PORT).str();
    if (identity.file_upload_url.empty())
        identity.file_upload_url = (boost::format("http://%1%:%2%/gcode_upload") % host % AC_INFO_PORT).str();

    // Signed credential request.
    const std::string ts    = epoch_ms_string();
    const std::string nonce = random_string("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 6);
    const std::string did   = random_string("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 32);
    const std::string sign  = handshake_sign(token, ts, nonce);
    const std::string ctrl_full = (boost::format("%1%?ts=%2%&nonce=%3%&sign=%4%&did=%5%") % ctrl_url % ts % nonce % sign % did).str();

    std::string ctrl_body;
    http_ok = false;
    Http::post(ctrl_full)
        .set_post_body(std::string())
        .timeout_connect(AC_CONNECT_TIMEOUT)
        .timeout_max(AC_REQUEST_TIMEOUT)
        .on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: /ctrl failed: %2%, HTTP %3%") % AC_LAN_NAME % error % status;
            msg = format_error(body, error, status);
        })
        .on_complete([&](std::string body, unsigned) {
            ctrl_body = std::move(body);
            http_ok   = true;
        })
        .perform_sync();
    if (!http_ok)
        return false;

    try {
        const json ctrl = json::parse(ctrl_body);
        const int  code = ctrl.value("code", 0);
        if (code != 200) {
            msg = GUI::format_wxstr(_L("The printer refused the LAN-mode credential request (code %1%). Re-enable LAN Mode and try again."), code);
            return false;
        }
        const std::string local_token = ctrl["data"].value("token", std::string());
        const std::string info_b64    = ctrl["data"].value("info", std::string());
        const std::string cipher      = b64_decode(info_b64);
        std::string       plain;
        if (cipher.empty() || !aes_cbc_decrypt(cipher, token.substr(16, 16), local_token, plain)) {
            msg = _L("Could not decrypt the LAN-mode credentials. The printer firmware may use an unsupported handshake version.");
            return false;
        }
        const json cred        = json::parse(plain);
        identity.mqtt_broker   = cred.value("broker", std::string());
        identity.mqtt_username = cred.value("username", std::string());
        identity.mqtt_password = cred.value("password", std::string());
        identity.device_id     = cred.value("deviceId", std::string());
    } catch (const std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << AC_LAN_NAME << ": handshake parse failure: " << e.what();
        msg = _L("The printer sent an invalid credential response.");
        return false;
    }

    if (identity.mqtt_username.empty() || identity.mqtt_password.empty() || identity.device_id.empty()) {
        msg = _L("The LAN-mode credentials are incomplete. Re-enable LAN Mode on the printer and try again.");
        return false;
    }
    if (identity.mqtt_broker.empty())
        identity.mqtt_broker = (boost::format("mqtts://%1%:%2%") % host % AC_MQTT_PORT).str();

    BOOST_LOG_TRIVIAL(info) << boost::format("%1%: handshake OK, model %2% (id %3%)")
        % AC_LAN_NAME % identity.model_name % identity.model_id;
    return true;
}

bool AnycubicLan::test(wxString &msg) const
{
    Identity identity;
    return handshake(identity, msg);
}

wxString AnycubicLan::get_test_ok_msg() const
{
    return _L("Connection to the Anycubic printer works correctly (LAN Mode).");
}

wxString AnycubicLan::get_test_failed_msg(wxString &msg) const
{
    return GUI::format_wxstr("%1%: %2%", _L("Could not connect to the Anycubic printer"), msg);
}

std::string AnycubicLan::sanitize_remote_filename(const std::string &filename)
{
    std::string name = fs::path(filename).filename().string();
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) != 0 ||
                        c == '.' || c == '_' || c == '-' || c == '+' || c == '(' || c == ')' || c == ' ' || c == '#';
        out += ok ? c : '_';
    }
    while (!out.empty() && (out.front() == '.' || out.front() == ' '))
        out.erase(out.begin());
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

bool AnycubicLan::mqtt_start_print(const Identity &identity, const std::string &remote_filename, const std::string &file_md5,
                                   uint64_t file_size, wxString &msg) const
{
    // Paho expects the "ssl://" scheme.
    std::string broker = identity.mqtt_broker;
    if (broker.rfind("mqtts://", 0) == 0)
        broker = "ssl://" + broker.substr(8);
    else if (broker.rfind("mqtt://", 0) == 0)
        broker = "tcp://" + broker.substr(7);

    const std::string client_id = "OrcaSlicerFS-" + random_string("abcdefghijklmnopqrstuvwxyz0123456789", 8);
    const std::string base      = std::string(AC_TOPIC_ROOT);
    const std::string print_topic    = base + "/slicer/printer/" + identity.model_id + "/" + identity.device_id + "/print";
    const std::string report_wildcard = base + "/printer/public/" + identity.model_id + "/" + identity.device_id + "/#";

    const std::string msgid = new_msgid();

    json payload;
    payload["type"]      = "print";
    payload["action"]    = "start";
    payload["msgid"]     = msgid;
    payload["timestamp"] = std::stoll(epoch_ms_string());
    payload["data"]      = {
        {"taskid", "-1"},
        {"filename", remote_filename},
        {"filetype", 1},
        {"md5", file_md5},
        {"filesize", file_size},
    };

    std::mutex              mtx;
    std::condition_variable cv;
    bool                    acked        = false;
    bool                    confirmed    = false;
    bool                    rejected     = false;
    std::string             reject_error;

    try {
        // Empty CA/cert/key: the printer serves a self-signed certificate and
        // does not require a client certificate; authentication is done with
        // the username/password derived by the handshake.
        auto client = std::make_shared<MqttClient>(broker, client_id,
                                                   std::string(), std::string(), std::string(),
                                                   identity.mqtt_username, identity.mqtt_password, true);
        client->SetMessageCallback([&](const std::string &topic, const std::string &body) {
            std::unique_lock<std::mutex> lock(mtx);
            if (boost::algorithm::ends_with(topic, "/response")) {
                // Bare {msgid} delivery ack.
                if (body.find(msgid) != std::string::npos) {
                    acked = true;
                    cv.notify_all();
                }
                return;
            }
            if (topic.find("/print/report") != std::string::npos) {
                try {
                    const json j    = json::parse(body);
                    const int  code = j.value("code", 0);
                    const std::string state = j.value("state", std::string());
                    if (code == 200 || state == "done" || state == "printing" || state == "preheating") {
                        confirmed = true;
                    } else {
                        rejected     = true;
                        reject_error = j.value("msg", j.value("message", std::string()));
                        if (reject_error.empty())
                            reject_error = "code " + std::to_string(code) + (state.empty() ? "" : (", state " + state));
                    }
                } catch (const std::exception &) {
                    // Ignore malformed reports; keep waiting for a valid one.
                }
                cv.notify_all();
            }
        });

        std::string mqtt_msg;
        if (!client->Connect(mqtt_msg)) {
            msg = GUI::format_wxstr(_L("Could not connect to the printer's MQTT service: %1%"), mqtt_msg);
            return false;
        }
        if (!client->Subscribe(report_wildcard, 1, mqtt_msg)) {
            client->Disconnect(mqtt_msg);
            msg = GUI::format_wxstr(_L("Could not subscribe to the printer's status channel: %1%"), mqtt_msg);
            return false;
        }
        if (!client->Publish(print_topic, payload.dump(), 1, mqtt_msg)) {
            client->Disconnect(mqtt_msg);
            msg = GUI::format_wxstr(_L("Could not send the print-start command: %1%"), mqtt_msg);
            return false;
        }

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(AC_MQTT_ACK_TIMEOUT_S), [&] { return confirmed || rejected; });
        }
        client->Disconnect(mqtt_msg);

        if (rejected) {
            msg = GUI::format_wxstr(_L("The printer rejected the print-start command: %1%"), reject_error);
            return false;
        }
        if (!confirmed && !acked) {
            msg = _L("The printer did not answer the print-start command (timeout). The file was uploaded; you can start it from the printer screen.");
            return false;
        }
        if (!confirmed)
            BOOST_LOG_TRIVIAL(warning) << AC_LAN_NAME << ": print-start acknowledged but not yet confirmed by a print report";
        return true;
    } catch (const std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << AC_LAN_NAME << ": MQTT failure: " << e.what();
        msg = GUI::format_wxstr(_L("MQTT communication with the printer failed: %1%"), e.what());
        return false;
    }
}

bool AnycubicLan::upload(PrintHostUpload upload_data, ProgressFn progress_fn, ErrorFn error_fn, InfoFn info_fn) const
{
    wxString msg;
    Identity identity;
    if (!handshake(identity, msg)) {
        error_fn(std::move(msg));
        return false;
    }
    info_fn(L"connected", GUI::from_u8(identity.model_name));

    boost::system::error_code size_ec;
    const auto file_size = fs::file_size(upload_data.source_path, size_ec);
    if (size_ec) {
        error_fn(_L("Could not read the sliced file from disk."));
        return false;
    }

    const std::string remote_name = sanitize_remote_filename(upload_data.upload_path.string());

    BOOST_LOG_TRIVIAL(info) << boost::format("%1%: uploading %2% (%3% bytes) as %4%")
        % AC_LAN_NAME % upload_data.source_path % file_size % remote_name;

    bool        res = true;
    std::string stored_name = remote_name;
    auto http = Http::post(identity.file_upload_url);
    http.header("X-File-Length", std::to_string(file_size))
        .form_add("filename", remote_name)
        .form_add_file("gcode", upload_data.source_path, remote_name)
        .timeout_connect(AC_CONNECT_TIMEOUT)
        .on_complete([&](std::string body, unsigned http_status) {
            try {
                const json j    = json::parse(body);
                const int  code = j.value("code", 0);
                if (code != 200) {
                    const std::string message = j.value("msg", j.value("message", std::string()));
                    error_fn(GUI::format_wxstr(_L("The printer refused the upload: %1% (code %2%)"), message, code));
                    res = false;
                    return;
                }
                if (j.contains("data") && j["data"].is_object())
                    stored_name = j["data"].value("gcode", remote_name);
            } catch (const std::exception &) {
                BOOST_LOG_TRIVIAL(error) << AC_LAN_NAME << ": invalid upload response, HTTP " << http_status;
                error_fn(_L("The printer sent an invalid response to the upload."));
                res = false;
                return;
            }
            BOOST_LOG_TRIVIAL(info) << AC_LAN_NAME << ": upload finished successfully";
        })
        .on_error([&](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: upload failed: %2%, HTTP %3%") % AC_LAN_NAME % error % http_status;
            error_fn(format_error(body, error, http_status));
            res = false;
        })
        .on_progress([&](Http::Progress progress, bool &cancel) {
            progress_fn(std::move(progress), cancel);
            if (cancel) {
                BOOST_LOG_TRIVIAL(info) << AC_LAN_NAME << ": upload canceled";
                res = false;
            }
        })
        .perform_sync();

    if (!res)
        return false;

    if (upload_data.post_action == PrintHostPostUploadAction::StartPrint) {
        const std::string file_md5 = md5_file_hex(upload_data.source_path);
        if (!mqtt_start_print(identity, stored_name, file_md5, file_size, msg)) {
            error_fn(std::move(msg));
            return false;
        }
        info_fn(L"print_started", GUI::from_u8(stored_name));
    }

    return true;
}

} // namespace Slic3r
