/* AbiSource Application Framework
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

#include <climits>

#include "tf_test.h"
#include "xap_UpdateCheck.h"

#define TFSUITE "core.af.xap.updatecheck"

TFTEST_MAIN("XAP_parseVersionTag")
{
	int v[3];

	TFPASS(XAP_parseVersionTag("v4.0.0", v));
	TFPASS(v[0] == 4 && v[1] == 0 && v[2] == 0);

	TFPASS(XAP_parseVersionTag("4.1.3", v));
	TFPASS(v[0] == 4 && v[1] == 1 && v[2] == 3);

	/* leading junk before the first digit is skipped */
	TFPASS(XAP_parseVersionTag("release-2024.2", v));
	TFPASS(v[0] == 2024 && v[1] == 2 && v[2] == 0);

	/* a fourth component is ignored */
	TFPASS(XAP_parseVersionTag("1.2.3.4", v));
	TFPASS(v[0] == 1 && v[1] == 2 && v[2] == 3);

	/* suffixes end the parse */
	TFPASS(XAP_parseVersionTag("4.0.0-beta1", v));
	TFPASS(v[0] == 4 && v[1] == 0 && v[2] == 0);

	/* no digits at all -> not a version */
	TFFAIL(XAP_parseVersionTag("", v));
	TFFAIL(XAP_parseVersionTag("latest", v));
	TFFAIL(XAP_parseVersionTag("v.", v));

	/* pathological digit runs must saturate, not overflow */
	TFPASS(XAP_parseVersionTag("v99999999999999999999.1.0", v));
	TFPASS(v[0] == INT_MAX && v[1] == 1 && v[2] == 0);
	TFPASS(XAP_parseVersionTag("v4.99999999999999999998.0", v));
	TFPASS(v[0] == 4 && v[1] == INT_MAX && v[2] == 0);
	TFPASS(XAP_parseVersionTag("v2147483647.0.0", v));
	TFPASS(v[0] == INT_MAX);
	TFPASS(XAP_parseVersionTag("v2147483648.0.0", v));
	TFPASS(v[0] == INT_MAX);
}

TFTEST_MAIN("XAP_jsonStringValue")
{
	const std::string doc =
		"{\"tag_name\":\"v4.1.0\",\"html_url\":\"https://x/y\",\"n\":3}";

	TFPASS(XAP_jsonStringValue(doc, "tag_name") == "v4.1.0");
	TFPASS(XAP_jsonStringValue(doc, "html_url") == "https://x/y");

	/* missing key, non-string value, malformed fragments */
	TFPASS(XAP_jsonStringValue(doc, "absent").empty());
	TFPASS(XAP_jsonStringValue(doc, "n").empty());
	TFPASS(XAP_jsonStringValue("", "tag_name").empty());
	TFPASS(XAP_jsonStringValue("{\"a\":", "a").empty());
	TFPASS(XAP_jsonStringValue("{\"a\":\"unterminated", "a").empty());
}
