/* AbiSource Applications
 * Copyright (C) 2006 Hubert Figuiere
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
#include "ut_vector.h"

#define TFSUITE "core.af.util.vector"

TFTEST_MAIN("UT_GenericVector basics")
{
	UT_GenericVector<const char *> v;

	TFPASS(v.getItemCount() == 0);
	v.addItem("foo");
	TFPASS(v.getItemCount() == 1);
	v.addItem("bar");
	TFPASS(v.getItemCount() == 2);
	v.addItem("baz");
	TFPASS(v.getItemCount() == 3);

	TFPASS(strcmp(v[1], "bar") == 0);
	TFPASS(strcmp(v.getNthItem(2), "baz") == 0);

	TFPASS(strcmp(v.getFirstItem(), "foo") == 0);
	TFPASS(strcmp(v.getLastItem(), "baz") == 0);
	TFPASS(strcmp(v.back(), "baz") == 0);

	v.push_back("metropolis");
	TFPASS(v.getItemCount() == 4);
	TFPASS(strcmp(v.back(), "metropolis") == 0);
	
	v.insertItemAt("matrix", 3);
	TFPASS(v.getItemCount() == 5);
	TFPASS(strcmp(v[3], "matrix") == 0);
	TFPASS(strcmp(v[4], "metropolis") == 0);

	v.deleteNthItem(3);
	TFPASS(v.getItemCount() == 4);
	TFPASS(strcmp(v[3], "metropolis") == 0);	

	TFPASS(v.size() == 4);

	TFPASS(v.findItem("metropolis") == 3);
	TFPASS(v.findItem("bar") == 1);

	TFPASS(v.pop_back());
	TFPASS(v.getItemCount() == 3);
	TFPASS(strcmp(v.getLastItem(), "baz") == 0);

	v.clear();
	TFPASS(v.getItemCount() == 0);
}

TFTEST_MAIN("out-of-bounds access yields null, not UB")
{
	// Unlike std::vector, UT_GenericVector's accessors are total
	// functions: OOB reads return UT_null<T>::value (nullptr for
	// pointer types) and OOB deletes are no-ops.  GR_CharWidths and
	// the migrated code rely on this.
	UT_GenericVector<const char *> v;
	v.addItem("a");
	v.addItem("b");

	TFPASS(v.getNthItem(-1) == nullptr);
	TFPASS(v.getNthItem(2) == nullptr);
	TFPASS(v.getNthItem(1000) == nullptr);
	TFPASS(v[-5] == nullptr);
	TFPASS(v[2] == nullptr);

	// OOB deletes are silently ignored, nothing changes
	v.deleteNthItem(-1);
	v.deleteNthItem(2);
	v.deleteNthItem(999);
	TFPASS(v.getItemCount() == 2);
	TFPASS(strcmp(v[0], "a") == 0);
	TFPASS(strcmp(v[1], "b") == 0);

	// first/last/back on an empty vector return T(), not UB
	UT_GenericVector<const char *> empty;
	TFPASS(empty.getFirstItem() == nullptr);
	TFPASS(empty.getLastItem() == nullptr);
	TFPASS(empty.back() == nullptr);
	TFPASS(empty.getNthItem(0) == nullptr);

	// pop_back reports failure on empty, does not underflow
	TFPASS(!empty.pop_back());
	TFPASS(empty.getItemCount() == 0);
}

TFTEST_MAIN("sparse slots via setNthItem")
{
	// setNthItem past the end grows the vector and leaves the gap
	// slots zeroed (nullptr for pointer T) -- this is the sparse-array
	// semantic GR_CharWidths depends on.
	UT_GenericVector<const char *> v;
	v.addItem("x");
	v.addItem("y");

	TFPASS(v.setNthItem(4, "tail", nullptr) == 0);
	TFPASS(v.getItemCount() == 5);
	TFPASS(v.getNthItem(0) == static_cast<const char*>("x"));
	TFPASS(v.getNthItem(2) == nullptr);
	TFPASS(v.getNthItem(3) == nullptr);
	TFPASS(v.getNthItem(4) == static_cast<const char*>("tail"));

	// writing an existing slot reports the displaced value via ppOld
	const char * old = nullptr;
	TFPASS(v.setNthItem(1, "Y", reinterpret_cast<const char **>(&old)) == 0);
	TFPASS(old == static_cast<const char*>("y"));
	TFPASS(strcmp(v[1], "Y") == 0);
	TFPASS(v.getItemCount() == 5);

	// ppOld reports null when the slot was beyond the old storage
	old = "sentinel";
	TFPASS(v.setNthItem(9, "far", reinterpret_cast<const char **>(&old)) == 0);
	TFPASS(old == nullptr);
	TFPASS(v.getItemCount() == 10);
	TFPASS(v.getNthItem(9) == static_cast<const char*>("far"));

	// negative index rejected
	TFPASS(v.setNthItem(-1, "no", nullptr) == -1);
	TFPASS(v.getItemCount() == 10);
}

TFTEST_MAIN("insert bounds")
{
	UT_GenericVector<const char *> v;
	v.addItem("a");
	v.addItem("c");

	// insert in the middle
	TFPASS(v.insertItemAt("b", 1) == 0);
	TFPASS(v.getItemCount() == 3);
	TFPASS(strcmp(v[0], "a") == 0);
	TFPASS(strcmp(v[1], "b") == 0);
	TFPASS(strcmp(v[2], "c") == 0);

	// insert at index == count is an append
	TFPASS(v.insertItemAt("d", v.getItemCount()) == 0);
	TFPASS(v.getItemCount() == 4);
	TFPASS(strcmp(v[3], "d") == 0);

	// index > count is rejected (and must not touch memory --
	// historically count+1 slipped past the guard into a negative
	// memmove length)
	TFPASS(v.insertItemAt("x", v.getItemCount() + 1) == -1);
	TFPASS(v.getItemCount() == 4);
	TFPASS(v.insertItemAt("x", 100) == -1);
	TFPASS(v.insertItemAt("x", -1) == -1);
	TFPASS(v.getItemCount() == 4);
}

TFTEST_MAIN("findItem / addItem index reporting")
{
	UT_GenericVector<const char *> v;
	// findItem compares element values -- for const char* elements
	// that is pointer identity, so use named variables
	const char * items[] = {"alpha", "beta", "gamma"};
	for (const char * it : items)
		v.addItem(it);

	TFPASS(v.findItem(items[1]) == 1);
	TFPASS(v.findItem(items[2]) == 2);
	TFPASS(v.findItem("delta") == -1);
	TFPASS(v.findItem(nullptr) == -1);

	// addItem returns 0 on success; the pIndex overload reports
	// the slot the item landed in (count-1)
	const char * delta = "delta";
	UT_sint32 idx = -1;
	TFPASS(v.addItem(delta, &idx) == 0);
	TFPASS(idx == 3);
	TFPASS(v.findItem(delta) == 3);
}

// binarysearch/addItemSorted comparator: called as compar(key, &element)
static int s_compare_cstr(const void * a, const void * b)
{
	return strcmp(*static_cast<const char * const *>(a),
	              *static_cast<const char * const *>(b));
}

TFTEST_MAIN("sort, binarysearch and addItemSorted")
{
	UT_GenericVector<const char *> v;
	v.addItem("pear");
	v.addItem("apple");
	v.addItem("zebra");
	v.addItem("mango");

	// templated sort() takes a strict-weak-order comparer invoked
	// on the elements themselves (not on element pointers)
	v.sort([](const char * const & a, const char * const & b) {
		return strcmp(a, b) < 0;
	});
	TFPASS(strcmp(v[0], "apple") == 0);
	TFPASS(strcmp(v[1], "mango") == 0);
	TFPASS(strcmp(v[2], "pear") == 0);
	TFPASS(strcmp(v[3], "zebra") == 0);

	// binarysearch takes a compar_fn_t (memcmp-style) over &element
	const char * key = "mango";
	TFPASS(v.binarysearch(&key, s_compare_cstr) == 1);
	key = "apple";
	TFPASS(v.binarysearch(&key, s_compare_cstr) == 0);
	key = "quince";
	TFPASS(v.binarysearch(&key, s_compare_cstr) == -1);

	// addItemSorted keeps the array ordered
	v.addItemSorted("banana", s_compare_cstr);
	v.addItemSorted("yam", s_compare_cstr);
	TFPASS(v.getItemCount() == 6);
	TFPASS(strcmp(v[0], "apple") == 0);
	TFPASS(strcmp(v[1], "banana") == 0);
	TFPASS(strcmp(v[5], "zebra") == 0);
	key = "yam";
	TFPASS(v.binarysearch(&key, s_compare_cstr) == 4);
}

TFTEST_MAIN("copy semantics are shallow element copies")
{
	// Copy ctor, operator= and copy() all copy elements by value
	// (shallow: pointer elements are shared, not cloned) but the
	// storage is independent -- mutating one vector's slots does not
	// disturb the other.
	UT_GenericVector<const char *> src;
	src.addItem("one");
	src.addItem("two");

	UT_GenericVector<const char *> v1(src);
	TFPASS(v1.getItemCount() == 2);
	TFPASS(v1[0] == src[0]);   // same pointer: shallow
	TFPASS(v1[1] == src[1]);

	// independent storage: setNthItem on the copy leaves src alone
	v1.setNthItem(0, "uno", nullptr);
	TFPASS(strcmp(src[0], "one") == 0);
	TFPASS(strcmp(v1[0], "uno") == 0);
	// and addItem on src does not grow the copy
	src.addItem("three");
	TFPASS(v1.getItemCount() == 2);

	UT_GenericVector<const char *> v2;
	v2.addItem("junk");
	v2 = src;
	TFPASS(v2.getItemCount() == 3);
	TFPASS(v2[2] == src[2]);

	// self-assignment is a no-op, not a self-clear
	v2 = v2;
	TFPASS(v2.getItemCount() == 3);

	// copy() clears the target first, returns true on success
	UT_GenericVector<const char *> v3;
	TFPASS(v3.copy(&src));
	TFPASS(v3.getItemCount() == 3);
	TFPASS(v3[0] == src[0]);
	// copy of nullptr clears and reports failure
	TFPASS(!v3.copy(nullptr));
	TFPASS(v3.getItemCount() == 0);
}

TFTEST_MAIN("UT_Vector and UT_NumberVector")
{
	UT_Vector pv;
	pv.addItem(nullptr);
	pv.addItem(pv.getItemCount() == 1 ? &pv : nullptr);
	TFPASS(pv.getItemCount() == 2);
	TFPASS(pv.getNthItem(0) == nullptr);
	TFPASS(pv.getNthItem(1) == static_cast<const void *>(&pv));
	TFPASS(pv.getNthItem(5) == nullptr);

	UT_NumberVector nv;
	nv.addItem(7);
	nv.addItem(-3);
	nv.addItem(0);
	TFPASS(nv.getItemCount() == 3);
	TFPASS(nv.getNthItem(0) == 7);
	TFPASS(nv.getNthItem(1) == -3);
	TFPASS(nv.getNthItem(3) == 0);   // UT_null<UT_sint32> is 0
	TFPASS(nv.findItem(-3) == 1);
	TFPASS(nv.findItem(99) == -1);
	nv.sort([](UT_sint32 a, UT_sint32 b) { return a < b; });
	TFPASS(nv[0] == -3);
	TFPASS(nv[1] == 0);
	TFPASS(nv[2] == 7);
}

TFTEST_MAIN("growth and clear retain defined behavior")
{
	UT_GenericVector<UT_sint32> v(4, 4, true);

	// push past the doubling cutoff and the incremental range
	for (UT_sint32 i = 0; i < 40; ++i)
		TFPASS(v.addItem(i) == 0);
	TFPASS(v.getItemCount() == 40);
	for (UT_sint32 i = 0; i < 40; ++i)
		TFPASS(v.getNthItem(i) == i);

	// clear() empties (storage stays allocated, contents zeroed)
	v.clear();
	TFPASS(v.getItemCount() == 0);
	TFPASS(v.getNthItem(0) == 0);

	// refill after clear works
	TFPASS(v.addItem(1) == 0);
	TFPASS(v.getNthItem(0) == 1);
}

TFTEST_MAIN("ownership stays with the caller")
{
	// The container never deletes its elements: deleteNthItem,
	// pop_back, clear and the destructor all just drop the pointer.
	// (UT_VECTOR_PURGEALL exists precisely because callers own.)
	static int s_alive = 0;
	struct Tracked { ~Tracked() { --s_alive; } };

	UT_GenericVector<Tracked *> v;
	Tracked * dropped[4];
	v.addItem(dropped[0] = new Tracked); ++s_alive;
	v.addItem(dropped[1] = new Tracked); ++s_alive;
	v.addItem(dropped[2] = new Tracked); ++s_alive;

	v.deleteNthItem(1);
	v.pop_back();
	TFPASS(s_alive == 3);          // nothing destroyed
	v.clear();
	TFPASS(s_alive == 3);
	v.addItem(dropped[3] = new Tracked); ++s_alive;
	v.clear();
	TFPASS(s_alive == 4);

	// caller-side purge does the deleting
	UT_GenericVector<Tracked *> w;
	w.addItem(new Tracked); ++s_alive;
	w.addItem(new Tracked); ++s_alive;
	UT_VECTOR_PURGEALL(Tracked *, w);
	TFPASS(s_alive == 4);
	// purge deletes the elements but does NOT shrink the vector:
	// the slots still hold dangling pointers - callers are expected
	// to clear or destroy the vector afterwards
	TFPASS(w.getItemCount() == 2);

	// dropped pointers are still the caller's to reclaim
	for (Tracked * p : dropped)
		delete p;
	TFPASS(s_alive == 0);
}

