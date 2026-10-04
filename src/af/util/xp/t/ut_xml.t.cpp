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

#include <string.h>
#include <string>
#include <vector>

#include "tf_test.h"
#include "ut_xml.h"
#include "ut_misc.h"

#define TFSUITE "core.af.util.xml"

namespace
{

// Recording listener: captures the event stream so the test can assert
// exact callback order, attribute arrays, and character data.
class RecListener : public UT_XML::Listener
{
public:
	virtual void startElement(const gchar* name, const gchar** atts) override
	{
		events.push_back(std::string("S:") + name);
		for (const gchar** p = atts; p && *p; p += 2)
			attrs.push_back(std::string(p[0]) + "=" + p[1]);
	}
	virtual void endElement(const gchar* name) override
	{
		events.push_back(std::string("E:") + name);
	}
	virtual void charData(const gchar* buffer, int length) override
	{
		text.append(buffer, length);
	}

	std::vector<std::string> events;
	std::vector<std::string> attrs;
	std::string text;
};

} // namespace

TFTEST_MAIN("UT_XML parse element and text callbacks")
{
	RecListener l;
	UT_XML parser;
	parser.setListener(&l);

	static const char doc[] =
		"<root a=\"1\" b=\"two\"><child>hello</child><empty/></root>";
	UT_Error err = parser.parse(doc, static_cast<UT_uint32>(strlen(doc)));
	TFPASS(err == UT_OK);

	// strict event order; self-closing elements fire both callbacks
	static const char* expected[] = {
		"S:root", "S:child", "E:child", "S:empty", "E:empty", "E:root"
	};
	TFPASS(l.events.size() == G_N_ELEMENTS(expected));
	if (l.events.size() == G_N_ELEMENTS(expected))
		for (size_t i = 0; i < G_N_ELEMENTS(expected); i++)
			TFPASS(l.events[i] == expected[i]);

	// attributes arrive as [name, value] pairs in document order
	TFPASS(l.attrs.size() == 2);
	TFPASS(l.attrs[0] == "a=1");
	TFPASS(l.attrs[1] == "b=two");

	// char data may be split across callbacks; concatenation is exact
	TFPASS(l.text == "hello");
}

TFTEST_MAIN("UT_XML parse errors are reported")
{
	RecListener l;
	UT_XML parser;
	parser.setListener(&l);

	// unbalanced element -> not well-formed -> IE error
	static const char bad[] = "<root><child></root>";
	TFPASS(parser.parse(bad, static_cast<UT_uint32>(strlen(bad))) != UT_OK);

	// nullptr buffer / zero length / no listener are rejected up front
	TFPASS(parser.parse(nullptr, 5) == UT_ERROR);
	TFPASS(parser.parse("<a/>", 0) == UT_ERROR);
	UT_XML noListener;
	TFPASS(noListener.parse("<a/>", 4) == UT_ERROR);
}

TFTEST_MAIN("UT_XML blocks external entities (XXE)")
{
	RecListener l;
	UT_XML parser;
	parser.setListener(&l);

	// The untrusted-parse scope refuses ALL external entity loads, so
	// this SYSTEM entity can neither be read off disk nor fetched.
	// The parse may fail outright or expand to nothing - either is
	// fine; what must never happen is /etc/passwd leaking into text.
	static const char xxe[] =
		"<?xml version=\"1.0\"?>"
		"<!DOCTYPE r [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]>"
		"<r>&xxe;</r>";
	parser.parse(xxe, static_cast<UT_uint32>(strlen(xxe)));
	TFPASS(l.text.find("root:") == std::string::npos);
	TFPASS(l.text.find("/bin/") == std::string::npos);
}

TFTEST_MAIN("UT_XML_Decode entity decoding")
{
	// wraps the input in <d k="..."/> and returns the attribute value
	// with XML entities resolved
	{
		// quirk worth pinning: libxml2's SAX1 attribute layer reports
		// values XML-escaped, so a decoded '&' arrives re-escaped as
		// "&#38;" while '<'/'>' come through plainly.  UT_XML_Decode
		// output is NOT a plain-text decode.
		char* v = UT_XML_Decode("a&amp;b&lt;c");
		TFPASS(v != nullptr);
		TFPASS(strcmp(v, "a&#38;b<c") == 0);
		g_free(v);
	}
	{
		char* v = UT_XML_Decode("x&#65;y&gt;z");
		TFPASS(v != nullptr);
		TFPASS(strcmp(v, "xAy>z") == 0);
		g_free(v);
	}
	{
		char* v = UT_XML_Decode("plain");
		TFPASS(v != nullptr);
		TFPASS(strcmp(v, "plain") == 0);
		g_free(v);
	}
}
