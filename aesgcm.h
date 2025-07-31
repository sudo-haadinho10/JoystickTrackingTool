#ifndef AES_GCM_API_H
#define AES_GCM_API_H



#include <stddef.h>
#include <stdint.h>

int aes_gcm_encrypt(const unsigned char *key,
		    const unsigned char *iv,
		   // const unsigned char *aad,
		  //  size_t aad_len,
		    const unsigned char *plaintext,
		    size_t plaintext_len,
		    unsigned char *ciphertext,
		    unsigned char *tag
);

int aes_gcm_decrypt(const unsigned char *key,
		    const unsigned char *iv,
		    //const unsigned char *aad,
		    //size_t aad_len,
		    const unsigned char *ciphertext,
		    size_t ciphertext_len,
		    const unsigned char *tag,
		    unsigned char *plaintext);

#endif //AES_GCM_API_H_
