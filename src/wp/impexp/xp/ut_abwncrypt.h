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

#pragma once

#include "ut_types.h"
#include <stddef.h>
#include <string>
#include <vector>

/**
 * Password-protected .abwn container (format version 1).
 *
 * The document is serialized exactly like a normal .abwn (XML,
 * gzip-compressed when configured), then wrapped in an authenticated
 * envelope:
 *
 *   offset  size    field
 *   0       8       magic "ABWNCRP1"
 *   8       2       format version (=1)
 *   10      2       kdf id (=1: PBKDF2-HMAC-SHA-256)
 *   12      4       kdf iterations (LE)
 *   16      2       salt length N (LE)
 *   18      N       salt
 *   .       2       nonce length M (LE)
 *   .       M       nonce
 *   .       2       cipher id (=1: AES-256-GCM)
 *   .       2       flags (=0)
 *   .       ..      ciphertext .. || 16-byte GCM tag (last 16 bytes)
 *
 * The whole header is bound into the GCM tag as AAD, so parameters
 * cannot be tampered with. AES-256-GCM keeps ~128-bit security even
 * under Grover's algorithm, so this remains safe against quantum
 * attacks; the kdf/cipher ids leave room for future key-encapsulation
 * (e.g. ML-KEM) modes.
 */
enum class UT_AbwnCrypt {
	Ok = 0,
	NotEncrypted,        // blob does not carry the magic
	WrongPassword,       // tag check failed (wrong pw or tampered data)
	Corrupt,             // truncated/malformed/unsupported header
	Unavailable          // crypto backend (libcrypto) not present
};

/** Is the crypto backend (AES-256-GCM via libcrypto) usable? */
bool UT_abwn_cryptoAvailable();

/** Cheap magic check; also validates minimum envelope size. */
bool UT_abwn_isEncrypted(const void * data, size_t len);

/** Serialize-and-wrap: encrypts @plainLen bytes of @plain. */
UT_AbwnCrypt UT_abwn_encrypt(const void * plain, size_t plainLen,
							 const std::string & password,
							 std::vector<unsigned char> & out);

/** Unwrap: verifies header + tag, fills @out with plaintext. */
UT_AbwnCrypt UT_abwn_decrypt(const void * blob, size_t blobLen,
							 const std::string & password,
							 std::vector<unsigned char> & out);
