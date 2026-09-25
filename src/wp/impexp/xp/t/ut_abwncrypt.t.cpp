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

#include "tf_test.h"
#include "ut_abwncrypt.h"

#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.wp.impexp.abwncrypt"

static const char PLAIN[] = "<abinova><section><p>hello encrypted world</p></section></abinova>";

static UT_uint16 rd_le16(const std::vector<unsigned char> & v, size_t off)
{
	return static_cast<UT_uint16>(v[off] | (v[off + 1] << 8));
}

static UT_uint32 rd_le32(const std::vector<unsigned char> & v, size_t off)
{
	return static_cast<UT_uint32>(v[off] | (v[off + 1] << 8) |
								  (v[off + 2] << 16) | (v[off + 3] << 24));
}

TFTEST_MAIN("crypto backend available")
{
	// The suite runs on a system where libcrypto is present; if it
	// ever isn't, every other check here becomes meaningless anyway.
	TFPASS(UT_abwn_cryptoAvailable());
}

TFTEST_MAIN("encrypt produces a well-formed envelope")
{
	std::vector<unsigned char> blob;
	TFPASSEQ(static_cast<int>(UT_abwn_encrypt(PLAIN, strlen(PLAIN), "s3cret", blob)),
			 static_cast<int>(UT_AbwnCrypt::Ok));

	// magic + minimum envelope size
	TFPASS(UT_abwn_isEncrypted(blob.data(), blob.size()));

	// header fields: version, kdf, iterations, salt
	TFPASSEQ(rd_le16(blob, 8), 1);            // format version
	TFPASSEQ(rd_le16(blob, 10), 1);           // PBKDF2-HMAC-SHA-256
	TFPASS(rd_le32(blob, 12) >= 100000);      // sane iteration count
	UT_uint16 saltLen = rd_le16(blob, 16);
	TFPASS(saltLen >= 16);
	size_t nonceOff = 18 + saltLen;
	TFPASS(nonceOff + 2 < blob.size());
	UT_uint16 nonceLen = rd_le16(blob, nonceOff);
	TFPASS(nonceLen >= 12);                   // >= GCM nonce minimum
	size_t tailOff = nonceOff + 2 + nonceLen;
	TFPASSEQ(rd_le16(blob, tailOff), 1);      // AES-256-GCM
	TFPASSEQ(rd_le16(blob, tailOff + 2), 0);  // flags
	TFPASS(tailOff + 4 < blob.size());        // ciphertext follows
}

TFTEST_MAIN("decrypt round-trip")
{
	std::vector<unsigned char> blob, out;
	TFPASSEQ(static_cast<int>(UT_abwn_encrypt(PLAIN, strlen(PLAIN), "s3cret", blob)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(), "s3cret", out)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	TFPASSEQ(out.size(), strlen(PLAIN));
	TFPASS(memcmp(out.data(), PLAIN, out.size()) == 0);
}

TFTEST_MAIN("decrypt round-trip, large plaintext")
{
	std::string big(300000, 'x');
	for (size_t i = 0; i < big.size(); i += 997)
		big[i] = static_cast<char>('A' + (i / 997) % 26);
	std::vector<unsigned char> blob, out;
	TFPASSEQ(static_cast<int>(UT_abwn_encrypt(big.data(), big.size(), "pw", blob)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(), "pw", out)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	TFPASSEQ(out.size(), big.size());
	TFPASS(memcmp(out.data(), big.data(), out.size()) == 0);
}

TFTEST_MAIN("empty plaintext round-trip; empty password rejected")
{
	std::vector<unsigned char> blob, out;
	TFPASSEQ(static_cast<int>(UT_abwn_encrypt("", 0, "pw", blob)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(), "pw", out)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	TFPASS(out.empty());

	// empty password is rejected on both sides
	std::vector<unsigned char> blob2, out2;
	TFPASSEQ(static_cast<int>(UT_abwn_encrypt(PLAIN, strlen(PLAIN), "", blob2)),
			 static_cast<int>(UT_AbwnCrypt::Corrupt));
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(), "", out2)),
			 static_cast<int>(UT_AbwnCrypt::Corrupt));
}

TFTEST_MAIN("wrong password fails with WrongPassword")
{
	std::vector<unsigned char> blob, out;
	UT_abwn_encrypt(PLAIN, strlen(PLAIN), "right", blob);
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(), "wrong", out)),
			 static_cast<int>(UT_AbwnCrypt::WrongPassword));
}

TFTEST_MAIN("isEncrypted rejects non-envelope input")
{
	TFPASS(!UT_abwn_isEncrypted(PLAIN, strlen(PLAIN)));
	TFPASS(!UT_abwn_isEncrypted("ABWNCRP", 7));   // one byte short of magic
	TFPASS(!UT_abwn_isEncrypted(nullptr, 0));
}

TFTEST_MAIN("truncated envelope reports Corrupt")
{
	std::vector<unsigned char> blob, out;
	UT_abwn_encrypt(PLAIN, strlen(PLAIN), "pw", blob);
	for (size_t cut : {8UL, 16UL, 20UL, blob.size() / 2, blob.size() - 1}) {
		UT_AbwnCrypt r = UT_abwn_decrypt(blob.data(), cut, "pw", out);
		TFPASS(r == UT_AbwnCrypt::Corrupt || r == UT_AbwnCrypt::WrongPassword);
		TFPASS(r != UT_AbwnCrypt::Ok);
	}
}

TFTEST_MAIN("tampered ciphertext is rejected")
{
	std::vector<unsigned char> blob, out;
	UT_abwn_encrypt(PLAIN, strlen(PLAIN), "pw", blob);
	// flip a byte in the ciphertext (last 40 bytes are surely ciphertext+tag)
	blob[blob.size() - 20] ^= 0x01;
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(), "pw", out)),
			 static_cast<int>(UT_AbwnCrypt::WrongPassword));
}

TFTEST_MAIN("tampered header is rejected (AAD binding)")
{
	std::vector<unsigned char> blob, out;
	UT_abwn_encrypt(PLAIN, strlen(PLAIN), "pw", blob);
	// downgrade the iteration count: header is authenticated, so the
	// GCM tag must fail rather than silently accepting weaker params
	blob[12] ^= 0x01;
	UT_AbwnCrypt r = UT_abwn_decrypt(blob.data(), blob.size(), "pw", out);
	TFPASS(r == UT_AbwnCrypt::WrongPassword || r == UT_AbwnCrypt::Corrupt);
}

TFTEST_MAIN("bad version / kdf / cipher ids report Corrupt")
{
	std::vector<unsigned char> blob, out;
	UT_abwn_encrypt(PLAIN, strlen(PLAIN), "pw", blob);

	std::vector<unsigned char> b = blob;
	b[8] = 99;                                   // bogus version
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(b.data(), b.size(), "pw", out)),
			 static_cast<int>(UT_AbwnCrypt::Corrupt));

	b = blob;
	b[10] = 77;                                  // unknown kdf
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(b.data(), b.size(), "pw", out)),
			 static_cast<int>(UT_AbwnCrypt::Corrupt));

	// unknown cipher id lives right after salt+nonce
	b = blob;
	UT_uint16 saltLen = rd_le16(b, 16);
	size_t ciphOff = 18 + saltLen + 2 + rd_le16(b, 18 + saltLen);
	b[ciphOff] = 66;
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(b.data(), b.size(), "pw", out)),
			 static_cast<int>(UT_AbwnCrypt::Corrupt));
}

TFTEST_MAIN("same plaintext encrypts to different envelopes (random salt/nonce)")
{
	std::vector<unsigned char> a, b;
	UT_abwn_encrypt(PLAIN, strlen(PLAIN), "pw", a);
	UT_abwn_encrypt(PLAIN, strlen(PLAIN), "pw", b);
	TFPASSEQ(a.size(), b.size());
	TFPASS(a != b);
}
