/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2025-2026 Abinova contributors
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

#include "ut_abwncrypt.h"
#include "ut_debugmsg.h"

#include <glib.h>
#include <dlfcn.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>

#define UT_ABWN_MAGIC "ABWNCRP1"
#define UT_ABWN_MAGIC_LEN 8
#define UT_ABWN_VERSION 1
#define UT_ABWN_KDF_PBKDF2_SHA256 1
#define UT_ABWN_CIPHER_AES256GCM 1

#define UT_ABWN_PBKDF2_ITERS 600000
#define UT_ABWN_PBKDF2_MAX_ITERS 20000000   // sanity bound for crafted files
#define UT_ABWN_SALT_LEN 16
#define UT_ABWN_NONCE_LEN 12
#define UT_ABWN_KEY_LEN 32
#define UT_ABWN_TAG_LEN 16

/*****************************************************************/
/* HMAC-SHA-256 and PBKDF2 on top of GLib's GChecksum              */
/*****************************************************************/

static void ut_hmac_sha256(const unsigned char * key, size_t keyLen,
						   const unsigned char * m1, size_t m1Len,
						   const unsigned char * m2, size_t m2Len,
						   unsigned char out[32])
{
	unsigned char k[64] = {0};
	unsigned char pad[64];
	unsigned char inner[32];
	unsigned char khash[32];

	if (keyLen > 64)
	{
		GChecksum * c = g_checksum_new(G_CHECKSUM_SHA256);
		gsize l = sizeof(khash);
		g_checksum_update(c, key, (gssize)keyLen);
		g_checksum_get_digest(c, khash, &l);
		g_checksum_free(c);
		memcpy(k, khash, sizeof(khash));
	}
	else
		memcpy(k, key, keyLen);

	GChecksum * c = g_checksum_new(G_CHECKSUM_SHA256);
	for (int i = 0; i < 64; i++)
		pad[i] = k[i] ^ 0x36;
	g_checksum_update(c, pad, 64);
	if (m1Len)
		g_checksum_update(c, m1, (gssize)m1Len);
	if (m2Len)
		g_checksum_update(c, m2, (gssize)m2Len);
	gsize l = sizeof(inner);
	g_checksum_get_digest(c, inner, &l);
	g_checksum_free(c);

	c = g_checksum_new(G_CHECKSUM_SHA256);
	for (int i = 0; i < 64; i++)
		pad[i] = k[i] ^ 0x5c;
	g_checksum_update(c, pad, 64);
	g_checksum_update(c, inner, sizeof(inner));
	l = 32;
	g_checksum_get_digest(c, out, &l);
	g_checksum_free(c);

	memset(k, 0, sizeof(k));
	memset(pad, 0, sizeof(pad));
	memset(inner, 0, sizeof(inner));
	memset(khash, 0, sizeof(khash));
}

static void ut_pbkdf2_sha256(const char * password,
							 const unsigned char * salt, size_t saltLen,
							 UT_uint32 iters, unsigned char out[32])
{
	unsigned char u[32], t[32];
	unsigned char ctr[4] = {0, 0, 0, 1};

	ut_hmac_sha256(reinterpret_cast<const unsigned char *>(password),
				   strlen(password), salt, saltLen, ctr, sizeof(ctr), u);
	memcpy(t, u, sizeof(t));
	for (UT_uint32 i = 1; i < iters; i++)
	{
		ut_hmac_sha256(reinterpret_cast<const unsigned char *>(password),
					   strlen(password), u, sizeof(u), nullptr, 0, u);
		for (int j = 0; j < 32; j++)
			t[j] ^= u[j];
	}
	memcpy(out, t, 32);
	memset(u, 0, sizeof(u));
	memset(t, 0, sizeof(t));
}

/*****************************************************************/
/* AES-256-GCM via the system libcrypto (dlopen, no -dev needed)   */
/*****************************************************************/

typedef void UT_EVP_CIPHER;
typedef void UT_EVP_CIPHER_CTX;

#define UT_EVP_CTRL_GCM_SET_IVLEN 0x9
#define UT_EVP_CTRL_GCM_GET_TAG   0x10
#define UT_EVP_CTRL_GCM_SET_TAG   0x11

struct UT_EvpApi {
	void * lib;
	const UT_EVP_CIPHER * (*aes_256_gcm)(void);
	UT_EVP_CIPHER_CTX * (*ctx_new)(void);
	void (*ctx_free)(UT_EVP_CIPHER_CTX *);
	int (*enc_init)(UT_EVP_CIPHER_CTX *, const UT_EVP_CIPHER *, const void *, const void *, const void *);
	int (*enc_update)(UT_EVP_CIPHER_CTX *, unsigned char *, int *, const unsigned char *, int);
	int (*enc_final)(UT_EVP_CIPHER_CTX *, unsigned char *, int *);
	int (*dec_init)(UT_EVP_CIPHER_CTX *, const UT_EVP_CIPHER *, const void *, const void *, const void *);
	int (*dec_update)(UT_EVP_CIPHER_CTX *, unsigned char *, int *, const unsigned char *, int);
	int (*dec_final)(UT_EVP_CIPHER_CTX *, unsigned char *, int *);
	int (*ctx_ctrl)(UT_EVP_CIPHER_CTX *, int, int, void *);
};

static void * ut_dlsym(void * lib, const char * name)
{
	return dlsym(lib, name);
}

static const UT_EvpApi * ut_evp()
{
	static UT_EvpApi a = {};
	static int tried = 0;
	if (tried)
		return a.lib ? &a : nullptr;
	tried = 1;

	const char * libs[] = {"libcrypto.so.3", "libcrypto.so.1.1",
						   "libcrypto.so", nullptr};
	for (int i = 0; libs[i] && !a.lib; i++)
		a.lib = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
	if (!a.lib)
		return nullptr;

	struct { const char * name; void ** slot; } syms[] = {
		{"EVP_aes_256_gcm",   reinterpret_cast<void **>(&a.aes_256_gcm)},
		{"EVP_CIPHER_CTX_new",reinterpret_cast<void **>(&a.ctx_new)},
		{"EVP_CIPHER_CTX_free",reinterpret_cast<void **>(&a.ctx_free)},
		{"EVP_EncryptInit_ex",reinterpret_cast<void **>(&a.enc_init)},
		{"EVP_EncryptUpdate", reinterpret_cast<void **>(&a.enc_update)},
		{"EVP_EncryptFinal_ex",reinterpret_cast<void **>(&a.enc_final)},
		{"EVP_DecryptInit_ex",reinterpret_cast<void **>(&a.dec_init)},
		{"EVP_DecryptUpdate", reinterpret_cast<void **>(&a.dec_update)},
		{"EVP_DecryptFinal_ex",reinterpret_cast<void **>(&a.dec_final)},
		{"EVP_CIPHER_CTX_ctrl",reinterpret_cast<void **>(&a.ctx_ctrl)},
	};
	for (size_t i = 0; i < G_N_ELEMENTS(syms); i++)
	{
		*syms[i].slot = ut_dlsym(a.lib, syms[i].name);
		if (!*syms[i].slot)
		{
			UT_DEBUGMSG(("abwncrypt: %s missing in libcrypto\n", syms[i].name));
			dlclose(a.lib);
			memset(&a, 0, sizeof(a));
			return nullptr;
		}
	}
	return &a;
}

bool UT_abwn_cryptoAvailable()
{
	return ut_evp() != nullptr;
}

static bool ut_gcm(bool encrypting,
				   const unsigned char * key,
				   const unsigned char * nonce, size_t nonceLen,
				   const unsigned char * aad, size_t aadLen,
				   const unsigned char * in, size_t inLen,
				   unsigned char * out,
				   unsigned char tag[UT_ABWN_TAG_LEN])
{
	const UT_EvpApi * e = ut_evp();
	if (!e)
		return false;
	UT_EVP_CIPHER_CTX * ctx = e->ctx_new();
	if (!ctx)
		return false;

	bool ok = false;
	int outl = 0;
	do {
		int r;
		if (encrypting)
			r = e->enc_init(ctx, e->aes_256_gcm(), nullptr, nullptr, nullptr);
		else
			r = e->dec_init(ctx, e->aes_256_gcm(), nullptr, nullptr, nullptr);
		if (r != 1)
			break;
		if (e->ctx_ctrl(ctx, UT_EVP_CTRL_GCM_SET_IVLEN, (int)nonceLen, nullptr) != 1)
			break;
		if (encrypting)
			r = e->enc_init(ctx, nullptr, nullptr, key, nonce);
		else
			r = e->dec_init(ctx, nullptr, nullptr, key, nonce);
		if (r != 1)
			break;
		if (aadLen)
		{
			if (encrypting)
				r = e->enc_update(ctx, nullptr, &outl, aad, (int)aadLen);
			else
				r = e->dec_update(ctx, nullptr, &outl, aad, (int)aadLen);
			if (r != 1)
				break;
		}
		if (inLen)
		{
			if (encrypting)
				r = e->enc_update(ctx, out, &outl, in, (int)inLen);
			else
				r = e->dec_update(ctx, out, &outl, in, (int)inLen);
			if (r != 1)
				break;
			out += outl;
		}
		if (encrypting)
		{
			if (e->enc_final(ctx, out, &outl) != 1)
				break;
			if (e->ctx_ctrl(ctx, UT_EVP_CTRL_GCM_GET_TAG, UT_ABWN_TAG_LEN, tag) != 1)
				break;
		}
		else
		{
			if (e->ctx_ctrl(ctx, UT_EVP_CTRL_GCM_SET_TAG, UT_ABWN_TAG_LEN, tag) != 1)
				break;
			if (e->dec_final(ctx, out, &outl) <= 0)
				break;  // authentication failed
		}
		ok = true;
	} while (false);

	e->ctx_free(ctx);
	return ok;
}

/*****************************************************************/
/* random bytes                                                   */
/*****************************************************************/

static bool ut_rand_bytes(unsigned char * buf, size_t len)
{
	size_t got = 0;
	while (got < len)
	{
		ssize_t r = getrandom(buf + got, len - got, 0);
		if (r < 0)
			break;
		got += (size_t)r;
	}
	if (got == len)
		return true;

	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	got = 0;
	while (got < len)
	{
		ssize_t r = read(fd, buf + got, len - got);
		if (r <= 0)
		{
			close(fd);
			return false;
		}
		got += (size_t)r;
	}
	close(fd);
	return true;
}

/*****************************************************************/
/* envelope helpers                                               */
/*****************************************************************/

static void ut_put_u16(std::vector<unsigned char> & v, UT_uint16 x)
{
	v.push_back((unsigned char)(x & 0xff));
	v.push_back((unsigned char)(x >> 8));
}

static void ut_put_u32(std::vector<unsigned char> & v, UT_uint32 x)
{
	v.push_back((unsigned char)(x & 0xff));
	v.push_back((unsigned char)((x >> 8) & 0xff));
	v.push_back((unsigned char)((x >> 16) & 0xff));
	v.push_back((unsigned char)(x >> 24));
}

static bool ut_get_u16(const unsigned char * & p, const unsigned char * end,
					   UT_uint32 & out)
{
	if (end - p < 2)
		return false;
	out = (UT_uint32)p[0] | ((UT_uint32)p[1] << 8);
	p += 2;
	return true;
}

static bool ut_get_u32(const unsigned char * & p, const unsigned char * end,
					   UT_uint32 & out)
{
	if (end - p < 4)
		return false;
	out = (UT_uint32)p[0] | ((UT_uint32)p[1] << 8) |
		  ((UT_uint32)p[2] << 16) | ((UT_uint32)p[3] << 24);
	p += 4;
	return true;
}

bool UT_abwn_isEncrypted(const void * data, size_t len)
{
	return data && len >= (size_t)UT_ABWN_MAGIC_LEN &&
		   0 == memcmp(data, UT_ABWN_MAGIC, UT_ABWN_MAGIC_LEN);
}

UT_AbwnCrypt UT_abwn_encrypt(const void * plain, size_t plainLen,
							 const std::string & password,
							 std::vector<unsigned char> & out)
{
	out.clear();
	if (!plain || (plainLen && !plain) || password.empty())
		return UT_AbwnCrypt::Corrupt;
	if (!ut_evp())
		return UT_AbwnCrypt::Unavailable;

	unsigned char salt[UT_ABWN_SALT_LEN];
	unsigned char nonce[UT_ABWN_NONCE_LEN];
	unsigned char key[UT_ABWN_KEY_LEN];
	if (!ut_rand_bytes(salt, sizeof(salt)) ||
		!ut_rand_bytes(nonce, sizeof(nonce)))
		return UT_AbwnCrypt::Unavailable;
	ut_pbkdf2_sha256(password.c_str(), salt, sizeof(salt),
					 UT_ABWN_PBKDF2_ITERS, key);

	std::vector<unsigned char> header;
	header.reserve(32);
	for (int i = 0; i < UT_ABWN_MAGIC_LEN; i++)
		header.push_back((unsigned char)UT_ABWN_MAGIC[i]);
	ut_put_u16(header, UT_ABWN_VERSION);
	ut_put_u16(header, UT_ABWN_KDF_PBKDF2_SHA256);
	ut_put_u32(header, UT_ABWN_PBKDF2_ITERS);
	ut_put_u16(header, UT_ABWN_SALT_LEN);
	header.insert(header.end(), salt, salt + sizeof(salt));
	ut_put_u16(header, UT_ABWN_NONCE_LEN);
	header.insert(header.end(), nonce, nonce + sizeof(nonce));
	ut_put_u16(header, UT_ABWN_CIPHER_AES256GCM);
	ut_put_u16(header, 0);   // flags

	out.resize(header.size() + plainLen + UT_ABWN_TAG_LEN);
	memcpy(out.data(), header.data(), header.size());
	unsigned char * tag = out.data() + header.size() + plainLen;

	bool ok = ut_gcm(true, key, nonce, sizeof(nonce),
					 header.data(), header.size(),
					 static_cast<const unsigned char *>(plain), plainLen,
					 out.data() + header.size(), tag);
	memset(key, 0, sizeof(key));
	if (!ok)
	{
		out.clear();
		return UT_AbwnCrypt::Unavailable;
	}
	return UT_AbwnCrypt::Ok;
}

UT_AbwnCrypt UT_abwn_decrypt(const void * blob, size_t blobLen,
							 const std::string & password,
							 std::vector<unsigned char> & out)
{
	out.clear();
	if (!blob || password.empty())
		return UT_AbwnCrypt::Corrupt;
	if (!UT_abwn_isEncrypted(blob, blobLen))
		return UT_AbwnCrypt::NotEncrypted;
	if (!ut_evp())
		return UT_AbwnCrypt::Unavailable;

	const unsigned char * p = static_cast<const unsigned char *>(blob);
	const unsigned char * end = p + blobLen;
	p += UT_ABWN_MAGIC_LEN;

	UT_uint32 version, kdf, iters, saltLen, nonceLen, cipher, flags;
	if (!ut_get_u16(p, end, version) || version != UT_ABWN_VERSION)
		return UT_AbwnCrypt::Corrupt;
	if (!ut_get_u16(p, end, kdf) || kdf != UT_ABWN_KDF_PBKDF2_SHA256)
		return UT_AbwnCrypt::Corrupt;
	if (!ut_get_u32(p, end, iters) ||
		iters == 0 || iters > UT_ABWN_PBKDF2_MAX_ITERS)
		return UT_AbwnCrypt::Corrupt;
	if (!ut_get_u16(p, end, saltLen) || saltLen == 0 || saltLen > 64 ||
		(size_t)(end - p) < saltLen)
		return UT_AbwnCrypt::Corrupt;
	const unsigned char * salt = p;
	p += saltLen;
	if (!ut_get_u16(p, end, nonceLen) || nonceLen == 0 || nonceLen > 64 ||
		(size_t)(end - p) < nonceLen)
		return UT_AbwnCrypt::Corrupt;
	const unsigned char * nonce = p;
	p += nonceLen;
	if (!ut_get_u16(p, end, cipher) || cipher != UT_ABWN_CIPHER_AES256GCM)
		return UT_AbwnCrypt::Corrupt;
	if (!ut_get_u16(p, end, flags))
		return UT_AbwnCrypt::Corrupt;

	// ciphertext + tag
	if ((size_t)(end - p) < UT_ABWN_TAG_LEN)
		return UT_AbwnCrypt::Corrupt;
	const unsigned char * ciphertext = p;
	size_t cipherLen = (size_t)(end - p) - UT_ABWN_TAG_LEN;
	unsigned char tag[UT_ABWN_TAG_LEN];
	memcpy(tag, end - UT_ABWN_TAG_LEN, UT_ABWN_TAG_LEN);

	const unsigned char * aad = static_cast<const unsigned char *>(blob);
	size_t aadLen = (size_t)(ciphertext - aad);

	unsigned char key[UT_ABWN_KEY_LEN];
	ut_pbkdf2_sha256(password.c_str(), salt, saltLen, iters, key);

	out.resize(cipherLen);
	bool ok = ut_gcm(false, key, nonce, nonceLen,
					 aad, aadLen, ciphertext, cipherLen,
					 out.data(), tag);
	memset(key, 0, sizeof(key));
	if (!ok)
	{
		out.clear();
		return UT_AbwnCrypt::WrongPassword;
	}
	return UT_AbwnCrypt::Ok;
}
