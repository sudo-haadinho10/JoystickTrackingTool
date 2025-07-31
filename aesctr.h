
#ifndef AES_CTR_API_H
#define AES_CTR_API_H


#include <openssl/aes.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

int aes_ctr_encrypt(const unsigned char *key,
		    const unsigned char *iv,
		    const unsigned char *plaintext,
		    size_t plaintext_len,
		    unsigned char *ciphertext
);

int aes_ctr_decrypt(const unsigned char *key,
                    const unsigned char *iv,
                    const unsigned char *ciphertext,
                    size_t ciphertext_len,
                    unsigned char *plaintext);

#endif
