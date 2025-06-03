#include "security_impl.h"

#include <cstring>
#include <random>

#include "libeddsa/eddsa.h"
#include "libeddsa/sha512.h"
#include "sc-api/core/protocol/security.h"

namespace sc_api::core::security {

SecureSessionKeyExchangeResult tryKeyExchange(SecureSessionParameters& params_out, uint32_t session_id,
                                              const SecureSessionOptions::Method& method,
                                              const std::vector<uint8_t>&         api_user_private_key,
                                              const std::vector<uint8_t>&         api_user_public_key) {
    if (method.method != SC_API_PROTOCOL_SECURITY_METHOD_X25519_AES128_GCM) {
        return SecureSessionKeyExchangeResult::error_not_supported;
    }

    if (method.public_key.size() != X25519_KEY_LEN) {
        return SecureSessionKeyExchangeResult::error_not_supported;
    }

    if (api_user_private_key.size() != X25519_KEY_LEN) {
        return SecureSessionKeyExchangeResult::error_invalid_private_key;
    }
    if (api_user_public_key.size() != X25519_KEY_LEN) {
        return SecureSessionKeyExchangeResult::error_invalid_public_key;
    }

    if (method.signature.size() != ED25519_SIG_LEN) {
        return SecureSessionKeyExchangeResult::error_signature_verification_failed;
    }

    if (!ed25519_verify(method.signature.data(), SC_API_PROTOCOL_ed25519_signature_public_key, method.public_key.data(),
                        method.public_key.size())) {
        return SecureSessionKeyExchangeResult::error_signature_verification_failed;
    }

    params_out.shared_secret.resize(X25519_KEY_LEN);
    x25519(params_out.shared_secret.data(), api_user_private_key.data(), method.public_key.data());
    params_out.controller_public_key = api_user_public_key;
    params_out.method                = method.method;
    params_out.session_id            = session_id;
    return SecureSessionKeyExchangeResult::ok;
}

std::vector<uint8_t> generateSymmetricEncryptionKey(const KeyExchangeSaltData& salt_params,
                                                    const uint8_t* shared_secret, uint32_t shared_secret_size) {
    // Shared secret shouldn't be used as the encryption key directly so lets do some hashing with salt
    // Lets use sha512 even if we only need 16 bytes, because it comes with libeddsa and avoids extra dependencies in
    // sc-api
    uint8_t salt[8];
    salt[0] = (salt_params.api_session_id >> 0) & 0xff;
    salt[1] = (salt_params.api_session_id >> 8) & 0xff;
    salt[2] = (salt_params.api_session_id >> 16) & 0xff;
    salt[3] = (salt_params.api_session_id >> 24) & 0xff;
    salt[4] = 0x54;
    salt[5] = 0x5f;
    salt[6] = 0x52;
    salt[7] = 0x59;

    std::vector<uint8_t> symmetric_encrypt_key_buf(64);

    sha512 ctx;
    sha512_init(&ctx);
    sha512_add(&ctx, salt, sizeof(salt));
    sha512_add(&ctx, (const uint8_t*)salt_params.controller_id_name.data(), salt_params.controller_id_name.size());
    sha512_add(&ctx, shared_secret, shared_secret_size);
    sha512_final(&ctx, symmetric_encrypt_key_buf.data());

    symmetric_encrypt_key_buf.resize(16);
    return symmetric_encrypt_key_buf;
}

SecureSession::~SecureSession() {}

void SecureSession::generateSymmetricEncryptionKey(const std::string& controller_id_name) {
    encryption_key_ = security::generateSymmetricEncryptionKey({secure_session_params_.session_id, controller_id_name},
                                                               secure_session_params_.shared_secret.data(),
                                                               (uint32_t)secure_session_params_.shared_secret.size());

    SC_API_gcm_setkey(&gcm_ctx, encryption_key_.data(), (uint32_t)encryption_key_.size());
}

void SecureSession::encrypt(uint8_t* iv, uint8_t* aad, uint32_t aad_len, uint8_t* data, uint32_t data_len,
                            uint8_t* tag) {
    std::memcpy(iv, handleIV().data(), k_iv_len);
    SC_API_gcm_start(&gcm_ctx, 1, iv, k_iv_len, aad, aad_len);
    SC_API_gcm_update(&gcm_ctx, data_len, data, data);
    SC_API_gcm_finish(&gcm_ctx, tag, k_tag_len);
}

std::vector<uint8_t> SecureSession::handleIV() {
    if (!iv_.empty()) {
        for (std::size_t i = 0; i < iv_.size(); i++) {
            iv_[i]++;
            if (iv_[i] != 0) {
                break;
            }
        }
    } else {
        // TODO: IV generation is not secure with current implementation!
        std::random_device                                       dev;
        std::mt19937                                             rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0x00, 0xff);

        for (int i = 0; i < k_iv_len; i++) {
            iv_.push_back(dist(rng));
        }
    }

    return iv_;
}

}  // namespace sc_api::core::security
