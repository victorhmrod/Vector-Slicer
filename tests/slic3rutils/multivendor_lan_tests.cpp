// Tests for the multivendor LAN print hosts (FlashForge AD5M / Anycubic Kobra 3).
// Everything runs against local mock servers - no printer is ever contacted.

#include <catch2/catch.hpp>

#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

#include <openssl/evp.h>

#include "libslic3r/PrintConfig.hpp"
#include "slic3r/Utils/PrintHost.hpp"
#include "slic3r/Utils/FlashforgeLan.hpp"
#include "slic3r/Utils/AnycubicLan.hpp"
#include "nlohmann/json.hpp"

using namespace Slic3r;
using json = nlohmann::json;

namespace {

// ---------------------------------------------------------------------------
// Minimal single-threaded HTTP mock server on 127.0.0.1 (ephemeral port).
// The handler receives the raw request (headers + body) and returns the raw
// response to write back.
class MockHttpServer
{
public:
    using Handler = std::function<std::string(const std::string &request)>;

    explicit MockHttpServer(Handler handler)
        : m_handler(std::move(handler))
        , m_acceptor(m_io, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0))
    {
        m_port   = m_acceptor.local_endpoint().port();
        m_thread = std::thread([this]() { this->run(); });
    }

    ~MockHttpServer()
    {
        m_stop = true;
        boost::system::error_code ec;
        m_acceptor.close(ec);
        if (m_thread.joinable())
            m_thread.join();
    }

    int port() const { return m_port; }

private:
    void run()
    {
        while (!m_stop) {
            boost::asio::ip::tcp::socket socket(m_io);
            boost::system::error_code    ec;
            m_acceptor.accept(socket, ec);
            if (ec)
                break;
            std::string request;
            char        buffer[4096];
            size_t      content_length = 0;
            bool        headers_done   = false;
            while (true) {
                const size_t n = socket.read_some(boost::asio::buffer(buffer), ec);
                if (ec)
                    break;
                request.append(buffer, n);
                if (!headers_done) {
                    const auto pos = request.find("\r\n\r\n");
                    if (pos == std::string::npos)
                        continue;
                    headers_done = true;
                    // Extract Content-Length if present.
                    std::string headers = request.substr(0, pos);
                    boost::algorithm::to_lower(headers);
                    const auto cl = headers.find("content-length:");
                    if (cl != std::string::npos)
                        content_length = std::stoul(headers.substr(cl + 15));
                }
                const auto body_start = request.find("\r\n\r\n") + 4;
                if (request.size() - body_start >= content_length)
                    break;
            }
            if (!request.empty()) {
                const std::string response = m_handler(request);
                boost::asio::write(socket, boost::asio::buffer(response), ec);
            }
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        }
    }

    Handler                        m_handler;
    boost::asio::io_context        m_io;
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::thread                    m_thread;
    std::atomic<bool>              m_stop{false};
    int                            m_port = 0;
};

std::string http_json_response(const std::string &body)
{
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

DynamicPrintConfig make_host_config(const std::string &host_type_key, const std::string &host,
                                    const std::string &apikey = std::string(), const std::string &user = std::string())
{
    DynamicPrintConfig config;
    auto *def_enum = new ConfigOptionEnum<PrintHostType>();
    REQUIRE(def_enum->deserialize(host_type_key));
    config.set_key_value("host_type", def_enum);
    config.set_key_value("print_host", new ConfigOptionString(host));
    config.set_key_value("printhost_apikey", new ConfigOptionString(apikey));
    config.set_key_value("printhost_user", new ConfigOptionString(user));
    config.set_key_value("printer_technology", new ConfigOptionEnum<PrinterTechnology>(ptFFF));
    config.set_key_value("gcode_flavor", new ConfigOptionEnum<GCodeFlavor>(gcfKlipper));
    // Every backend constructor reads its own subset of these; a missing key
    // would dereference a null option, so provide the whole print-host set.
    config.set_key_value("printhost_cafile", new ConfigOptionString());
    config.set_key_value("printhost_password", new ConfigOptionString());
    config.set_key_value("printhost_port", new ConfigOptionString());
    config.set_key_value("print_host_webui", new ConfigOptionString());
    config.set_key_value("printhost_ssl_ignore_revoke", new ConfigOptionBool(false));
    config.set_key_value("printhost_authorization_type", new ConfigOptionEnum<AuthorizationType>(atKeyPassword));
    config.set_key_value("bbl_use_printhost", new ConfigOptionBool(false));
    return config;
}

std::string b64_encode(const std::string &data)
{
    std::string out;
    out.resize(4 * ((data.size() + 2) / 3) + 1);
    const int n = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()),
                                  reinterpret_cast<const unsigned char *>(data.data()), int(data.size()));
    out.resize(size_t(n));
    return out;
}

std::string aes_cbc_encrypt(const std::string &plaintext, const std::string &key16, const std::string &iv_source)
{
    unsigned char iv[16] = {0};
    std::memcpy(iv, iv_source.data(), std::min<size_t>(16, iv_source.size()));
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    REQUIRE(ctx != nullptr);
    REQUIRE(EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
                               reinterpret_cast<const unsigned char *>(key16.data()), iv) == 1);
    std::string out;
    out.resize(plaintext.size() + 32);
    int len1 = 0;
    REQUIRE(EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(out.data()), &len1,
                              reinterpret_cast<const unsigned char *>(plaintext.data()), int(plaintext.size())) == 1);
    int len2 = 0;
    REQUIRE(EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(out.data()) + len1, &len2) == 1);
    EVP_CIPHER_CTX_free(ctx);
    out.resize(size_t(len1 + len2));
    return out;
}

} // namespace

// ---------------------------------------------------------------------------

TEST_CASE("Multivendor host types serialize and deserialize", "[MultivendorLan]") {
    for (const std::string key : {"flashforge_lan", "anycubic_lan", "octoprint", "flashforge", "elegoolink"}) {
        ConfigOptionEnum<PrintHostType> opt;
        REQUIRE(opt.deserialize(key));
        REQUIRE(opt.serialize() == key);
    }
    ConfigOptionEnum<PrintHostType> ff;
    REQUIRE(ff.deserialize("flashforge_lan"));
    REQUIRE(ff.value == htFlashforgeLan);
    ConfigOptionEnum<PrintHostType> ac;
    REQUIRE(ac.deserialize("anycubic_lan"));
    REQUIRE(ac.value == htAnycubicLan);
}

TEST_CASE("Print host factory keeps existing backends and registers the new ones", "[MultivendorLan]") {
    const std::vector<std::pair<std::string, std::string>> expectations = {
        {"octoprint", "OctoPrint"},
        {"duet", "Duet"},
        {"mks", "MKS"},
        {"flashforge", "Flashforge"},
        {"flashforge_lan", "FlashForge LAN"},
        {"anycubic_lan", "Anycubic LAN"},
    };
    for (const auto &[key, name] : expectations) {
        DynamicPrintConfig config = make_host_config(key, "192.168.1.50");
        std::unique_ptr<PrintHost> host(PrintHost::get_print_host(&config, false));
        REQUIRE(host != nullptr);
        REQUIRE(std::string(host->get_name()) == name);
    }
}

TEST_CASE("New backends report their capabilities", "[MultivendorLan]") {
    DynamicPrintConfig ff_config = make_host_config("flashforge_lan", "192.168.1.50", "12345");
    std::unique_ptr<PrintHost> ff(PrintHost::get_print_host(&ff_config, false));
    const PrintHostCapabilities ff_caps = ff->get_capabilities();
    REQUIRE(ff_caps.can_upload);
    REQUIRE(ff_caps.can_start_print);
    REQUIRE(ff_caps.can_query_status);
    REQUIRE(!ff_caps.can_pause);
    REQUIRE(!ff_caps.can_access_camera);

    DynamicPrintConfig ac_config = make_host_config("anycubic_lan", "192.168.1.60");
    std::unique_ptr<PrintHost> ac(PrintHost::get_print_host(&ac_config, false));
    const PrintHostCapabilities ac_caps = ac->get_capabilities();
    REQUIRE(ac_caps.can_upload);
    REQUIRE(ac_caps.can_start_print);
    REQUIRE(!ac_caps.can_cancel);
}

TEST_CASE("FlashForge LAN configuration validation", "[MultivendorLan]") {
    REQUIRE(FlashforgeLan::validate_config("", "", "1234").empty() == false);
    REQUIRE(FlashforgeLan::validate_config("192.168.1.10", "", "").empty() == false);       // missing check code
    REQUIRE(FlashforgeLan::validate_config("192.168.1.10/path", "", "1234").empty() == false);
    REQUIRE(FlashforgeLan::validate_config("192.168.1.10:99999", "", "1234").empty() == false);
    REQUIRE(FlashforgeLan::validate_config("192.168.1.10", "", "1234").empty());            // serial optional
    REQUIRE(FlashforgeLan::validate_config("192.168.1.10:8898", "SN123", "1234").empty());
    REQUIRE(FlashforgeLan::validate_config("http://192.168.1.10:8898", "SN123", "1234").empty());
}

TEST_CASE("Anycubic LAN configuration validation", "[MultivendorLan]") {
    REQUIRE(AnycubicLan::validate_config("").empty() == false);
    REQUIRE(AnycubicLan::validate_config("192.168.1.10/api").empty() == false);
    REQUIRE(AnycubicLan::validate_config("192.168.1.10:9999").empty() == false); // ports are fixed
    REQUIRE(AnycubicLan::validate_config("192.168.1.10").empty());
    REQUIRE(AnycubicLan::validate_config("kobra3.local").empty());
}

TEST_CASE("Remote filenames are sanitized against traversal and junk", "[MultivendorLan]") {
    for (auto sanitize : {&FlashforgeLan::sanitize_remote_filename, &AnycubicLan::sanitize_remote_filename}) {
        REQUIRE(sanitize("benchy.gcode") == "benchy.gcode");
        REQUIRE(sanitize("dir/sub/benchy.gcode") == "benchy.gcode");
        REQUIRE(sanitize("..\\..\\windows\\evil.gcode") == "evil.gcode");
        REQUIRE(sanitize("../../etc/passwd") == "passwd");
        const std::string weird = sanitize("bad<>:\"|?*name.gcode");
        REQUIRE(weird.find('<') == std::string::npos);
        REQUIRE(weird.find('|') == std::string::npos);
        REQUIRE(sanitize("...") == "print.gcode");
        REQUIRE(sanitize("") == "print.gcode");
        const std::string long_name = sanitize(std::string(300, 'a') + ".gcode");
        REQUIRE(long_name.size() <= 120);
        REQUIRE(boost::algorithm::ends_with(long_name, ".gcode"));
    }
}

TEST_CASE("FlashForge LAN response envelope parsing", "[MultivendorLan]") {
    int         code = -1;
    std::string message;
    REQUIRE(FlashforgeLan::parse_code_message("{\"code\":0,\"message\":\"Success\"}", code, message));
    REQUIRE(code == 0);
    REQUIRE(message == "Success");
    REQUIRE(FlashforgeLan::parse_code_message("{\"code\":1,\"message\":\"check code error\"}", code, message));
    REQUIRE(code == 1);
    REQUIRE(!FlashforgeLan::parse_code_message("<html>not json</html>", code, message));
    REQUIRE(!FlashforgeLan::parse_code_message("", code, message));
}

TEST_CASE("Anycubic handshake crypto primitives", "[MultivendorLan]") {
    // RFC 1321 test vector.
    REQUIRE(AnycubicLan::md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    // sign = md5(md5(token[:16]) + ts + nonce), inner digest as hex string.
    const std::string token = "0123456789abcdefFEDCBA9876543210";
    const std::string sign  = AnycubicLan::handshake_sign(token, "1700000000000", "abc123");
    REQUIRE(sign == AnycubicLan::md5_hex(AnycubicLan::md5_hex("0123456789abcdef") + "1700000000000" + "abc123"));
    REQUIRE(sign.size() == 32);

    // AES-128-CBC round trip with the documented key/IV derivation.
    const std::string key   = token.substr(16, 16);
    const std::string local = "iv-local-token";
    const std::string plain = "{\"broker\":\"mqtts://192.168.1.60:9883\",\"username\":\"u\",\"password\":\"p\",\"deviceId\":\"d\"}";
    const std::string cipher = aes_cbc_encrypt(plain, key, local);
    std::string decrypted;
    REQUIRE(AnycubicLan::aes_cbc_decrypt(cipher, key, local, decrypted));
    REQUIRE(decrypted == plain);
    // Wrong key must fail (padding check).
    REQUIRE(!AnycubicLan::aes_cbc_decrypt(cipher, std::string(16, 'x'), local, decrypted));
}

TEST_CASE("FlashForge LAN test() against a mock printer", "[MultivendorLan]") {
    std::string last_request;
    MockHttpServer server([&](const std::string &request) {
        last_request = request;
        if (request.find("POST /detail") == 0) {
            return http_json_response(
                "{\"code\":0,\"message\":\"Success\",\"detail\":{\"name\":\"AD5M Test\",\"status\":\"ready\",\"pid\":35}}");
        }
        return http_json_response("{\"code\":404,\"message\":\"unknown endpoint\"}");
    });

    DynamicPrintConfig config = make_host_config("flashforge_lan",
                                                 "127.0.0.1:" + std::to_string(server.port()),
                                                 "9876", "SN12345678");
    std::unique_ptr<PrintHost> host(PrintHost::get_print_host(&config, false));
    wxString msg;
    REQUIRE(host->test(msg));
    // Request must carry the auth envelope, and never leak into a GET.
    REQUIRE(last_request.find("POST /detail") == 0);
    REQUIRE(last_request.find("\"serialNumber\":\"SN12345678\"") != std::string::npos);
    REQUIRE(last_request.find("\"checkCode\":\"9876\"") != std::string::npos);
}

TEST_CASE("FlashForge LAN reports authentication failure", "[MultivendorLan]") {
    MockHttpServer server([&](const std::string &) {
        return http_json_response("{\"code\":1,\"message\":\"check code error\"}");
    });
    DynamicPrintConfig config = make_host_config("flashforge_lan",
                                                 "127.0.0.1:" + std::to_string(server.port()),
                                                 "0000", "SN12345678");
    std::unique_ptr<PrintHost> host(PrintHost::get_print_host(&config, false));
    wxString msg;
    REQUIRE(!host->test(msg));
    REQUIRE(!msg.empty());
}

TEST_CASE("FlashForge LAN rejects an invalid response body", "[MultivendorLan]") {
    MockHttpServer server([&](const std::string &) {
        return std::string("HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\n<html></html>");
    });
    DynamicPrintConfig config = make_host_config("flashforge_lan",
                                                 "127.0.0.1:" + std::to_string(server.port()),
                                                 "1234", "SN1");
    std::unique_ptr<PrintHost> host(PrintHost::get_print_host(&config, false));
    wxString msg;
    REQUIRE(!host->test(msg));
    REQUIRE(!msg.empty());
}

TEST_CASE("FlashForge LAN handles a refused connection", "[MultivendorLan]") {
    // Reserve an ephemeral port, then close it so nothing listens there.
    int dead_port = 0;
    {
        boost::asio::io_context        io;
        boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
        dead_port = acceptor.local_endpoint().port();
    }
    DynamicPrintConfig config = make_host_config("flashforge_lan",
                                                 "127.0.0.1:" + std::to_string(dead_port),
                                                 "1234", "SN1");
    std::unique_ptr<PrintHost> host(PrintHost::get_print_host(&config, false));
    wxString msg;
    REQUIRE(!host->test(msg));
    REQUIRE(!msg.empty());
}

TEST_CASE("Anycubic LAN full credential handshake against a mock printer", "[MultivendorLan]") {
    const std::string token       = "AABBCCDDEEFF00112233445566778899";
    const std::string local_token = "LT0011223344";
    const std::string creds       = json({{"broker", "mqtts://127.0.0.1:9883"},
                                          {"username", "mockuser"},
                                          {"password", "mockpass"},
                                          {"deviceId", "mockdevice"}})
                                        .dump();
    std::string ctrl_request;
    std::shared_ptr<MockHttpServer> server;
    server = std::make_shared<MockHttpServer>([&](const std::string &request) -> std::string {
        if (request.find("GET /info") == 0) {
            const json info = {
                {"token", token},
                {"modelName", "Anycubic Kobra 3"},
                {"modelId", "20024"},
                {"cn", "MOCKSERIAL"},
                {"ctrlInfoUrl", "http://127.0.0.1:" + std::to_string(server->port()) + "/ctrl"},
                {"fileUploadurl", "http://127.0.0.1:" + std::to_string(server->port()) + "/gcode_upload"},
            };
            return http_json_response(info.dump());
        }
        if (request.find("POST /ctrl") == 0) {
            ctrl_request = request;
            const std::string cipher = aes_cbc_encrypt(creds, token.substr(16, 16), local_token);
            const json response = {{"code", 200}, {"message", "success"},
                                   {"data", {{"token", local_token}, {"info", b64_encode(cipher)}}}};
            return http_json_response(response.dump());
        }
        return http_json_response("{\"code\":404}");
    });

    DynamicPrintConfig config = make_host_config("anycubic_lan", "127.0.0.1");
    std::unique_ptr<PrintHost> base(PrintHost::get_print_host(&config, false));
    auto *host = dynamic_cast<AnycubicLan *>(base.get());
    REQUIRE(host != nullptr);
    host->set_info_port_for_testing(server->port());

    wxString msg;
    REQUIRE(host->test(msg));
    // The signed request must carry ts, nonce, sign and did query parameters.
    REQUIRE(ctrl_request.find("ts=") != std::string::npos);
    REQUIRE(ctrl_request.find("nonce=") != std::string::npos);
    REQUIRE(ctrl_request.find("sign=") != std::string::npos);
    REQUIRE(ctrl_request.find("did=") != std::string::npos);
}

TEST_CASE("Anycubic LAN reports a rejected credential request", "[MultivendorLan]") {
    std::shared_ptr<MockHttpServer> server;
    server = std::make_shared<MockHttpServer>([&](const std::string &request) -> std::string {
        if (request.find("GET /info") == 0) {
            const json info = {
                {"token", "AABBCCDDEEFF00112233445566778899"},
                {"modelName", "Anycubic Kobra 3"},
                {"modelId", "20024"},
                {"ctrlInfoUrl", "http://127.0.0.1:" + std::to_string(server->port()) + "/ctrl"},
            };
            return http_json_response(info.dump());
        }
        return http_json_response("{\"code\":401,\"message\":\"forbidden\"}");
    });

    DynamicPrintConfig config = make_host_config("anycubic_lan", "127.0.0.1");
    std::unique_ptr<PrintHost> base(PrintHost::get_print_host(&config, false));
    auto *host = dynamic_cast<AnycubicLan *>(base.get());
    REQUIRE(host != nullptr);
    host->set_info_port_for_testing(server->port());

    wxString msg;
    REQUIRE(!host->test(msg));
    REQUIRE(!msg.empty());
}
