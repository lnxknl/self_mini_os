#include "cascade_crypto.h"
#include <stdio.h>
#include <string.h>

void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main() {
    // Test data
    const char* test_data = "Hello, CascadeX Encryption!";
    size_t data_len = strlen(test_data);
    
    // Pad data to block size
    size_t padded_len = ((data_len + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    uint8_t* input = (uint8_t*)calloc(padded_len, 1);
    uint8_t* output = (uint8_t*)calloc(padded_len, 1);
    uint8_t* decrypted = (uint8_t*)calloc(padded_len, 1);
    
    memcpy(input, test_data, data_len);
    
    // Generate encryption key
    cascade_key_t key;
    cascade_generate_key(&key, 12);  // 12 rounds
    
    // Generate IV for CBC mode
    uint8_t iv[16] = {0};
    for (int i = 0; i < 16; i++) {
        iv[i] = rand() & 0xFF;
    }
    
    printf("Original data: %s\n", test_data);
    printf("Original hex: ");
    print_hex(input, padded_len);
    
    // Encrypt using CBC mode
    cascade_error_t result = cascade_encrypt(&key, CASCADE_MODE_CBC, input, padded_len, 
                                           output, padded_len, iv, 0);
    
    if (result == CASCADE_SUCCESS) {
        printf("Encrypted hex: ");
        print_hex(output, padded_len);
        
        // Decrypt
        result = cascade_decrypt(&key, CASCADE_MODE_CBC, output, padded_len,
                               decrypted, padded_len, iv, 0);
        
        if (result == CASCADE_SUCCESS) {
            printf("Decrypted: %s\n", decrypted);
            printf("Decrypted hex: ");
            print_hex(decrypted, padded_len);
        } else {
            printf("Decryption failed!\n");
        }
    } else {
        printf("Encryption failed!\n");
    }
    
    free(input);
    free(output);
    free(decrypted);
    
    return 0;
}
