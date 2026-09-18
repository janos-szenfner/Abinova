/* Copyright (C) 2010 Marc Maurer <uwog@uwog.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301 USA.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__linux__) || defined(__GLIBC__)
#include <sys/random.h>
#endif

#include <zlib.h>
#include <glib.h>

#include <gsf/gsf.h>

#include "sha1.h"
#include "gc-pbkdf2-sha1.h"

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ODc_Crypto.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GCRYPT
#include "gcrypt.h"
#else
#include "blowfish/blowfish.h"
#endif

#define PASSWORD_HASH_LEN 20
#define PBKDF2_KEYLEN 16

#define HANDLEGERR(e)                           \
    {                                           \
    gcry_err_code_t code = gcry_err_code(e);    \
    if( code != GPG_ERR_NO_ERROR )              \
    {                                           \
        switch(code)                            \
        {                                       \
            case GPG_ERR_ENOMEM:                \
                return UT_OUTOFMEM;             \
            case GPG_ERR_DECRYPT_FAILED:        \
                return UT_IE_PROTECTED;         \
            default:                            \
                return UT_ERROR;                \
        }                                       \
    }                                           \
    }



UT_Error ODc_Crypto::performDecrypt(GsfInput* pStream, 
                                    unsigned char* salt, UT_uint32 salt_length, UT_uint32 iter_count,
                                    unsigned char* ivec, gsize ivec_length, const std::string& password, UT_uint32 decrypted_size,
                                    GsfInput** pDecryptedInput)
{
	unsigned char sha1_password[PASSWORD_HASH_LEN];
	char key[PBKDF2_KEYLEN];

	// get the sha1 sum of the password
	sha1_buffer(&password[0], password.size(), sha1_password);

	// create a PBKDF2 key from the sha1 sum
	int k = pbkdf2_sha1 ((const char*)sha1_password, PASSWORD_HASH_LEN, (const char*)salt, salt_length, iter_count, key, PBKDF2_KEYLEN);
	if (k != 0)
		return UT_ERROR;

    // Get the encrypted content ready
	UT_sint32 content_size = gsf_input_size(pStream); 
	if (content_size == -1)
		return UT_ERROR;
	const unsigned char* content = gsf_input_read(pStream, content_size, nullptr);
	if (!content)
		return UT_ERROR;

	unsigned char* content_decrypted = (unsigned char*)g_malloc(content_size);
    
	// perform the actual decryption
#ifdef HAVE_GCRYPT

    UT_DEBUGMSG(("ODc_Crypto::performDecrypt() using gcrypt\n" ));

    gcry_cipher_hd_t h;
    HANDLEGERR( gcry_cipher_open( &h,
                                  GCRY_CIPHER_BLOWFISH,
                                  GCRY_CIPHER_MODE_CFB,
                                  0 ));
    HANDLEGERR( gcry_cipher_setkey( h, key, PBKDF2_KEYLEN ));
    HANDLEGERR( gcry_cipher_setiv ( h, ivec, ivec_length ));
    HANDLEGERR( gcry_cipher_decrypt( h,
                                     content_decrypted,
                                     content_size,
                                     content,
                                     content_size ));
    gcry_cipher_close( h );


#else

    // In-tree Blowfish implementation (Eric Young's, formerly in the
    // opendocument plugin); gives CFB64, which is what ODF encrypted
    // streams use.
    int num = 0;
    unsigned char ivec_copy[8] = {0};
    memcpy(ivec_copy, ivec, ivec_length > 8 ? 8 : ivec_length);

    BF_KEY bf_key;
    BF_set_key(&bf_key, PBKDF2_KEYLEN, (const unsigned char*)key);
    BF_cfb64_encrypt(content, content_decrypted, content_size,
                     &bf_key, ivec_copy, &num, BF_DECRYPT);

#endif
    
	// deflate the decrypted content
	z_stream zs;
	zs.zalloc = nullptr;
	zs.zfree = nullptr;
	zs.opaque = nullptr;
	zs.avail_in = 0;
	zs.next_in = nullptr;

	int err;
	err = inflateInit2(&zs, -MAX_WBITS);
	if (err != Z_OK)
		return UT_ERROR;

	unsigned char* decrypted = (unsigned char*)g_malloc(decrypted_size);
	zs.avail_in = content_size;
	zs.avail_out = decrypted_size;
	zs.next_in = content_decrypted;
	zs.next_out = decrypted;

	err = inflate(&zs, Z_FINISH);
	FREEP(content_decrypted);
	
	if (err != Z_STREAM_END)
	{
		inflateEnd(&zs);
		FREEP(decrypted);
		return UT_ERROR;
	}

	inflateEnd(&zs);

	*pDecryptedInput = gsf_input_memory_new(decrypted, decrypted_size, TRUE);
	
	return UT_OK;
}

UT_Error ODc_Crypto::decrypt(GsfInput* pStream, const ODc_CryptoInfo& cryptInfo,
    const std::string& password, GsfInput** pDecryptedInput)
{
	UT_return_val_if_fail(pStream, UT_ERROR);
	UT_return_val_if_fail(pDecryptedInput, UT_ERROR);
	
	// check if we support the requested decryption method
	UT_return_val_if_fail(g_ascii_strcasecmp(cryptInfo.m_algorithm.c_str(), "Blowfish CFB") == 0, UT_ERROR);
	UT_return_val_if_fail(g_ascii_strcasecmp(cryptInfo.m_keyType.c_str(), "PBKDF2") == 0, UT_ERROR);
	
	// base64 decode the salt
	gsize salt_length;
	unsigned char* salt = g_base64_decode(cryptInfo.m_salt.c_str(), &salt_length);

	// base64 decode the initialization vector
	gsize ivec_length;
	unsigned char* ivec = g_base64_decode(cryptInfo.m_initVector.c_str(), &ivec_length);

	// decrypt the content
	UT_Error result = performDecrypt(pStream, salt, salt_length, cryptInfo.m_iterCount,  
                                     ivec, ivec_length, password, cryptInfo.m_decryptedSize, pDecryptedInput);

	// cleanup
	FREEP(salt);
	FREEP(ivec);

	return result;
}

/**
 * Fill @buf with @len cryptographically-suitable random bytes.
 */
static void odRandomBytes(unsigned char* buf, gsize len)
{
#if defined(__linux__) || defined(__GLIBC__)
    gsize done = 0;
    while (done < len)
    {
        ssize_t n = getrandom(buf + done, len - done, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        done += (gsize)n;
    }
    if (done == len)
        return;
    // fall through to the weaker source on failure
#endif
    for (gsize i = 0; i < len; i++)
        buf[i] = (unsigned char)g_random_int_range(0, 256);
}

/**
 * Encrypt a stream for storage inside an ODF package, per the
 * OpenDocument v1.2 Part 3 encryption model:
 *
 *   key        = PBKDF2-SHA1(SHA1(password), salt, iter, 16)
 *   ciphertext = Blowfish-CFB64( raw-deflate(plaintext), key, iv )
 *
 * On success @cryptInfoOut is filled with everything needed to write
 * the manifest:encryption-data entry (base64 salt + iv, iteration
 * count, algorithm name, plaintext size and the SHA1/1K checksum of
 * the plaintext). @encrypted is g_malloc()'d and owned by the caller.
 */
UT_Error ODc_Crypto::encrypt(const guint8* plaintext, gsize plaintextSize,
                             const std::string& password, ODc_CryptoInfo& cryptInfoOut,
                             guint8** encrypted, gsize* encryptedSize)
{
    UT_return_val_if_fail(plaintext, UT_ERROR);
    UT_return_val_if_fail(encrypted, UT_ERROR);
    UT_return_val_if_fail(encryptedSize, UT_ERROR);
    UT_return_val_if_fail(!password.empty(), UT_ERROR);

    // random per-file salt and initialisation vector
    unsigned char salt[16];
    unsigned char ivec[8];
    odRandomBytes(salt, sizeof(salt));
    odRandomBytes(ivec, sizeof(ivec));

    const UT_uint32 iter_count = 1024;

    // key = PBKDF2-SHA1(SHA1(password), salt, iter, 16)
    unsigned char sha1_password[PASSWORD_HASH_LEN];
    char key[PBKDF2_KEYLEN];
    sha1_buffer(&password[0], password.size(), sha1_password);
    int k = pbkdf2_sha1((const char*)sha1_password, PASSWORD_HASH_LEN,
                        (const char*)salt, sizeof(salt), iter_count,
                        key, PBKDF2_KEYLEN);
    if (k != 0)
        return UT_ERROR;

    // SHA1/1K checksum of the plaintext for the manifest
    {
        unsigned char digest[PASSWORD_HASH_LEN];
        gsize n = plaintextSize < 1024 ? plaintextSize : 1024;
        sha1_buffer((const char*)plaintext, n, digest);
        gchar* b64 = g_base64_encode(digest, sizeof(digest));
        cryptInfoOut.m_checksum = b64;
        g_free(b64);
    }

    // raw-deflate the plaintext
    z_stream zs;
    zs.zalloc = nullptr;
    zs.zfree = nullptr;
    zs.opaque = nullptr;
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return UT_ERROR;

    uLongf compBound = deflateBound(&zs, plaintextSize);
    unsigned char* compressed = (unsigned char*)g_malloc(compBound);
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(plaintext));
    zs.avail_in = plaintextSize;
    zs.next_out = compressed;
    zs.avail_out = compBound;

    int err = deflate(&zs, Z_FINISH);
    if (err != Z_STREAM_END)
    {
        deflateEnd(&zs);
        g_free(compressed);
        return UT_ERROR;
    }
    gsize compSize = zs.total_out;
    deflateEnd(&zs);

    // encrypt the compressed stream
    unsigned char* out = (unsigned char*)g_malloc(compSize);

#ifdef HAVE_GCRYPT
    gcry_cipher_hd_t h;
    gcry_err_code_t gerr;
    gerr = gcry_cipher_open(&h, GCRY_CIPHER_BLOWFISH,
                            GCRY_CIPHER_MODE_CFB, 0);
    if (gerr == GPG_ERR_NO_ERROR)
        gerr = gcry_cipher_setkey(h, key, PBKDF2_KEYLEN);
    if (gerr == GPG_ERR_NO_ERROR)
        gerr = gcry_cipher_setiv(h, ivec, sizeof(ivec));
    if (gerr == GPG_ERR_NO_ERROR)
        gerr = gcry_cipher_encrypt(h, out, compSize, compressed, compSize);
    gcry_cipher_close(h);
    if (gerr != GPG_ERR_NO_ERROR)
    {
        g_free(compressed);
        g_free(out);
        return UT_ERROR;
    }
#else
    {
        int num = 0;
        unsigned char ivec_copy[8];
        memcpy(ivec_copy, ivec, sizeof(ivec_copy));
        BF_KEY bf_key;
        BF_set_key(&bf_key, PBKDF2_KEYLEN, (const unsigned char*)key);
        BF_cfb64_encrypt(compressed, out, compSize,
                         &bf_key, ivec_copy, &num, BF_ENCRYPT);
    }
#endif

    g_free(compressed);

    // fill in the manifest information
    {
        gchar* b64;
        b64 = g_base64_encode(salt, sizeof(salt));
        cryptInfoOut.m_salt = b64;
        g_free(b64);
        b64 = g_base64_encode(ivec, sizeof(ivec));
        cryptInfoOut.m_initVector = b64;
        g_free(b64);
    }
    cryptInfoOut.m_algorithm = "Blowfish CFB";
    cryptInfoOut.m_keyType = "PBKDF2";
    cryptInfoOut.m_iterCount = iter_count;
    cryptInfoOut.m_decryptedSize = plaintextSize;

    *encrypted = out;
    *encryptedSize = compSize;
    return UT_OK;
}
