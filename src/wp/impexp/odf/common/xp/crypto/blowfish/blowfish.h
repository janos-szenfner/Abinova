/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 *
 * Vendored from OpenSSL 4.0.2 (crypto/bf). Stripped of OpenSSL
 * build-system headers (opensslconf.h, e_os2.h, macros.h) so it
 * can be built standalone.
 */

#ifndef OPENSSL_BLOWFISH_H
#define OPENSSL_BLOWFISH_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BF_BLOCK 8

#define BF_ENCRYPT 1
#define BF_DECRYPT 0

/*-
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * ! BF_LONG has to be at least 32 bits wide.                     !
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */
typedef unsigned int BF_LONG;

#define BF_ROUNDS 16

typedef struct bf_key_st {
    BF_LONG P[BF_ROUNDS + 2];
    BF_LONG S[4 * 256];
} BF_KEY;

void BF_set_key(BF_KEY *key, int len,
    const unsigned char *data);
void BF_encrypt(BF_LONG *data, const BF_KEY *key);
void BF_decrypt(BF_LONG *data, const BF_KEY *key);
void BF_ecb_encrypt(const unsigned char *in,
    unsigned char *out, const BF_KEY *key,
    int enc);
void BF_cbc_encrypt(const unsigned char *in,
    unsigned char *out, long length,
    const BF_KEY *schedule,
    unsigned char *ivec, int enc);
void BF_cfb64_encrypt(const unsigned char *in,
    unsigned char *out,
    long length, const BF_KEY *schedule,
    unsigned char *ivec, int *num,
    int enc);
void BF_ofb64_encrypt(const unsigned char *in,
    unsigned char *out,
    long length, const BF_KEY *schedule,
    unsigned char *ivec, int *num);

#ifdef __cplusplus
}
#endif

#endif
