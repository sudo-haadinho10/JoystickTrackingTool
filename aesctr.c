#include "aesctr.h"


#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>


static OSSL_LIB_CTX *libctx = NULL;
static const char *propq = NULL;


int aes_ctr_encrypt(const unsigned char *key,
                    const unsigned char *iv,
                    const unsigned char *plaintext,
                    size_t plaintext_len,
                    unsigned char *ciphertext
)
{
	EVP_CIPHER_CTX *ctx=NULL;
	EVP_CIPHER *cipher = NULL;
	int len = 0;
	int ciphertext_len = 0;
	size_t gcm_ivlen =16; //16 bytes
	int ret=-1;

        OSSL_PARAM params[2] = {OSSL_PARAM_END,OSSL_PARAM_END};

	/* Create a context for the encrypt operation */
	if((ctx = EVP_CIPHER_CTX_new()) == NULL)
		goto err;

	/* Fetch the cipher implementation */
	if((cipher = EVP_CIPHER_fetch(libctx,"AES-256-CTR",propq)) == NULL)
		goto err;

	/* SET IV length. */
	params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_IVLEN,&gcm_ivlen);

	/*Initialize the encryption operation */
	if(!EVP_EncryptInit_ex2(ctx,cipher,(const unsigned char *)key,iv,params))
		goto err;

/* Provide the message to be encrypted and obtain the encrypted output */
        if(!EVP_EncryptUpdate(ctx,ciphertext,&len,plaintext,plaintext_len))
                goto err;
        ciphertext_len = len;

        /* Finalize the encryption */
        if(!EVP_EncryptFinal_ex(ctx,ciphertext + len,&len))
                goto err;
        ciphertext_len +=len;

if(!EVP_CIPHER_CTX_get_params(ctx,params))
                goto err;

        ret = ciphertext_len;

err:
        if (ret<0)
                ERR_print_errors_fp(stderr);
        EVP_CIPHER_free(cipher);
        EVP_CIPHER_CTX_free(ctx);
        return ret;
}


int aes_ctr_decrypt(const unsigned char *key,
                    const unsigned char *iv,
                    const unsigned char *ciphertext,
                    size_t ciphertext_len,
                    unsigned char *plaintext)
{

	EVP_CIPHER_CTX *ctx = NULL;
	EVP_CIPHER *cipher = NULL;
	int len = 0 ;
	int plaintext_len = 0;
	int rv;
	size_t gcm_ivlen = 16;
	OSSL_PARAM params[2] = {OSSL_PARAM_END,OSSL_PARAM_END };
	int ret = -1; //Default to failure

	if((ctx = EVP_CIPHER_CTX_new()) == NULL)
		goto err;

	if((cipher = EVP_CIPHER_fetch(libctx,"AES-256-CTR",propq)) == NULL)
		goto err;

	params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_IVLEN,&gcm_ivlen);

	/* Initialize the decryption operation. */
	if(!EVP_DecryptInit_ex2(ctx,cipher,(const unsigned char *)key,iv,params))
		goto err;

	/* Provide ant AAD data. */
	//if (aad !=NULL && !EVP_DecryptUpdate(ctx,NULL,&len,aad,aad_len))
	//	goto err;

	/*
	 * Provide the message to be decrypted and obtain the plaintext output
	 * The tag verification happens at the end
	*/

	if (!EVP_DecryptUpdate(ctx,plaintext,&len,ciphertext,ciphertext_len))
		goto err;
	plaintext_len = len;

	/* Set the expected tag value, this must be done before finalizing. */
	//params[0] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,(void*)tag,16);

	if(!EVP_CIPHER_CTX_set_params(ctx,params))
		goto err;

	/* Finalize the decryption. A successfull finalzation call means the tag was successfully verified and the plaintext is authentic */

	rv = EVP_DecryptFinal_ex(ctx,plaintext + len,&len);

	/* The return value of EVP_DecryptFinal_ex is >0 on success */

	if(rv > 0) {
		plaintext_len +=len;
		ret = plaintext_len;
	}

	err:
		if (ret < 0)
			ERR_print_errors_fp(stderr); //On failure, print OpenSSL errors
		EVP_CIPHER_free(cipher);
		EVP_CIPHER_CTX_free(ctx);
		return ret;
}


