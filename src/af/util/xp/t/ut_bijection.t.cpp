/* AbiSource Applications
 * Copyright (C) 2026 Abinova contributors
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

#include <string.h>

#include "tf_test.h"
#include "ut_bijection.h"

#define TFSUITE "core.af.util.bijection"

TFTEST_MAIN("lookup in both directions")
{
	UT_Bijection b;

	// empty map: lookups miss and nth accessors are nullptr-safe
	TFPASS(b.size() == 0);
	TFPASS(b.lookupBySource("x") == nullptr);
	TFPASS(b.lookupByTarget("x") == nullptr);
	TFPASS(b.nth1(0) == nullptr);
	TFPASS(b.nth2(0) == nullptr);
	TFPASS(b.lookupBySource(nullptr) == nullptr);
	TFPASS(b.lookupByTarget(nullptr) == nullptr);

	b.add("left", "rechts");
	b.add("up", "down");
	TFPASS(b.size() == 2);

	TFPASS(strcmp(b.lookupBySource("left"), "rechts") == 0);
	TFPASS(strcmp(b.lookupBySource("up"), "down") == 0);
	TFPASS(strcmp(b.lookupByTarget("rechts"), "left") == 0);
	TFPASS(strcmp(b.lookupByTarget("down"), "up") == 0);

	TFPASS(b.lookupBySource("down") == nullptr);
	TFPASS(b.lookupByTarget("left") == nullptr);

	TFPASS(strcmp(b.nth1(0), "left") == 0);
	TFPASS(strcmp(b.nth2(0), "rechts") == 0);
	TFPASS(strcmp(b.nth1(1), "up") == 0);
	TFPASS(strcmp(b.nth2(1), "down") == 0);
	TFPASS(b.nth1(2) == nullptr);
	TFPASS(b.nth2(2) == nullptr);

	// entries are copied: mutating the source string does not
	// disturb the stored pair
	char mutable_src[] = "orig";
	char mutable_dst[] = "copy";
	b.add(mutable_src, mutable_dst);
	mutable_src[0] = 'X';
	mutable_dst[0] = 'X';
	TFPASS(strcmp(b.lookupBySource("orig"), "copy") == 0);
}

TFTEST_MAIN("pair array add and clear")
{
	const UT_Bijection::pair_data pairs[] = {
		{"one", "eins"},
		{"two", "zwei"},
		{"three", "drei"},
		{nullptr, "unreachable"},   // s1 == nullptr terminates
		{"also-unreachable", "x"},
		{nullptr, nullptr},
	};

	UT_Bijection b;
	b.add(pairs);
	TFPASS(b.size() == 3);
	TFPASS(strcmp(b.lookupBySource("three"), "drei") == 0);
	TFPASS(b.lookupBySource("unreachable") == nullptr);

	// a pair with s2 == nullptr also terminates the walk -
	// "cut" itself is NOT added
	const UT_Bijection::pair_data pairs2[] = {
		{"a", "b"},
		{"cut", nullptr},
		{"beyond", "here"},
		{nullptr, nullptr},
	};
	UT_Bijection b2;
	b2.add(pairs2);
	TFPASS(b2.size() == 1);
	TFPASS(b2.lookupBySource("cut") == nullptr);

	b2.clear();
	TFPASS(b2.size() == 0);
	TFPASS(b2.nth1(0) == nullptr);
	// clear on empty is a no-op
	b2.clear();
	TFPASS(b2.size() == 0);
}
