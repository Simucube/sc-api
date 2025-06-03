#ifndef SC_API_INTERNAL_SECURITY_IMPL_H
#define SC_API_INTERNAL_SECURITY_IMPL_H
#include "crypto/gcm.h"
#include "sc-api/core/session.h"

namespace sc_api::core::security {

struct KeyExchangeSaltData {
    uint32_t           api_session_id;
    const std::string& controller_id_name;
};

/** Try to generate secure session parameters by doing key exchange
 *
 *  Verifies API backend public key by the signature and does key exchange defined by method by using
 *  provided private and public keys
 */
SecureSessionKeyExchangeResult tryKeyExchange(SecureSessionParameters& params_out, uint32_t session_id,
                                              const SecureSessionOptions::Method& method,
                                              const std::vector<uint8_t>&         api_user_private_key,
                                              const std::vector<uint8_t>&         api_user_public_key);

std::vector<uint8_t> generateSymmetricEncryptionKey(const KeyExchangeSaltData& salt, const uint8_t* shared_secret,
                                                    uint32_t shared_secret_size);

class SecureSession : public SecureSessionInterface {
public:
    virtual ~SecureSession();
    void generateSymmetricEncryptionKey(const std::string& controller_id_name) override;
    void encrypt(uint8_t* iv, uint8_t* aad, uint32_t aad_len, uint8_t* data, uint32_t data_len, uint8_t* tag) override;

private:
    /** Generate or increment iv if already generated, return iv after
     * TODO: IV generation is not secure with current implementation!
     */
    std::vector<uint8_t> handleIV();

    static constexpr uint8_t k_iv_len  = 12;
    static constexpr uint8_t k_tag_len = 12;

    std::vector<uint8_t> encryption_key_;
    std::vector<uint8_t> iv_;

    gcm_context gcm_ctx;
};

}  // namespace sc_api::core::security

#endif  // SECURITY_IMPL_H
