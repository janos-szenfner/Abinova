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

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "tf_test.h"
#include "ut_hash.h"
#include "ut_string_class.h"

#define TFSUITE "core.af.util.hash"

TFTEST_MAIN("insert / pick / contains / remove")
{
	UT_StringPtrMap m;

	TFPASS(m.size() == 0);
	TFPASS(m.pick("absent") == nullptr);
	TFPASS(!m.contains("absent", nullptr));

	TFPASS(m.insert("one", reinterpret_cast<const void *>(1)));
	TFPASS(m.size() == 1);
	TFPASS(m.pick("one") == reinterpret_cast<const void *>(1));

	// duplicate insert is refused, existing value kept
	TFPASS(!m.insert("one", reinterpret_cast<const void *>(2)));
	TFPASS(m.pick("one") == reinterpret_cast<const void *>(1));
	TFPASS(m.size() == 1);

	// contains(key, nullptr) checks key presence;
	// contains(key, v) checks the pair
	TFPASS(m.contains("one", nullptr));
	TFPASS(m.contains("one", reinterpret_cast<const void *>(1)));
	TFPASS(!m.contains("one", reinterpret_cast<const void *>(2)));
	TFPASS(!m.contains("other", nullptr));

	// set replaces the value of an existing key
	m.set("one", reinterpret_cast<const void *>(7));
	TFPASS(m.pick("one") == reinterpret_cast<const void *>(7));
	// set on a missing key inserts it
	m.set("two", reinterpret_cast<const void *>(8));
	TFPASS(m.size() == 2);
	TFPASS(m.pick("two") == reinterpret_cast<const void *>(8));

	// remove drops the key; the value is NOT owned/freed
	m.remove("one", nullptr);
	TFPASS(m.size() == 1);
	TFPASS(m.pick("one") == nullptr);
	TFPASS(!m.contains("one", nullptr));
	// removing an absent key is harmless
	m.remove("one", nullptr);
	m.remove("never-present", nullptr);
	TFPASS(m.size() == 1);

	// a removed key can be reinserted
	TFPASS(m.insert("one", reinterpret_cast<const void *>(9)));
	TFPASS(m.pick("one") == reinterpret_cast<const void *>(9));
	TFPASS(m.size() == 2);

	// UT_String keys are interchangeable with const char* keys
	UT_String ks("two");
	TFPASS(m.pick(ks) == reinterpret_cast<const void *>(8));
	TFPASS(m.contains(ks, nullptr));
	m.remove(ks, nullptr);
	TFPASS(m.pick("two") == nullptr);

	m.clear();
	TFPASS(m.size() == 0);
	TFPASS(m.pick("one") == nullptr);
}

TFTEST_MAIN("cursor iteration")
{
	UT_StringPtrMap m;
	m.insert("a", reinterpret_cast<const void *>(1));
	m.insert("b", reinterpret_cast<const void *>(2));
	m.insert("c", reinterpret_cast<const void *>(3));

	// cursor visits every live slot exactly once
	std::set<const void *> seen;
	std::set<std::string> keys;
	UT_GenericStringMap<const void *>::UT_Cursor c(&m);
	for (const void * val = c.first(); c.is_valid(); val = c.next())
	{
		TFPASS(seen.insert(val).second);        // no duplicates
		keys.insert(c.key().c_str());
	}
	TFPASS(seen.size() == 3);
	TFPASS(keys == std::set<std::string>({"a", "b", "c"}));

	// prev() only steps back from a still-valid cursor: there is no
	// last() so once iteration runs off the end it stays invalid
	TFPASS(!c.is_valid());
	(void)c.prev();
	TFPASS(!c.is_valid());

	// re-iterating with a fresh cursor sees the same set
	std::set<const void *> seen2;
	UT_GenericStringMap<const void *>::UT_Cursor c2(&m);
	for (const void * val = c2.first(); c2.is_valid(); val = c2.next())
		seen2.insert(val);
	TFPASS(seen2 == seen);

	// an empty map's cursor is immediately invalid
	UT_StringPtrMap empty;
	UT_GenericStringMap<const void *>::UT_Cursor ec(&empty);
	(void)ec.first();
	TFPASS(!ec.is_valid());
}

TFTEST_MAIN("enumerate and keys")
{
	UT_StringPtrMap m;
	const void * v1 = reinterpret_cast<const void *>(11);
	const void * v2 = reinterpret_cast<const void *>(22);
	m.insert("k1", v1);
	m.insert("k2", v2);

	auto vals = m.enumerate();
	TFPASS(vals != nullptr);
	TFPASS(vals->size() == 2);
	std::set<const void *> vs(vals->begin(), vals->end());
	TFPASS(vs.count(v1) == 1 && vs.count(v2) == 1);

	auto ks = m.keys();
	TFPASS(ks != nullptr);
	TFPASS(ks->size() == 2);
	std::set<std::string> keyset;
	for (const UT_String * k : *ks)
		keyset.insert(k->c_str());
	TFPASS(keyset.count("k1") == 1 && keyset.count("k2") == 1);

	// a nullptr value makes the slot look EMPTY (slot emptiness is
	// defined by m_value == UT_null<T>::value): n_keys still counted
	// the insert but pick/enumerate/cursor never see the entry --
	// a zombie.  enumerate(false) can only keep nulls that live in
	// visible slots, so a null-valued entry is invisible either way.
	UT_StringPtrMap mn;
	mn.insert("null", nullptr);
	mn.insert("real", v1);
	TFPASS(mn.size() == 2);                    // counted
	TFPASS(mn.pick("null") == nullptr);        // invisible
	auto stripped = mn.enumerate();
	TFPASS(stripped->size() == 1);
	TFPASS((*stripped)[0] == v1);
	auto full = mn.enumerate(false);
	TFPASS(full->size() == 1);                 // zombie stays invisible
}

TFTEST_MAIN("growth triggers reorg and preserves entries")
{
	UT_StringPtrMap m(11);
	char buf[16];
	std::vector<std::string> names;

	for (int i = 0; i < 300; ++i)
	{
		snprintf(buf, sizeof(buf), "key%03d", i);
		names.emplace_back(buf);
		TFPASS(m.insert(names.back().c_str(),
		                reinterpret_cast<const void *>(static_cast<uintptr_t>(i + 1))));
	}
	TFPASS(m.size() == 300);

	for (int i = 0; i < 300; ++i)
		TFPASS(m.pick(names[i].c_str()) ==
		       reinterpret_cast<const void *>(static_cast<uintptr_t>(i + 1)));

	// heavy deletion shrinks the table and keeps the survivors
	for (int i = 0; i < 250; ++i)
		m.remove(names[i].c_str(), nullptr);
	TFPASS(m.size() == 50);
	for (int i = 0; i < 250; ++i)
		TFPASS(m.pick(names[i].c_str()) == nullptr);
	for (int i = 250; i < 300; ++i)
		TFPASS(m.pick(names[i].c_str()) ==
		       reinterpret_cast<const void *>(static_cast<uintptr_t>(i + 1)));

	// enumeration covers exactly the survivors
	auto vals = m.enumerate();
	TFPASS(vals->size() == 50);
}

TFTEST_MAIN("purgeData and freeData release caller-owned values")
{
	static int s_deleted = 0;
	struct Owned { ~Owned() { ++s_deleted; } };

	{
		UT_GenericStringMap<Owned *> m;
		m.insert("a", new Owned);
		m.insert("b", new Owned);
		m.purgeData();
		TFPASS(s_deleted == 2);
		// slots are left deleted: lookups miss, enumeration is empty
		TFPASS(m.pick("a") == nullptr);
		auto vals = m.enumerate();
		TFPASS(vals->empty());
	}

	// freeData is the g_free twin for malloc'd values
	UT_GenericStringMap<char *> mg;
	mg.insert("s1", g_strdup("payload1"));
	mg.insert("s2", g_strdup("payload2"));
	mg.freeData();
	TFPASS(mg.pick("s1") == nullptr);
	TFPASS(mg.enumerate()->empty());
}

TFTEST_MAIN("list() flattens key/value pairs")
{
	// list() is only for <char*> -> <char*> maps per the API note
	// (the implementation casts values to gchar* so T must be
	// non-const char*)
	static char k1[] = "width",  v1[] = "100";
	static char k2[] = "height", v2[] = "200";
	static char k3[] = "depth",  v3[] = "3";
	UT_GenericStringMap<char *> m;
	m.insert(k1, v1);
	m.insert(k2, v2);

	const gchar ** flat = m.list();
	TFPASS(flat != nullptr);

	std::set<std::string> pairs;
	for (size_t i = 0; flat[i]; i += 2)
	{
		TFPASS(flat[i + 1] != nullptr);
		pairs.emplace(std::string(flat[i]) + "=" + flat[i + 1]);
	}
	TFPASS(pairs.count("width=100") == 1);
	TFPASS(pairs.count("height=200") == 1);

	// mutating the map invalidates the cached list
	m.insert(k3, v3);
	flat = m.list();
	size_t n = 0;
	for (; flat[n]; ++n)
		;
	TFPASS(n == 6);
}

TFTEST_MAIN("hash size helper")
{
	// smallest magic table size >= request, or (UT_uint32)-1
	TFPASS(_Recommended_hash_size(11) == 11);
	TFPASS(_Recommended_hash_size(0) == 2);    // smallest table entry
	TFPASS(_Recommended_hash_size(2) == 2);
	TFPASS(_Recommended_hash_size(500) >= 500);
	TFPASS(_Recommended_hash_size(0xffffffff) == static_cast<UT_uint32>(-1));
}
