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

#include "config.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <gio/gio.h>

#include "xap_Zotero.h"

/* ----------------------------------------------------------------
 * transport: plain HTTP/1.0 GET to the loopback Zotero connector.
 * HTTP/1.0 has no chunked transfer coding - the body is
 * close-delimited, so no chunk decoding is needed (same rationale
 * as XAP_httpsGet).  Generous response cap: a 25-item search with
 * full bib output stays well under a megabyte.
 * ---------------------------------------------------------------- */

static std::string _zotero_host(void)
{
	const char * h = g_getenv("ABINOVA_ZOTERO_HOST");
	return (h && *h) ? h : "127.0.0.1";
}

static guint16 _zotero_port(void)
{
	const char * p = g_getenv("ABINOVA_ZOTERO_PORT");
	if (p && *p)
	{
		const long v = strtol(p, nullptr, 10);
		if (v > 0 && v < 65536)
			return static_cast<guint16>(v);
	}
	return 23119;
}

bool XAP_zoteroGet(const char * path, std::string & bodyOut,
				   std::string & errOut)
{
	bodyOut.clear();
	errOut.clear();

	const std::string host = _zotero_host();
	GError * err = nullptr;
	GSocketClient * client = g_socket_client_new();
	g_socket_client_set_timeout(client, 8);
	GSocketConnection * conn = g_socket_client_connect_to_host(
		client, host.c_str(), _zotero_port(), nullptr, &err);
	g_object_unref(client);
	if (!conn)
	{
		errOut = "cannot reach Zotero at ";
		errOut += host;
		if (err && err->message)
		{
			errOut += " (";
			errOut += err->message;
			errOut += ")";
		}
		errOut += " - is Zotero running?";
		g_clear_error(&err);
		return false;
	}

	std::string req = "GET ";
	req += path;
	req += " HTTP/1.0\r\nHost: ";
	req += host;
	req += "\r\nUser-Agent: abinova-zotero\r\n"
		   "Accept: application/json\r\n"
		   "Connection: close\r\n\r\n";

	GOutputStream * out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
	GInputStream * in = g_io_stream_get_input_stream(G_IO_STREAM(conn));
	bool ok = g_output_stream_write_all(out, req.data(), req.size(),
										nullptr, nullptr, &err) == TRUE;
	if (!ok)
	{
		errOut = "sending the request to Zotero failed";
		g_clear_error(&err);
	}
	if (ok)
	{
		std::string resp;
		char buf[8192];
		gssize n;
		while ((n = g_input_stream_read(in, buf, sizeof(buf),
										nullptr, &err)) > 0)
		{
			resp.append(buf, n);
			if (resp.size() > 8 * 1024 * 1024)
			{
				errOut = "Zotero sent an unreasonably large response";
				ok = false;
				break;
			}
		}
		if (ok && n < 0)
		{
			errOut = "the connection to Zotero broke mid-response";
			g_clear_error(&err);
			ok = false;
		}

		if (ok)
		{
			const size_t sp = resp.find(' ');
			const long status = (sp == std::string::npos)
				? 0 : strtol(resp.c_str() + sp + 1, nullptr, 10);
			const size_t bodyPos = resp.find("\r\n\r\n");
			ok = status >= 200 && status < 300 &&
				bodyPos != std::string::npos;
			if (!ok)
			{
				if (bodyPos == std::string::npos)
					errOut = "Zotero sent a malformed HTTP response";
				else
				{
					errOut = "Zotero answered with HTTP status ";
					errOut += std::to_string(status);
				}
			}
			else
				bodyOut = resp.substr(bodyPos + 4);
		}
	}
	g_io_stream_close(G_IO_STREAM(conn), nullptr, nullptr);
	g_object_unref(conn);
	return ok;
}

/* ----------------------------------------------------------------
 * minimal JSON parser - enough for the Zotero REST payloads
 * (objects, arrays, strings with \uXXXX escapes, numbers, true/
 * false/null).  Depth-capped against hostile nesting; NUL bytes in
 * the input make the parse fail rather than truncate silently.
 * ---------------------------------------------------------------- */

struct _JVal
{
	enum Type : uint8_t { T_NUL, T_FALSE, T_TRUE, T_NUM, T_STR, T_ARR, T_OBJ };
	Type t = T_NUL;
	std::string s;                              /* STR text / NUM literal */
	std::vector<_JVal> a;                       /* ARR */
	std::map<std::string, _JVal> o;             /* OBJ */

	const _JVal * find(const char * k) const
	{
		if (t != T_OBJ)
			return nullptr;
		auto it = o.find(k);
		return it == o.end() ? nullptr : &it->second;
	}
};

struct _JParse
{
	const char * p;
	const char * end;
	unsigned depth = 0;
};

static void _j_ws(_JParse & s)
{
	while (s.p < s.end && (*s.p == ' ' || *s.p == '\t' ||
						   *s.p == '\r' || *s.p == '\n'))
		++s.p;
}

static bool _j_lit(_JParse & s, const char * lit)
{
	const size_t n = strlen(lit);
	if (static_cast<size_t>(s.end - s.p) < n ||
		memcmp(s.p, lit, n) != 0)
		return false;
	s.p += n;
	return true;
}

static bool _j_string(_JParse & s, std::string & out)
{
	if (s.p >= s.end || *s.p != '"')
		return false;
	++s.p;
	out.clear();
	while (s.p < s.end)
	{
		const unsigned char c = static_cast<unsigned char>(*s.p++);
		if (c == '"')
			return true;
		if (c == '\\')
		{
			if (s.p >= s.end)
				return false;
			const char e = *s.p++;
			switch (e)
			{
			case '"':  out += '"';  break;
			case '\\': out += '\\'; break;
			case '/':  out += '/';  break;
			case 'b':  out += '\b'; break;
			case 'f':  out += '\f'; break;
			case 'n':  out += '\n'; break;
			case 'r':  out += '\r'; break;
			case 't':  out += '\t'; break;
			case 'u':
			{
				if (s.end - s.p < 4)
					return false;
				unsigned cp = 0;
				for (int i = 0; i < 4; ++i)
				{
					const char h = *s.p++;
					cp <<= 4;
					if (h >= '0' && h <= '9')      cp += h - '0';
					else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
					else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
					else return false;
				}
				/* surrogate pairs collapse into the scalar */
				if (cp >= 0xD800 && cp <= 0xDBFF)
				{
					if (s.end - s.p >= 6 && s.p[0] == '\\' &&
						s.p[1] == 'u')
					{
						unsigned lo = 0;
						bool ok = true;
						for (int i = 2; i < 6 && ok; ++i)
						{
							const char h = s.p[i];
							lo <<= 4;
							if (h >= '0' && h <= '9')      lo += h - '0';
							else if (h >= 'a' && h <= 'f') lo += h - 'a' + 10;
							else if (h >= 'A' && h <= 'F') lo += h - 'A' + 10;
							else ok = false;
						}
						if (ok && lo >= 0xDC00 && lo <= 0xDFFF)
						{
							cp = 0x10000 + ((cp - 0xD800) << 10) +
								(lo - 0xDC00);
							s.p += 6;
						}
						else
							cp = 0xFFFD;
					}
					else
						cp = 0xFFFD;
				}
				else if (cp >= 0xDC00 && cp <= 0xDFFF)
					cp = 0xFFFD;
				char enc[8];
				const gint n = g_unichar_to_utf8(
					static_cast<gunichar>(cp), enc);
				out.append(enc, n);
				break;
			}
			default:
				return false;
			}
		}
		else if (c < 0x20)
			return false;               /* raw control chars are invalid */
		else
			out += static_cast<char>(c);
	}
	return false;
}

static bool _j_value(_JParse & s, _JVal & v)
{
	if (++s.depth > 64)
		return false;
	_j_ws(s);
	bool ok = false;
	if (s.p < s.end)
	{
		const char c = *s.p;
		if (c == '{')
		{
			++s.p;
			v.t = _JVal::T_OBJ;
			_j_ws(s);
			ok = true;
			if (s.p < s.end && *s.p == '}')
				++s.p;
			else while (ok)
			{
				std::string k;
				if (!_j_string(s, k)) { ok = false; break; }
				_j_ws(s);
				if (s.p >= s.end || *s.p != ':') { ok = false; break; }
				++s.p;
				if (!_j_value(s, v.o[k])) { ok = false; break; }
				_j_ws(s);
				if (s.p < s.end && *s.p == ',') { ++s.p; _j_ws(s); continue; }
				if (s.p < s.end && *s.p == '}') { ++s.p; break; }
				ok = false;
			}
		}
		else if (c == '[')
		{
			++s.p;
			v.t = _JVal::T_ARR;
			_j_ws(s);
			ok = true;
			if (s.p < s.end && *s.p == ']')
				++s.p;
			else while (ok)
			{
				v.a.emplace_back();
				if (!_j_value(s, v.a.back())) { ok = false; break; }
				_j_ws(s);
				if (s.p < s.end && *s.p == ',') { ++s.p; continue; }
				if (s.p < s.end && *s.p == ']') { ++s.p; break; }
				ok = false;
			}
		}
		else if (c == '"')
		{
			v.t = _JVal::T_STR;
			ok = _j_string(s, v.s);
		}
		else if (c == 't')
		{
			ok = _j_lit(s, "true");
			if (ok) v.t = _JVal::T_TRUE;
		}
		else if (c == 'f')
		{
			ok = _j_lit(s, "false");
			if (ok) v.t = _JVal::T_FALSE;
		}
		else if (c == 'n')
		{
			ok = _j_lit(s, "null");
			if (ok) v.t = _JVal::T_NUL;
		}
		else
		{
			const char * q = s.p;
			while (q < s.end && (isdigit(static_cast<unsigned char>(*q)) ||
								 *q == '-' || *q == '+' || *q == '.' ||
								 *q == 'e' || *q == 'E'))
				++q;
			if (q > s.p)
			{
				v.t = _JVal::T_NUM;
				v.s.assign(s.p, q - s.p);
				s.p = q;
				ok = true;
			}
		}
	}
	--s.depth;
	return ok;
}

static bool _j_str_at(const _JVal & v, const char * k, std::string & out)
{
	const _JVal * m = v.find(k);
	if (m && m->t == _JVal::T_STR)
	{
		out = m->s;
		return true;
	}
	return false;
}

/* ----------------------------------------------------------------
 * CSL output arrives as HTML fragments ("<i>…</i>",
 * "<div class=\"csl-entry\">…").  Strip tags, decode the entity set
 * citeproc actually emits, and fold whitespace.
 * ---------------------------------------------------------------- */

static void _ent(std::string & out, const std::string & e)
{
	if (e == "amp")       out += '&';
	else if (e == "lt")   out += '<';
	else if (e == "gt")   out += '>';
	else if (e == "quot") out += '"';
	else if (e == "apos" || e == "39") out += '\'';
	else if (e == "nbsp") out += ' ';
	else if (!e.empty() && e[0] == '#')
	{
		unsigned cp = 0;
		if (e.size() > 1 && (e[1] == 'x' || e[1] == 'X'))
			cp = static_cast<unsigned>(
				strtoul(e.c_str() + 2, nullptr, 16));
		else
			cp = static_cast<unsigned>(
				strtoul(e.c_str() + 1, nullptr, 10));
		char enc[8];
		const gint n = g_unichar_to_utf8(
			cp <= 0x10FFFF ? static_cast<gunichar>(cp) : 0xFFFD, enc);
		out.append(enc, n);
	}
	else
	{
		out += '&';
		out += e;
		out += ';';
	}
}

std::string XAP_zoteroStripHtml(const std::string & in)
{
	std::string out;
	out.reserve(in.size());
	bool tag = false;
	for (size_t i = 0; i < in.size(); ++i)
	{
		const char c = in[i];
		if (tag)
		{
			if (c == '>')
				tag = false;
			continue;
		}
		if (c == '<')
		{
			tag = true;
			continue;
		}
		if (c == '&')
		{
			const size_t sc = in.find(';', i + 1);
			if (sc != std::string::npos && sc - i < 12)
			{
				_ent(out, in.substr(i + 1, sc - i - 1));
				i = sc;
				continue;
			}
		}
		out += c;
	}
	/* fold whitespace runs + trim */
	std::string r;
	bool ws = false;
	for (const char c : out)
	{
		if (isspace(static_cast<unsigned char>(c)))
		{
			ws = !r.empty();
			continue;
		}
		if (ws)
		{
			r += ' ';
			ws = false;
		}
		r += c;
	}
	return r;
}

/* ----------------------------------------------------------------
 * item mapping: data.title is the display name; citation/bib are
 * the CSL-rendered forms.  When the service omits them (older
 * local API without include=citation, or a style it lacks) compose
 * a plain "Last, First (Year). Title." so the document still gets
 * useful text.
 * ---------------------------------------------------------------- */

static void _item_from_json(const _JVal & obj, XAP_ZoteroItem & it)
{
	_j_str_at(obj, "key", it.key);
	std::string bib, cit, title;
	_j_str_at(obj, "bib", bib);
	_j_str_at(obj, "citation", cit);
	it.bib = XAP_zoteroStripHtml(bib);
	it.citation = XAP_zoteroStripHtml(cit);

	const _JVal * data = obj.find("data");
	if (data)
		_j_str_at(*data, "title", title);
	it.title = title;

	if (!it.bib.empty() || !it.citation.empty())
		return;

	/* composed fallback */
	std::string who, when;
	if (data)
	{
		const _JVal * creators = data->find("creators");
		if (creators && creators->t == _JVal::T_ARR &&
			!creators->a.empty())
		{
			const _JVal & c0 = creators->a.front();
			std::string last, first, name;
			_j_str_at(c0, "lastName", last);
			_j_str_at(c0, "firstName", first);
			_j_str_at(c0, "name", name);
			who = !last.empty()
				? (first.empty() ? last : last + ", " + first)
				: name;
			if (creators->a.size() > 1)
				who += " et al.";
		}
		std::string date;
		_j_str_at(*data, "date", date);
		if (date.size() >= 4)
			when = date.substr(0, 4);
		else
			when = date;
	}
	std::string s = who;
	if (!when.empty())
		s += (s.empty() ? "" : " ") + std::string("(") + when + ").";
	if (!it.title.empty())
		s += (s.empty() ? "" : " ") + it.title + ".";
	if (s.empty())
		s = it.key;
	it.bib = s;
	if (it.citation.empty())
	{
		std::string last = who;
		const size_t comma = last.find(',');
		if (comma != std::string::npos)
			last = last.substr(0, comma);
		it.citation = "(" + last +
			(when.empty() ? "" : ", " + when) + ")";
	}
}

static std::string _style_query(const char * style)
{
	if (!style || !*style)
		return std::string();
	gchar * enc = g_uri_escape_string(style, nullptr, FALSE);
	std::string q = "&style=";
	q += enc;
	g_free(enc);
	return q;
}

bool XAP_zoteroSearch(const std::string & query,
					  const char * style,
					  std::vector<XAP_ZoteroItem> & out,
					  std::string & err)
{
	gchar * enc = g_uri_escape_string(query.c_str(), nullptr, FALSE);
	std::string path = "/api/users/0/items?q=";
	path += enc;
	g_free(enc);
	path += "&qmode=everything&format=json"
			"&include=data,bib,citation&limit=25";
	path += _style_query(style);

	std::string body;
	if (!XAP_zoteroGet(path.c_str(), body, err))
		return false;

	_JParse s{ body.c_str(), body.c_str() + body.size(), 0 };
	_JVal root;
	if (!_j_value(s, root) || root.t != _JVal::T_ARR)
	{
		err = "Zotero returned an unreadable response";
		return false;
	}
	std::vector<XAP_ZoteroItem> items;
	for (const _JVal & obj : root.a)
	{
		XAP_ZoteroItem it;
		_item_from_json(obj, it);
		if (!it.key.empty())
			items.push_back(std::move(it));
	}
	out.swap(items);
	return true;
}

bool XAP_zoteroFetchItem(const std::string & key,
						 const char * style,
						 XAP_ZoteroItem & out,
						 std::string & err)
{
	gchar * enc = g_uri_escape_string(key.c_str(), nullptr, FALSE);
	std::string path = "/api/users/0/items/";
	path += enc;
	g_free(enc);
	path += "?format=json&include=data,bib,citation";
	path += _style_query(style);

	std::string body;
	if (!XAP_zoteroGet(path.c_str(), body, err))
		return false;

	_JParse s{ body.c_str(), body.c_str() + body.size(), 0 };
	_JVal root;
	if (!_j_value(s, root) || root.t != _JVal::T_OBJ)
	{
		err = "Zotero returned an unreadable response";
		return false;
	}
	_item_from_json(root, out);
	if (out.key.empty())
		out.key = key;
	if (out.citation.empty() && out.bib.empty())
	{
		err = "Zotero found no such item";
		return false;
	}
	return true;
}
