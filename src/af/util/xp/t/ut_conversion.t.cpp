/* AbiSource Program Utilities
 * Copyright (C) 2026 Abinova developers
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

#include <string>

#include "tf_test.h"
#include "ut_conversion.h"

#define TFSUITE "core.af.util.conversion"

TFTEST_MAIN("toType / tostr")
{
	TFPASS(toType<int>("42") == 42);
	TFPASS(toType<int>(std::string("-7")) == -7);
	TFPASS(toType<double>("2.5") > 2.49 && toType<double>("2.5") < 2.51);

	TFPASS(tostr(42) == "42");
	TFPASS(tostr(-3) == "-3");
	TFPASS(tostr(std::string("s")) == "s");

	// round trip
	TFPASS(toType<int>(tostr(12345)) == 12345);
}
