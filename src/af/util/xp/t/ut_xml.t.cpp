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

#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "tf_test.h"
#include "ut_xml.h"
#include "ut_misc.h"
#include "ut_bytebuf.h"

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

// Recording expert listener: captures the extended SAX stream
// (PIs, comments, CDATA, default data).
class RecExpertListener : public UT_XML::ExpertListener
{
public:
	virtual void StartElement(const gchar* name, const gchar** /*atts*/) override
		{ events.push_back(std::string("S:") + name); }
	virtual void EndElement(const gchar* name) override
		{ events.push_back(std::string("E:") + name); }
	virtual void CharData(const gchar* buffer, int length) override
		{ events.push_back(std::string("T:") + std::string(buffer, length)); }
	virtual void ProcessingInstruction(const gchar* target, const gchar* data) override
		{ events.push_back(std::string("PI:") + target + "|" + (data ? data : "")); }
	virtual void Comment(const gchar* data) override
		{ events.push_back(std::string("C:") + (data ? data : "")); }
	virtual void StartCdataSection() override
		{ events.push_back("CDATA+"); }
	virtual void EndCdataSection() override
		{ events.push_back("CDATA-"); }
	virtual void Default(const gchar* buffer, int length) override
		{ events.push_back(std::string("D:") + std::string(buffer, length)); }

	std::vector<std::string> events;
};

static UT_ByteBuf * bbOf(const char * s)
{
	UT_ByteBuf * bb = new UT_ByteBuf;
	bb->append(reinterpret_cast<const UT_Byte *>(s),
			   static_cast<UT_uint32>(strlen(s)));
	return bb;
}

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

TFTEST_MAIN("UT_XML::sniff")
{
	// root element matching the requested type -> true
	static const char doc[] = "<svg width=\"1\"/>";
	TFPASS(UT_XML().sniff(doc, static_cast<UT_uint32>(strlen(doc)), "svg"));

	// a different root -> false
	TFPASS(!UT_XML().sniff(doc, static_cast<UT_uint32>(strlen(doc)), "html"));

	// non-XML input -> false
	TFPASS(!UT_XML().sniff("not xml", 7, "svg"));

	// null args rejected
	TFPASS(!UT_XML().sniff(nullptr, 5, "svg"));
	TFPASS(!UT_XML().sniff(doc, 5, nullptr));

	// ByteBuf overload
	UT_ByteBuf * bb = bbOf(doc);
	TFPASS(UT_XML().sniff(bb, "svg"));
	delete bb;
}

TFTEST_MAIN("UT_XML namespace stripping")
{
	RecListener l;
	UT_XML parser;
	parser.setListener(&l);
	parser.setNameSpace("svg");

	// "svg:" prefixes are stripped from element names for the listener
	static const char doc[] =
		"<svg:svg xmlns:svg=\"x\"><svg:rect/><other/></svg:svg>";
	TFPASS(parser.parse(doc, static_cast<UT_uint32>(strlen(doc))) == UT_OK);

	TFPASS(l.events.size() == 6);
	if (l.events.size() == 6)
	{
		TFPASS(l.events[0] == "S:svg");
		TFPASS(l.events[1] == "S:rect");
		TFPASS(l.events[2] == "E:rect");
		TFPASS(l.events[3] == "S:other");
		TFPASS(l.events[4] == "E:other");
		TFPASS(l.events[5] == "E:svg");
	}
}

TFTEST_MAIN("UT_XML parse from UT_ByteBuf")
{
	RecListener l;
	UT_XML parser;
	parser.setListener(&l);

	static const char doc[] = "<r><x>buf</x></r>";
	UT_ByteBuf * bb = bbOf(doc);
	TFPASS(parser.parse(bb) == UT_OK);
	TFPASS(l.events.size() == 4);
	TFPASS(l.text == "buf");
	delete bb;

	// null buffer / no listener
	TFPASS(parser.parse(static_cast<const UT_ByteBuf *>(nullptr)) == UT_ERROR);
	UT_XML noListener;
	UT_ByteBuf * bb2 = bbOf("<a/>");
	TFPASS(noListener.parse(bb2) == UT_ERROR);
	delete bb2;
}

TFTEST_MAIN("UT_XML parse from file and reader")
{
	RecListener l;

	gchar * tmpl = g_strdup("ut_xml_XXXXXX");
	gchar * dir = g_dir_make_tmp(tmpl, nullptr);
	g_free(tmpl);
	TFPASS(dir != nullptr);
	if (!dir) return;

	std::string path = std::string(dir) + "/doc.xml";
	FILE * f = fopen(path.c_str(), "w");
	fputs("<f><i>file</i></f>", f);
	fclose(f);

	// filename parse goes through DefaultReader
	UT_XML p1;
	p1.setListener(&l);
	TFPASS(p1.parse(path.c_str()) == UT_OK);
	TFPASS(l.text == "file");

	// a custom reader replaces the file source entirely
	static const char override[] = "<f><i>reader</i></f>";
	UT_XML_BufReader bufReader(override,
							   static_cast<UT_uint32>(strlen(override)));
	l.events.clear();
	l.text.clear();
	UT_XML p2;
	p2.setListener(&l);
	p2.setReader(&bufReader);
	TFPASS(p2.parse(path.c_str()) == UT_OK);
	TFPASS(l.text == "reader");

	// a missing file -> error
	UT_XML p3;
	p3.setListener(&l);
	TFPASS(p3.parse((std::string(dir) + "/absent.xml").c_str()) != UT_OK);

	g_remove(path.c_str());
	g_rmdir(dir);
	g_free(dir);
}

TFTEST_MAIN("UT_XML_BufReader standalone")
{
	static const char data[] = "abcdef";
	UT_XML_BufReader r(data, 6);

	// openFile resets the read cursor
	TFPASS(r.openFile("ignored"));

	char buf[4];
	TFPASS(r.readBytes(buf, 4) == 4);
	TFPASS(memcmp(buf, "abcd", 4) == 0);
	// a second read continues and clamps at the end
	TFPASS(r.readBytes(buf, 4) == 2);
	TFPASS(memcmp(buf, "ef", 2) == 0);
	TFPASS(r.readBytes(buf, 4) == 0);
	r.closeFile();

	// zero-length reads and null buffer are rejected
	TFPASS(r.readBytes(buf, 0) == 0);
	TFPASS(r.readBytes(nullptr, 4) == 0);

	// an empty reader can't open
	UT_XML_BufReader empty(nullptr, 0);
	TFPASS(!empty.openFile("x"));
}

TFTEST_MAIN("UT_XML expert listener sees extended events")
{
	RecExpertListener l;
	UT_XML parser;
	parser.setExpertListener(&l);

	static const char doc[] =
		"<?xml version=\"1.0\"?>"
		"<?pi-target some data?>"
		"<r>t1<!--a comment-->"
		"<![CDATA[raw <notags>]]>"
		"t2</r>";
	TFPASS(parser.parse(doc, static_cast<UT_uint32>(strlen(doc))) == UT_OK);

	// collect the event kinds into one joined string for matching
	std::string joined;
	for (const auto & e : l.events) { joined += e; joined += "|"; }

	TFPASS(joined.find("PI:pi-target|some data") != std::string::npos);
	TFPASS(joined.find("S:r|") != std::string::npos);
	TFPASS(joined.find("T:t1") != std::string::npos);
	TFPASS(joined.find("C:a comment") != std::string::npos);
	TFPASS(joined.find("CDATA+") != std::string::npos);
	TFPASS(joined.find("T:raw <notags>") != std::string::npos);
	TFPASS(joined.find("CDATA-") != std::string::npos);
	TFPASS(joined.find("E:r|") != std::string::npos);
}

TFTEST_MAIN("UT_XML stop() suppresses further callbacks")
{
	// listener that stops the parse at the second element
	struct StopListener : public UT_XML::Listener
	{
		UT_XML * parser = nullptr;
		int starts = 0;
		virtual void startElement(const gchar*, const gchar**) override
		{
			if (++starts == 2) parser->stop();
		}
		virtual void endElement(const gchar*) override {}
		virtual void charData(const gchar*, int) override {}
	};

	StopListener l;
	UT_XML parser;
	l.parser = &parser;
	parser.setListener(&l);

	static const char doc[] = "<a><b><c/><d/></b></a>";
	parser.parse(doc, static_cast<UT_uint32>(strlen(doc)));
	TFPASS(l.starts == 2);
}

TFTEST_MAIN("UT_XML error counters")
{
	UT_XML parser;
	TFPASS(parser.getNumMinorErrors() == 0);
	TFPASS(parser.getNumRecoveredErrors() == 0);
	parser.incMinorErrors();
	parser.incMinorErrors();
	parser.incRecoveredErrors();
	TFPASS(parser.getNumMinorErrors() == 2);
	TFPASS(parser.getNumRecoveredErrors() == 1);
}
