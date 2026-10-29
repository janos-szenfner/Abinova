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

#include <string>
#include <utility>
#include <vector>

/**
 * OOXML AG_Password hashing for w:documentProtection
 * (ECMA-376 Part 1 §17.15.1.29, MS-OE376 2.15.1.28).
 *
 * The element's crypto attributes travel through the
 * document-protection-attrs doc property as one packed
 * "name=value;name=value;..." string so unknown fields survive
 * round-trips.  We generate and verify the "standard" hash scheme
 * Word writes today:
 *
 *   H0   = H(salt || utf16le(password))          -- salt prepended
 *   Hn+1 = H(Hn || le32(n))                       n = 0..spinCount-1
 *
 * (MS-OI29500: Word prepends the binary form of the salt to the
 * password; MS-OE376: H[n] = H(H[n-1], count) with count a 0-based
 * little-endian dword and the initial hash not counted.)
 */

/** Pack "name=value;name=value" preserving attribute order. */
std::string UT_dp_packAttrs(
	const std::vector<std::pair<std::string, std::string>> & attrs);

/** Unpack a packed attr string (first '=' splits name/value). */
void UT_dp_unpackAttrs(
	const std::string & packed,
	std::vector<std::pair<std::string, std::string>> & out);

/** Value of @name inside the packed string; "" when absent. */
std::string UT_dp_attrValue(const std::string & packed,
							const char * name);

/** Does the packed set carry a password verifier (w:hash /
 * w:hashValue)? */
bool UT_dp_needsPassword(const std::string & packedAttrs);

/** Build a fresh password verifier: rsaAES/hash SHA-256, 16-byte
 * salt, 100000 spins; returns the packed attribute string for
 * document-protection-attrs, or "" on failure. */
std::string UT_dp_makePasswordAttrs(const std::string & password);

/** Verify @password against the packed verifier: empty password is
 * accepted only when no verifier is stored; unknown/legacy schemes
 * fail closed (return false). */
bool UT_dp_verifyPassword(const std::string & packedAttrs,
						  const std::string & password);
