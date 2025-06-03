#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "libeddsa/eddsa.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        return -1;
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) return -2;

    uint8_t buf[32 * 7 + 64 * 2];
    uint8_t secret1[32];
    uint8_t secret2[32];

    uint8_t ed_pub_buf[32];
    uint8_t ed_sig1_buf[64];
    uint8_t ed_sig2_buf[64];

    int success = 1;
    while (fread(buf, 1, sizeof(buf), f) == sizeof(buf)) {
        const uint8_t* priv1  = &buf[0];
        const uint8_t* pub1   = &buf[32ll * 1];
        const uint8_t* priv2  = &buf[32ll * 2];
        const uint8_t* pub2   = &buf[32ll * 3];
        const uint8_t* secret = &buf[32ll * 4];
        const uint8_t* ed_priv  = &buf[32ll * 5];
        const uint8_t* ed_pub   = &buf[32ll * 6];
        const uint8_t* pub1_sig = &buf[32ll * 7];
        const uint8_t* pub2_sig = &buf[32ll * 9];

        x25519(secret1, priv1, pub2);
        x25519(secret2, priv2, pub1);

        if (memcmp(secret1, secret2, sizeof(secret1)) != 0) {
            printf("%s: Generated shared secrets don't match.\n", argv[1]);
            success = 0;
        }
        if (memcmp(secret1, secret, sizeof(secret1)) != 0) {
            printf("%s: Generated shared secret doesn't match the reference implementation\n", argv[1]);
            success = 0;
        }

        // Ed25519 signature verification
        ed25519_genpub(ed_pub_buf, ed_priv);
        if (memcmp(ed_pub_buf, ed_pub, sizeof(ed_pub_buf)) != 0) {
            printf("%s: Ed25519 public key doesn't match reference\n", argv[1]);
            success = 0;
        }

        ed25519_sign(ed_sig1_buf, ed_priv, ed_pub, pub1, 32);
        ed25519_sign(ed_sig2_buf, ed_priv, ed_pub, pub2, 32);

        if (memcmp(ed_sig1_buf, pub1_sig, 64) != 0) {
            printf("%s: Ed25519 pub1 signature doens't match reference\n", argv[1]);
            success = 0;
        }

        if (memcmp(ed_sig1_buf, pub1_sig, 64) != 0) {
            printf("%s: Ed25519 pub2 signature doens't match reference\n", argv[1]);
            success = 0;
        }

        if (!ed25519_verify(pub1_sig, ed_pub, pub1, 32)) {
            printf("%s: Ed25519 pub1 signature verification failed\n", argv[1]);
            success = 0;
        }

        if (!ed25519_verify(pub2_sig, ed_pub, pub2, 32)) {
            printf("%s: Ed25519 pub2 signature verification failed\n", argv[1]);
            success = 0;
        }
    }

    fclose(f);
    if (success) {
        return 0;
    }

    return 1;
}
