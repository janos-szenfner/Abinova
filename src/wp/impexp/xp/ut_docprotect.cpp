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

#include "ut_docprotect.h"
#include "ut_go_file.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#define UT_DP_SPIN 100000
#define UT_DP_MAX_SPIN 20000000   /* sanity bound for crafted attrs */
#define UT_DP_SALT_LEN 16

void UT_dp_unpackAttrs(
	const std::string & packed,
	std::vector<std::pair<std::string, std::string>> & out)
{
	out.clear();
	size_t i = 0;
	while (i <= packed.size())
	{
		size_t sep = packed.find(';', i);
		if (sep == std::string::npos)
			sep = packed.size();
		size_t eq = packed.find('=', i);
		if (eq != std::string::npos && eq < sep)
		{
			out.emplace_back(packed.substr(i, eq - i),
							 packed.substr(eq + 1, sep - eq - 1));
		}
		i = sep + 1;
	}
}

std::string UT_dp_packAttrs(
	const std::vector<std::pair<std::string, std::string>> & attrs)
{
	std::string s;
	for (const auto & kv : attrs)
	{
		if (kv.first.empty())
			continue;
		s += kv.first;
		s += '=';
		s += kv.second;
		s += ';';
	}
	return s;
}

std::string UT_dp_attrValue(const std::string & packed,
							const char * name)
{
	std::vector<std::pair<std::string, std::string>> attrs;
	UT_dp_unpackAttrs(packed, attrs);
	for (const auto & kv : attrs)
		if (kv.first == name)
			return kv.second;
	return "";
}

bool UT_dp_needsPassword(const std::string & packedAttrs)
{
	return !UT_dp_attrValue(packedAttrs, "hash").empty() ||
		   !UT_dp_attrValue(packedAttrs, "hashValue").empty();
}

/* cryptAlgorithmSid / algorithmName -> GChecksumType; the MD2 and
 * MD4 ids have no GChecksum backend and stay unsupported */
static bool s_digest_for_sid(int sid, GChecksumType & t)
{
	switch (sid)
	{
		case 3:  t = G_CHECKSUM_MD5;    return true;
		case 4:  t = G_CHECKSUM_SHA1;   return true;
		case 12: t = G_CHECKSUM_SHA256; return true;
		case 13: t = G_CHECKSUM_SHA384; return true;
		case 14: t = G_CHECKSUM_SHA512; return true;
		default: return false;
	}
}

static bool s_digest_for_name(const std::string & name,
							  GChecksumType & t)
{
	if (name == "MD5")
		{ t = G_CHECKSUM_MD5; return true; }
	if (name == "SHA-1" || name == "SHA1")
		{ t = G_CHECKSUM_SHA1; return true; }
	if (name == "SHA-256" || name == "SHA256")
		{ t = G_CHECKSUM_SHA256; return true; }
	if (name == "SHA-384" || name == "SHA384")
		{ t = G_CHECKSUM_SHA384; return true; }
	if (name == "SHA-512" || name == "SHA512")
		{ t = G_CHECKSUM_SHA512; return true; }
	return false;
}

static bool s_digest(GChecksumType t,
					 const unsigned char * d1, size_t l1,
					 const unsigned char * d2, size_t l2,
					 unsigned char * out, size_t outLen)
{
	GChecksum * c = g_checksum_new(t);
	if (!c)
		return false;
	if (d1 && l1)
		g_checksum_update(c, d1, static_cast<gssize>(l1));
	if (d2 && l2)
		g_checksum_update(c, d2, static_cast<gssize>(l2));
	gsize l = outLen;
	g_checksum_get_digest(c, out, &l);
	g_checksum_free(c);
	return l == outLen;
}

/* utf16le without BOM, the form AG_Password hashing consumes */
static void s_password_utf16(const std::string & password,
							 std::vector<unsigned char> & out)
{
	out.clear();
	glong n = 0;
	gunichar2 * u = g_utf8_to_utf16(password.c_str(),
									static_cast<gssize>(password.size()),
									nullptr, &n, nullptr);
	if (!u)
		return;
	out.reserve(static_cast<size_t>(n) * 2);
	for (glong i = 0; i < n; i++)
	{
		out.push_back(static_cast<unsigned char>(u[i] & 0xff));
		out.push_back(static_cast<unsigned char>((u[i] >> 8) & 0xff));
	}
	g_free(u);
}

/* H0 = H(salt || utf16le(pw)); Hn+1 = H(Hn || le32(n)) */
static bool s_ag_password_hash(GChecksumType t,
							   const unsigned char * pw, size_t pwLen,
							   const unsigned char * salt, size_t saltLen,
							   unsigned long spin,
							   std::vector<unsigned char> & out)
{
	const gsize hlen = g_checksum_type_get_length(t);
	if (hlen <= 0)
		return false;
	out.resize(static_cast<size_t>(hlen));
	if (!s_digest(t, salt, saltLen, pw, pwLen, out.data(), out.size()))
		return false;
	for (unsigned long i = 0; i < spin; i++)
	{
		unsigned char ctr[4] = {
			static_cast<unsigned char>(i & 0xff),
			static_cast<unsigned char>((i >> 8) & 0xff),
			static_cast<unsigned char>((i >> 16) & 0xff),
			static_cast<unsigned char>((i >> 24) & 0xff) };
		std::vector<unsigned char> prev(out);
		if (!s_digest(t, prev.data(), prev.size(), ctr, sizeof(ctr),
					  out.data(), out.size()))
			return false;
	}
	return true;
}

std::string UT_dp_makePasswordAttrs(const std::string & password)
{
	if (password.empty())
		return "";
	unsigned char salt[UT_DP_SALT_LEN];
	if (!UT_go_random_bytes(salt, sizeof(salt)))
		return "";

	std::vector<unsigned char> pw;
	s_password_utf16(password, pw);
	std::vector<unsigned char> hash;
	if (!s_ag_password_hash(G_CHECKSUM_SHA256,
							pw.data(), pw.size(),
							salt, sizeof(salt), UT_DP_SPIN, hash))
		return "";

	gchar * salt64 = g_base64_encode(salt, sizeof(salt));
	gchar * hash64 = g_base64_encode(hash.data(), hash.size());
	std::vector<std::pair<std::string, std::string>> attrs = {
		{"cryptProviderType",   "rsaAES"},
		{"cryptAlgorithmClass", "hash"},
		{"cryptAlgorithmType",  "typeAny"},
		{"cryptAlgorithmSid",   "12"},        /* SHA-256 */
		{"cryptSpinCount",      std::to_string(UT_DP_SPIN)},
		{"salt",                salt64 ? salt64 : ""},
		{"hash",                hash64 ? hash64 : ""}
	};
	g_free(salt64);
	g_free(hash64);
	return UT_dp_packAttrs(attrs);
}

bool UT_dp_verifyPassword(const std::string & packedAttrs,
						  const std::string & password)
{
	std::vector<std::pair<std::string, std::string>> attrs;
	UT_dp_unpackAttrs(packedAttrs, attrs);
	auto value = [&](const char * n) -> std::string {
		for (const auto & kv : attrs)
			if (kv.first == n)
				return kv.second;
		return "";
	};

	const std::string want = value("hash").empty()
		? value("hashValue") : value("hash");
	if (want.empty())
		return true;                       /* unpassworded */
	if (password.empty())
		return false;

	GChecksumType t;
	const std::string sid = value("cryptAlgorithmSid");
	if (!sid.empty())
	{
		if (!s_digest_for_sid(atoi(sid.c_str()), t))
			return false;
	}
	else if (!s_digest_for_name(value("algorithmName"), t))
		return false;                      /* legacy w:hash: unsupported */

	const std::string salt64 = value("salt").empty()
		? value("saltValue") : value("salt");
	gsize saltLen = 0;
	guchar * salt = salt64.empty() ? nullptr
		: g_base64_decode(salt64.c_str(), &saltLen);
	const std::string spinStr = value("cryptSpinCount").empty()
		? value("spinCount") : value("cryptSpinCount");
	const unsigned long spin =
		strtoul(spinStr.empty() ? "0" : spinStr.c_str(), nullptr, 10);
	if (spin > UT_DP_MAX_SPIN)
	{
		g_free(salt);
		return false;
	}

	gsize wantLen = 0;
	guchar * wantBin = g_base64_decode(want.c_str(), &wantLen);

	std::vector<unsigned char> pw;
	s_password_utf16(password, pw);
	std::vector<unsigned char> hash;
	const bool ok =
		s_ag_password_hash(t, pw.data(), pw.size(),
						   salt, saltLen, spin, hash) &&
		wantLen == hash.size();

	unsigned char diff = 0;
	if (ok)
		for (size_t i = 0; i < hash.size(); i++)
			diff |= static_cast<unsigned char>(hash[i] ^ wantBin[i]);

	g_free(salt);
	g_free(wantBin);
	return ok && diff == 0;
}
