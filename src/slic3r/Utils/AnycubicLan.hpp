#ifndef slic3r_AnycubicLan_hpp_
#define slic3r_AnycubicLan_hpp_

#include <string>
#include <wx/string.h>
#include "PrintHost.hpp"

namespace Slic3r {

class DynamicPrintConfig;

// LAN print host for the Anycubic Kobra 3 generation (Kobra 3 / 3 V2 / 3 Max /
// S1 / S1 Max) running the stock firmware with "LAN Mode" enabled on the
// printer screen. No Anycubic cloud account and no proprietary library is
// involved:
//
//   1. GET  http://<ip>:18910/info            -> token, ctrlInfoUrl, modelId, cn
//   2. POST <ctrlInfoUrl>?ts&nonce&sign&did   -> AES-CBC encrypted config blob
//      sign = md5( md5(token[:16]) + ts + nonce )
//      key  = token[16:32], IV = response token (padded/truncated to 16 bytes)
//      decrypted JSON -> MQTT broker/username/password/deviceId
//   3. multipart POST <fileUploadurl>         -> upload the sliced file
//   4. MQTT over TLS <ip>:9883 (self-signed)  -> "print:start" command
//
// Protocol references (all community-documented, no code copied):
//   https://jbatonnet.github.io/Rinkhals/firmware/mqtt/
//   https://github.com/chrisfore/anycubic_ha_local (MIT, hardware-validated)
//   https://github.com/ChestnutLabs/printer-protocol-orchard
class AnycubicLan : public PrintHost
{
public:
    explicit AnycubicLan(DynamicPrintConfig *config);
    ~AnycubicLan() override = default;

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

    // Returns an empty string when the configured host is usable.
    static wxString validate_config(const std::string &host);
    // Same sanitizer contract as the FlashForge backend; public for tests.
    static std::string sanitize_remote_filename(const std::string &filename);

    // Pure helpers of the credential handshake, public for unit testing.
    static std::string md5_hex(const std::string &data);
    static std::string handshake_sign(const std::string &token, const std::string &ts, const std::string &nonce);
    static bool        aes_cbc_decrypt(const std::string &ciphertext, const std::string &key16, const std::string &iv_source, std::string &plaintext);

    // Result of the /info + /ctrl handshake.
    struct Identity
    {
        std::string model_name;
        std::string model_id;
        std::string device_id;
        std::string serial;
        std::string file_upload_url;
        std::string mqtt_broker;   // e.g. "mqtts://192.168.0.10:9883"
        std::string mqtt_username;
        std::string mqtt_password;
    };

private:
    std::string m_host; // IP or hostname (ports are fixed by the protocol)

    std::string host_only() const;
    bool        handshake(Identity &identity, wxString &msg) const;
    bool        mqtt_start_print(const Identity &identity, const std::string &remote_filename, const std::string &file_md5,
                                 uint64_t file_size, wxString &msg) const;
};

} // namespace Slic3r

#endif
