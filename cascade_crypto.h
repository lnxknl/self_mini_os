#ifndef CASCADE_CRYPTO_H
#define CASCADE_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// Key structure for CascadeX encryption
typedef struct {
    uint8_t primary_key[32];   // Primary encryption key
    uint8_t tweak_key[16];     // Tweak key for block modification
    uint32_t rounds;           // Number of encryption rounds
    uint32_t seed;             // Seed for pseudo-random operations
} cascade_key_t;

// Block operation modes
typedef enum {
    CASCADE_MODE_ECB = 0,  // Electronic Code Book
    CASCADE_MODE_CBC = 1,  // Cipher Block Chaining
    CASCADE_MODE_CTR = 2,  // Counter Mode
    CASCADE_MODE_XTS = 3   // XEX with Tweaking for each Sector
} cascade_mode_t;

// Error codes
typedef enum {
    CASCADE_SUCCESS = 0,
    CASCADE_ERROR_INVALID_KEY = -1,
    CASCADE_ERROR_INVALID_INPUT = -2,
    CASCADE_ERROR_BUFFER_TOO_SMALL = -3,
    CASCADE_ERROR_INVALID_MODE = -4
} cascade_error_t;

// Initialize encryption key with custom parameters
cascade_error_t cascade_init_key(
    cascade_key_t* key,
    const uint8_t* primary_key,
    size_t primary_key_len,
    const uint8_t* tweak_key,
    size_t tweak_key_len,
    uint32_t rounds,
    uint32_t seed
);

// Encrypt data using CascadeX algorithm
cascade_error_t cascade_encrypt(
    const cascade_key_t* key,
    cascade_mode_t mode,
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t output_size,
    uint8_t* iv,          // Initialization vector (16 bytes)
    uint64_t sector_num   // Sector number for XTS mode
);

// Decrypt data using CascadeX algorithm
cascade_error_t cascade_decrypt(
    const cascade_key_t* key,
    cascade_mode_t mode,
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t output_size,
    uint8_t* iv,          // Initialization vector (16 bytes)
    uint64_t sector_num   // Sector number for XTS mode
);

// Generate a random key suitable for CascadeX encryption
cascade_error_t cascade_generate_key(
    cascade_key_t* key,
    uint32_t rounds
);

// Key derivation function to create encryption key from password
cascade_error_t cascade_derive_key(
    cascade_key_t* key,
    const char* password,
    size_t password_len,
    const uint8_t* salt,
    size_t salt_len,
    uint32_t rounds
);

// Utility functions
void cascade_xor_blocks(uint8_t* dst, const uint8_t* src, size_t len);
void cascade_increment_counter(uint8_t* counter, size_t len);
void cascade_generate_tweak(uint8_t* tweak, const uint8_t* key, uint64_t sector_num);

#endif // CASCADE_CRYPTO_H
