#ifndef EDDSA_H
#define EDDSA_H

#include <stddef.h>	/* for size_t */
#include <stdbool.h>	/* foor bool */
#include <stdint.h>	/* for uint8_t */

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifdef __cplusplus
extern "C" {
#endif



/*
 * Ed25519 DSA
 */

#define ED25519_KEY_LEN		32
#define ED25519_SIG_LEN		64

void ed25519_genpub(uint8_t pub[ED25519_KEY_LEN], const uint8_t sec[ED25519_KEY_LEN]);

void ed25519_sign(uint8_t sig[ED25519_SIG_LEN], const uint8_t sec[ED25519_KEY_LEN], const uint8_t pub[ED25519_KEY_LEN],
                  const uint8_t* data, size_t len);

bool ed25519_verify(const uint8_t sig[ED25519_SIG_LEN], const uint8_t pub[ED25519_KEY_LEN], const uint8_t* data,
                    size_t len);

/*
 * X25519 Diffie-Hellman
 */

#define X25519_KEY_LEN		32

void x25519_base(uint8_t out[X25519_KEY_LEN], const uint8_t scalar[X25519_KEY_LEN]);

void x25519(uint8_t out[X25519_KEY_LEN], const uint8_t scalar[X25519_KEY_LEN], const uint8_t point[X25519_KEY_LEN]);

/*
 * Key-conversion between ed25519 and x25519
 */

void pk_ed25519_to_x25519(uint8_t out[X25519_KEY_LEN], const uint8_t in[ED25519_KEY_LEN]);

void sk_ed25519_to_x25519(uint8_t out[X25519_KEY_LEN], const uint8_t in[ED25519_KEY_LEN]);

#ifdef __cplusplus
}
#endif


#endif
