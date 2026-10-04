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
#include "ut_growbuf.h"

#define TFSUITE "core.af.util.growbuf"

static bool s_eq(const UT_GrowBuf & b, const UT_GrowBufElement * expect,
                 UT_uint32 len)
{
	if (b.getLength() != len)
		return false;
	for (UT_uint32 i = 0; i < len; ++i)
		if (b.getPointer(i)[0] != expect[i])
			return false;
	return true;
}

TFTEST_MAIN("append, ins and del")
{
	UT_GrowBuf b;
	const UT_GrowBufElement abc[] = {'a', 'b', 'c'};
	const UT_GrowBufElement xy[] = {'x', 'y'};
	const UT_GrowBufElement axbc[] = {'a', 'x', 'y', 'b', 'c'};

	TFPASS(b.getLength() == 0);
	TFPASS(b.getPointer(0) == nullptr);

	TFPASS(b.append(abc, 3));
	TFPASS(b.getLength() == 3);
	TFPASS(s_eq(b, abc, 3));

	// insert in the middle shifts the tail
	TFPASS(b.ins(1, xy, 2));
	TFPASS(b.getLength() == 5);
	TFPASS(s_eq(b, axbc, 5));

	// zero-length insert is a no-op
	TFPASS(b.ins(0, nullptr, 0));
	TFPASS(b.getLength() == 5);

	// inserting zeroed space
	TFPASS(b.ins(0, 2));
	TFPASS(b.getLength() == 7);
	TFPASS(b.getPointer(0)[0] == 0);
	TFPASS(b.getPointer(1)[0] == 0);
	TFPASS(b.getPointer(2)[0] == 'a');

	// insert beyond the end is rebased to the current end and the
	// slack counts into the copied length: the caller's array must
	// cover the whole span (position + length worth of elements)
	UT_GrowBuf pad;
	const UT_GrowBufElement gap[] = {'g', 'a', 'p', '_', '_', 'x', 'y'};
	TFPASS(pad.ins(5, gap, 2));
	TFPASS(pad.getLength() == 7);
	TFPASS(pad.getPointer(0)[0] == 'g');
	TFPASS(pad.getPointer(6)[0] == 'y');

	// the same rebase applies to the zeroing overload
	UT_GrowBuf zpad;
	TFPASS(zpad.ins(4, 2));
	TFPASS(zpad.getLength() == 6);   // slack 4 + length 2
	TFPASS(zpad.getPointer(0)[0] == 0);
	TFPASS(zpad.getPointer(5)[0] == 0);

	// delete a middle range
	const UT_GrowBufElement xbc[] = {'x', 'y', 'b', 'c'};
	UT_GrowBuf d;
	TFPASS(d.append(axbc, 5));
	TFPASS(d.del(0, 1));
	TFPASS(s_eq(d, xbc, 4));

	// delete to the end
	TFPASS(d.del(2, 2));
	TFPASS(d.getLength() == 2);
	TFPASS(d.getPointer(0)[0] == 'x');
	TFPASS(d.getPointer(1)[0] == 'y');

	// delete everything shrinks the buffer
	TFPASS(d.del(0, 2));
	TFPASS(d.getLength() == 0);
}

TFTEST_MAIN("del bounds are checked")
{
	UT_GrowBuf b;
	const UT_GrowBufElement abc[] = {'a', 'b', 'c'};
	TFPASS(b.append(abc, 3));

	// out-of-range deletes fail and leave the buffer intact
	TFPASS(!b.del(3, 1));      // position == size
	TFPASS(!b.del(1, 3));      // position+amount > size
	TFPASS(!b.del(9, 9));
	TFPASS(b.getLength() == 3);
	TFPASS(s_eq(b, abc, 3));

	// del on an empty/never-allocated buffer fails cleanly
	UT_GrowBuf empty;
	TFPASS(!empty.del(0, 1));
	TFPASS(empty.del(0, 0));   // zero amount is a no-op success

	// truncate
	UT_GrowBuf t;
	TFPASS(t.append(abc, 3));
	t.truncate(2);
	TFPASS(t.getLength() == 2);
	// truncating to a longer length is a no-op on the size
	t.truncate(9);
	TFPASS(t.getLength() == 2);
	// truncate(0) empties
	t.truncate(0);
	TFPASS(t.getLength() == 0);
	// truncate(0) on a never-allocated buffer is safe
	UT_GrowBuf t2;
	t2.truncate(0);
	TFPASS(t2.getLength() == 0);
}

TFTEST_MAIN("overwrite and getPointer bounds")
{
	UT_GrowBuf b;
	const UT_GrowBufElement abc[] = {'a', 'b', 'c'};
	const UT_GrowBufElement azz[] = {'a', 'z', 'z'};

	TFPASS(b.append(abc, 3));
	UT_GrowBufElement zcopy[] = {'z', 'z'};
	TFPASS(b.overwrite(1, zcopy, 2));
	TFPASS(s_eq(b, azz, 3));       // overwrite replaces in place, no shift

	// overwrite does not change the logical length -- it writes raw
	// cells into the allocated space, so data past m_iSize is
	// invisible to getLength()/getPointer()
	UT_GrowBufElement big[] = {1, 2, 3, 4, 5, 6, 7, 8};
	TFPASS(b.overwrite(4, big, 8));
	TFPASS(b.getLength() == 3);

	// getPointer: position == size is a legal one-past-end pointer,
	// beyond that is nullptr
	UT_GrowBuf small;
	const UT_GrowBufElement one[] = {42};
	TFPASS(small.append(one, 1));
	TFPASS(small.getPointer(0) != nullptr);
	TFPASS(small.getPointer(0)[0] == 42);
	TFPASS(small.getPointer(1) != nullptr);   // one-past-end
	TFPASS(small.getPointer(2) == nullptr);
}
