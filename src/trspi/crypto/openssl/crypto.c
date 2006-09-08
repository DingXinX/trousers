
/*
 * Licensed Materials - Property of IBM
 *
 * trousers - An open source TCG Software Stack
 *
 * (C) Copyright International Business Machines Corp. 2004-2006
 *
 */

/*
 * crypto.c - openssl TSS crypto routines
 *
 * Kent Yoder <shpedoikal@gmail.com>
 *
 */

#include <string.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "trousers/tss.h"
#include "trousers/trousers.h"
#include "spi_internal_types.h"
#include "spi_utils.h"
#include "tsplog.h"


/*
 * Hopefully this will make the code clearer since
 * OpenSSL returns 1 on success
 */
#define EVP_SUCCESS 1

TSS_RESULT
Trspi_Hash(UINT32 HashType, UINT32 BufSize, BYTE* Buf, BYTE* Digest)
{
	EVP_MD_CTX md_ctx;
	unsigned int result_size;
	int rv;

	switch (HashType) {
		case TSS_HASH_SHA1:
			rv = EVP_DigestInit(&md_ctx, EVP_sha1());
			break;
		default:
			rv = TSPERR(TSS_E_BAD_PARAMETER);
			goto out;
			break;
	}

	if (rv != EVP_SUCCESS) {
		rv = TSPERR(TSS_E_INTERNAL_ERROR);
		goto err;
	}

	rv = EVP_DigestUpdate(&md_ctx, Buf, BufSize);
	if (rv != EVP_SUCCESS) {
		rv = TSPERR(TSS_E_INTERNAL_ERROR);
		goto err;
	}

	result_size = EVP_MD_CTX_size(&md_ctx);
	rv = EVP_DigestFinal(&md_ctx, Digest, &result_size);
	if (rv != EVP_SUCCESS) {
		rv = TSPERR(TSS_E_INTERNAL_ERROR);
		goto err;
	} else
		rv = TSS_SUCCESS;

	goto out;

err:
	DEBUG_print_openssl_errors();
out:
        return rv;
}

UINT32
Trspi_HMAC(UINT32 HashType, UINT32 SecretSize, BYTE* Secret, UINT32 BufSize, BYTE* Buf, BYTE* hmacOut)
{
	/*HMAC_CTX hmac_ctx;*/
	const EVP_MD *md;
	unsigned int len;
	int rv = TSS_SUCCESS;

	switch (HashType) {
		case TSS_HASH_SHA1:
			md = EVP_sha1();
			break;
		default:
			rv = TSPERR(TSS_E_BAD_PARAMETER);
			goto out;
			break;
	}

	len = EVP_MD_size(md);

	HMAC(md, Secret, SecretSize, Buf, BufSize, hmacOut, &len);
out:
	return rv;
}

/* XXX int set to unsigned int values */
int
Trspi_RSA_Encrypt(unsigned char *dataToEncrypt, /* in */
		unsigned int dataToEncryptLen,  /* in */
		unsigned char *encryptedData,   /* out */
		unsigned int *encryptedDataLen, /* out */
		unsigned char *publicKey,
		unsigned int keysize)
{
	int rv;
	unsigned char exp[] = { 0x01, 0x00, 0x01 }; /* 65537 hex */
	unsigned char oaepPad[] = "TCPA";
	int oaepPadLen = 4;
	RSA *rsa = RSA_new();
	BYTE encodedData[256];
	int encodedDataLen;

	if (rsa == NULL) {
		rv = TSPERR(TSS_E_OUTOFMEMORY);
		goto err;
	}

	/* set the public key value in the OpenSSL object */
	rsa->n = BN_bin2bn(publicKey, keysize, rsa->n);
	/* set the public exponent */
	rsa->e = BN_bin2bn(exp, sizeof(exp), rsa->e);

	if (rsa->n == NULL || rsa->e == NULL) {
		rv = TSPERR(TSS_E_OUTOFMEMORY);
		goto err;
	}

	/* padding constraint for PKCS#1 OAEP padding */
	if ((int)dataToEncryptLen >= (RSA_size(rsa) - ((2 * SHA_DIGEST_LENGTH) + 1))) {
		rv = TSPERR(TSS_E_INTERNAL_ERROR);
		goto err;
	}

	encodedDataLen = MIN(RSA_size(rsa), 256);

	/* perform our OAEP padding here with custom padding parameter */
	rv = RSA_padding_add_PKCS1_OAEP(encodedData, encodedDataLen, dataToEncrypt,
			dataToEncryptLen, oaepPad, oaepPadLen);
	if (rv != EVP_SUCCESS) {
		rv = TSPERR(TSS_E_INTERNAL_ERROR);
		goto err;
	}

	/* call OpenSSL with no additional padding */
	rv = RSA_public_encrypt(encodedDataLen, encodedData,
				encryptedData, rsa, RSA_NO_PADDING);
	if (rv == -1) {
		rv = TSPERR(TSS_E_INTERNAL_ERROR);
		goto err;
	}

	/* RSA_public_encrypt returns the size of the encrypted data */
	*encryptedDataLen = rv;
	rv = TSS_SUCCESS;
	goto out;

err:
	DEBUG_print_openssl_errors();
out:
	if (rsa)
		RSA_free(rsa);
        return rv;
}

TSS_RESULT
Trspi_Verify(UINT32 HashType, BYTE *pHash, UINT32 iHashLength,
	     unsigned char *pModulus, int iKeyLength,
	     BYTE *pSignature, UINT32 sig_len)
{
	int rv, nid;
	unsigned char exp[] = { 0x01, 0x00, 0x01 }; /* The default public exponent for the TPM */
	unsigned char buf[256];
	RSA *rsa = RSA_new();

	if (rsa == NULL) {
		rv = TSPERR(TSS_E_OUTOFMEMORY);
		goto err;
	}

	/* We assume we're verifying data from a TPM, so there are only
	 * two options, SHA1 data and PKCSv1.5 encoded signature data.
	 */
	switch (HashType) {
		case TSS_HASH_SHA1:
			nid = NID_sha1;
			break;
		case TSS_HASH_OTHER:
			nid = NID_undef;
			break;
		default:
			rv = TSPERR(TSS_E_BAD_PARAMETER);
			goto out;
			break;
	}

	/* set the public key value in the OpenSSL object */
	rsa->n = BN_bin2bn(pModulus, iKeyLength, rsa->n);
	/* set the public exponent */
	rsa->e = BN_bin2bn(exp, sizeof(exp), rsa->e);

	if (rsa->n == NULL || rsa->e == NULL) {
		rv = TSPERR(TSS_E_OUTOFMEMORY);
		goto err;
	}

	/* if we don't know the structure of the data we're verifying, do a public decrypt
	 * and compare manually. If we know we're looking for a SHA1 hash, allow OpenSSL
	 * to do the work for us.
	 */
	if (nid == NID_undef) {
		rv = RSA_public_decrypt(sig_len, pSignature, buf, rsa, RSA_PKCS1_PADDING);
		if ((UINT32)rv != iHashLength) {
			rv = TSPERR(TSS_E_FAIL);
			goto out;
		} else if (memcmp(pHash, buf, iHashLength)) {
			rv = TSPERR(TSS_E_FAIL);
			goto out;
		}
	} else {
		if ((rv = RSA_verify(nid, pHash, iHashLength, pSignature, sig_len, rsa)) == 0) {
			rv = TSPERR(TSS_E_FAIL);
			goto out;
		}
	}

	rv = TSS_SUCCESS;
	goto out;

err:
	DEBUG_print_openssl_errors();
out:
	if (rsa)
		RSA_free(rsa);
        return rv;
}

int
Trspi_RSA_Public_Encrypt(unsigned char *in, unsigned int inlen,
			 unsigned char *out, unsigned int *outlen,
			 unsigned char *pubkey, unsigned int pubsize,
			 unsigned int e, int padding)
{
	int rv, e_size = 3;
	unsigned char exp[] = { 0x01, 0x00, 0x01 };
	RSA *rsa = RSA_new();

	if (rsa == NULL) {
		rv = TSPERR(TSS_E_OUTOFMEMORY);
		goto err;
	}

	switch (e) {
		case 0:
			/* fall through */
		case 65537:
			break;
		case 17:
			exp[0] = 17;
			e_size = 1;
			break;
		case 3:
			exp[0] = 3;
			e_size = 1;
			break;
		default:
			rv = TSPERR(TSS_E_INTERNAL_ERROR);
			goto out;
			break;
	}

	/* set the public key value in the OpenSSL object */
	rsa->n = BN_bin2bn(pubkey, pubsize, rsa->n);
	/* set the public exponent */
	rsa->e = BN_bin2bn(exp, e_size, rsa->e);

	if (rsa->n == NULL || rsa->e == NULL) {
		rv = TSPERR(TSS_E_OUTOFMEMORY);
		goto err;
	}

	switch (padding) {
		case RSA_PKCS1_PADDING:
		case RSA_PKCS1_OAEP_PADDING:
		case RSA_NO_PADDING:
			break;
		default:
			rv = TSPERR(TSS_E_INTERNAL_ERROR);
			goto out;
			break;
	}

	rv = RSA_public_encrypt(inlen, in, out, rsa, padding);
	if (rv == -1) {
		rv = TSPERR(TSS_E_INTERNAL_ERROR);
		goto err;
	}

	/* RSA_public_encrypt returns the size of the encrypted data */
	*outlen = rv;
	rv = TSS_SUCCESS;
	goto out;

err:
	DEBUG_print_openssl_errors();
out:
	if (rsa)
		RSA_free(rsa);
        return rv;
}

TSS_RESULT
Trspi_Encrypt_ECB(UINT16 alg, BYTE *key, BYTE *in, UINT32 in_len, BYTE *out,
		  UINT32 *out_len)
{
	TSS_RESULT result = TSS_SUCCESS;
	EVP_CIPHER_CTX ctx;
	UINT32 tmp;

	switch (alg) {
		case TSS_ALG_AES:
			break;
		default:
			result = TSPERR(TSS_E_INTERNAL_ERROR);
			goto done;
			break;
	}

	EVP_CIPHER_CTX_init(&ctx);

	if (!EVP_EncryptInit(&ctx, EVP_aes_256_ecb(), key, NULL)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (*out_len < in_len + EVP_CIPHER_CTX_block_size(&ctx) - 1) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		goto done;
	}

	if (!EVP_EncryptUpdate(&ctx, out, (int *)out_len, in, in_len)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (!EVP_EncryptFinal(&ctx, out + *out_len, (int *)&tmp)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}
	*out_len += tmp;
done:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return result;
}

TSS_RESULT
Trspi_Decrypt_ECB(UINT16 alg, BYTE *key, BYTE *in, UINT32 in_len, BYTE *out,
		  UINT32 *out_len)
{
	TSS_RESULT result = TSS_SUCCESS;
	EVP_CIPHER_CTX ctx;
	UINT32 tmp;

	switch (alg) {
		case TSS_ALG_AES:
			break;
		default:
			result = TSPERR(TSS_E_INTERNAL_ERROR);
			goto done;
			break;
	}

	EVP_CIPHER_CTX_init(&ctx);

	if (!EVP_DecryptInit(&ctx, EVP_aes_256_ecb(), key, NULL)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (!EVP_DecryptUpdate(&ctx, out, (int *)out_len, in, in_len)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (!EVP_DecryptFinal(&ctx, out + *out_len, (int *)&tmp)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}
	*out_len += tmp;
done:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return result;
}

TSS_RESULT
Trspi_SymEncrypt(UINT16 alg, BYTE mode, BYTE *key, BYTE *iv, BYTE *in, UINT32 in_len, BYTE *out,
		 UINT32 *out_len)
{
	TSS_RESULT result = TSS_SUCCESS;
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER *cipher;
	UINT32 tmp;

	/* TPM 1.1 had no defines for symmetric encryption modes, must use CBC */
	if (mode != TR_SYM_MODE_CBC && mode != TCPA_ES_NONE) {
		LogDebug("Invalid mode in doing symmetric encryption");
		return TSPERR(TSS_E_INTERNAL_ERROR);
	}

	switch (alg) {
		case TSS_ALG_AES:
		case TCPA_ALG_AES:
			cipher = (EVP_CIPHER *)EVP_aes_128_cbc();
			break;
		case TSS_ALG_DES:
		case TCPA_ALG_DES:
			cipher = (EVP_CIPHER *)EVP_des_cbc();
			break;
		case TSS_ALG_3DES:
		case TCPA_ALG_3DES:
			cipher = (EVP_CIPHER *)EVP_des_ede3();
			break;
		default:
			return TSPERR(TSS_E_INTERNAL_ERROR);
			break;
	}

	EVP_CIPHER_CTX_init(&ctx);

	if (!EVP_EncryptInit(&ctx, (const EVP_CIPHER *)cipher, key, iv)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (*out_len < in_len + EVP_CIPHER_CTX_block_size(&ctx) - 1) {
		LogDebug("No enough space to do symmetric encryption");
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		goto done;
	}

	if (!EVP_EncryptUpdate(&ctx, out, (int *)out_len, in, in_len)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (!EVP_EncryptFinal(&ctx, out + *out_len, (int *)&tmp)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}
	*out_len += tmp;
done:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return result;
}

TSS_RESULT
Trspi_SymDecrypt(UINT16 alg, BYTE mode, BYTE *key, BYTE *iv, BYTE *in, UINT32 in_len, BYTE *out,
		 UINT32 *out_len)
{
	TSS_RESULT result = TSS_SUCCESS;
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER *cipher;
	UINT32 tmp;

	/* TPM 1.1 had no defines for symmetric encryption modes, must use CBC */
	if (mode != TR_SYM_MODE_CBC && mode != TCPA_ES_NONE) {
		LogDebug("Invalid mode in doing symmetric decryption");
		return TSPERR(TSS_E_INTERNAL_ERROR);
	}

	switch (alg) {
		case TSS_ALG_AES:
		case TCPA_ALG_AES:
			cipher = (EVP_CIPHER *)EVP_aes_128_cbc();
			break;
		case TSS_ALG_DES:
		case TCPA_ALG_DES:
			cipher = (EVP_CIPHER *)EVP_des_cbc();
			break;
		case TSS_ALG_3DES:
		case TCPA_ALG_3DES:
			cipher = (EVP_CIPHER *)EVP_des_ede3();
			break;
		default:
			return TSPERR(TSS_E_INTERNAL_ERROR);
			break;
	}

	EVP_CIPHER_CTX_init(&ctx);

	if (!EVP_DecryptInit(&ctx, cipher, key, iv)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (!EVP_DecryptUpdate(&ctx, out, (int *)out_len, in, in_len)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}

	if (!EVP_DecryptFinal(&ctx, out + *out_len, (int *)&tmp)) {
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		DEBUG_print_openssl_errors();
		goto done;
	}
	*out_len += tmp;
done:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return result;
}
