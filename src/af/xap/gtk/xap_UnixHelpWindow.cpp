/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiSource Application Framework
 * Copyright (C) 2024 Abinova contributors
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

#include <string.h>

#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "xap_UnixHelpWindow.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"

namespace
{

/* drop <head>, <script>, <style>, comments and other non-body parts */
static std::string bodyOnly(const std::string& html)
{
	std::string s = html;
	for (const char * tag : {"head", "script", "style"})
	{
		std::string open = "<"; open += tag;
		std::string close = "</"; close += tag; close += ">";
		for (;;)
		{
			size_t b = s.find(open);
			if (b == std::string::npos)
				break;
			size_t e = s.find(close, b);
			if (e == std::string::npos)
			{
				s.erase(b);
				break;
			}
			s.erase(b, e + close.size() - b);
		}
	}
	for (;;)
	{
		size_t b = s.find("<!--");
		if (b == std::string::npos)
			break;
		size_t e = s.find("-->", b + 4);
		if (e == std::string::npos)
		{
			s.erase(b);
			break;
		}
		s.erase(b, e + 3 - b);
	}
	return s;
}

static const std::pair<const char*, const char*> s_entities[] =
{
	{"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""},
	{"apos", "'"}, {"nbsp", "\xc2\xa0"}, {"copy", "\xc2\xa9"},
	{"reg", "\xc2\xae"}, {"eacute", "\xc3\xa9"}, {"egrave", "\xc3\xa8"},
	{"agrave", "\xc3\xa0"}, {"ccedil", "\xc3\xa7"}, {"laquo", "\xc2\xab"},
	{"raquo", "\xc2\xbb"}, {"ndash", "\xe2\x80\x93"},
	{"mdash", "\xe2\x80\x94"}, {"hellip", "\xe2\x80\xa6"},
	{"euro", "\xe2\x82\xac"}, {"middot", "\xc2\xb7"},
	{"sect", "\xc2\xa7"}, {"para", "\xc2\xb6"}, {"deg", "\xc2\xb0"},
};

static std::string decodeEntities(const std::string& in)
{
	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i)
	{
		if (in[i] != '&')
		{
			out += in[i];
			continue;
		}
		size_t semi = in.find(';', i + 1);
		if (semi == std::string::npos || semi - i > 12)
		{
			out += '&';
			continue;
		}
		std::string name = in.substr(i + 1, semi - i - 1);
		if (name[0] == '#')
		{
			long cp = name.size() > 1 && (name[1] == 'x' || name[1] == 'X')
				? strtol(name.c_str() + 2, nullptr, 16)
				: strtol(name.c_str() + 1, nullptr, 10);
			if (cp > 0 && cp < 0x110000)
			{
				char ubuf[8] = {0};
				g_unichar_to_utf8((gunichar)cp, ubuf);
				out += ubuf;
				i = semi;
				continue;
			}
			out += '&';
			continue;
		}
		bool found = false;
		for (const auto& e : s_entities)
		{
			if (name == e.first)
			{
				out += e.second;
				found = true;
				break;
			}
		}
		if (!found)
		{
			out += '&';
			continue;
		}
		i = semi;
	}
	return out;
}

/* read the value of an attribute inside a tag body, e.g. href="x" */
static std::string tagAttr(const std::string& tag, const char * attr)
{
	std::string key = attr;
	key += '=';
	size_t p = 0;
	while ((p = tag.find(key, p)) != std::string::npos)
	{
		/* must be preceded by whitespace so "xml:lang" can't match "lang" */
		if (p == 0 || !g_ascii_isspace(tag[p - 1]))
		{
			++p;
			continue;
		}
		size_t v = p + key.size();
		char q = v < tag.size() ? tag[v] : 0;
		if (q == '"' || q == '\'')
		{
			size_t e = tag.find(q, v + 1);
			if (e != std::string::npos)
				return tag.substr(v + 1, e - v - 1);
		}
		else
		{
			size_t e = tag.find_first_of(" \t>", v);
			return tag.substr(v, e - v);
		}
		break;
	}
	return std::string();
}

} // anonymous namespace

XAP_UnixHelpWindow::XAP_UnixHelpWindow(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wWindow(nullptr)
	, m_wBack(nullptr)
	, m_wSearch(nullptr)
	, m_wText(nullptr)
	, m_lang("en-US")
	, m_iSearchTimer(0)
{
}

XAP_UnixHelpWindow::~XAP_UnixHelpWindow()
{
	if (m_iSearchTimer)
		g_source_remove(m_iSearchTimer);
	if (m_wWindow)
		gtk_window_destroy(GTK_WINDOW(m_wWindow));
}

std::string XAP_UnixHelpWindow::_langDir() const
{
	std::string dir = XAP_App::getApp()->getAbiSuiteLibDir();
	dir += "/help/";
	dir += m_lang;
	return dir;
}

/* keep the browser inside the help dir: strip ".." and leading "/" */
std::string XAP_UnixHelpWindow::_resolve(const std::string& rel) const
{
	std::string r = rel;
	for (;;)
	{
		size_t p = r.find("../");
		if (p == std::string::npos)
			break;
		r.erase(p, 3);
	}
	while (!r.empty() && (r[0] == '/' || r[0] == '.'))
		r.erase(0, 1);
	return _langDir() + "/" + r;
}

void XAP_UnixHelpWindow::_navigate(const std::string& rel, bool bRecord)
{
	if (rel.empty())
		return;
	if (bRecord && !m_page.empty())
		m_history.push_back(m_page);
	m_page = rel;
	_loadPage(rel);
	_updateNavButtons();
}

void XAP_UnixHelpWindow::_loadPage(const std::string& rel)
{
	std::string path = _resolve(rel);
	gchar * contents = nullptr;
	if (!g_file_get_contents(path.c_str(), &contents, nullptr, nullptr))
	{
		/* fall back to the English copy, then to the index page —
		 * the manual is a single page now, so stale per-dialog
		 * help URLs just land on the index */
		std::string enPath = XAP_App::getApp()->getAbiSuiteLibDir();
		enPath += "/help/en-US/" + rel;
		if (!g_file_get_contents(enPath.c_str(), &contents, nullptr, nullptr)
			&& rel != "index.html")
		{
			path = _langDir() + "/index.html";
			g_file_get_contents(path.c_str(), &contents, nullptr, nullptr);
		}
		if (!contents)
		{
			GtkTextBuffer * buf =
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_wText));
			gtk_text_buffer_set_text(buf, "", -1);
			GtkTextIter it;
			gtk_text_buffer_get_start_iter(buf, &it);
			std::string msg = "The help page could not be found:\n" + rel;
			gtk_text_buffer_insert(buf, &it, msg.c_str(), -1);
			return;
		}
	}
	std::string html = contents ? contents : "";
	g_free(contents);
	/* replace any stray non-UTF-8 bytes rather than mangling the
	 * whole page with a wrong-charset conversion */
	if (!g_utf8_validate(html.c_str(), -1, nullptr))
	{
		gchar * conv = g_utf8_make_valid(html.c_str(), -1);
		html = conv;
		g_free(conv);
	}
	_renderHtml(html);
}

std::string XAP_UnixHelpWindow::_stripTags(const std::string& html) const
{
	std::string s = bodyOnly(html);
	std::string out;
	out.reserve(s.size());
	bool inTag = false;
	for (char c : s)
	{
		if (c == '<')
			inTag = true;
		else if (c == '>')
		{
			inTag = false;
			out += ' ';
		}
		else if (!inTag)
			out += c;
	}
	return decodeEntities(out);
}

std::string XAP_UnixHelpWindow::_pageTitle(const std::string& html,
										   const std::string& fallback) const
{
	size_t b = html.find("<title>");
	if (b != std::string::npos)
	{
		size_t e = html.find("</title>", b);
		if (e != std::string::npos)
		{
			std::string t = decodeEntities(
				html.substr(b + 7, e - b - 7));
			if (!t.empty())
				return t;
		}
	}
	return fallback;
}

/* drop the anonymous per-link tags from the previous page/search so
 * the tag table does not grow without bound */
void XAP_UnixHelpWindow::_clearLinkTags()
{
	GtkTextBuffer * buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_wText));
	GtkTextTagTable * tt = gtk_text_buffer_get_tag_table(buf);
	std::vector<GtkTextTag*> dead;
	gtk_text_tag_table_foreach(tt,
		+[](GtkTextTag * t, gpointer d)
		{
			if (g_object_get_data(G_OBJECT(t), "help-href"))
				static_cast<std::vector<GtkTextTag*>*>(d)->push_back(t);
		}, &dead);
	for (GtkTextTag * t : dead)
		gtk_text_tag_table_remove(tt, t);
}

void XAP_UnixHelpWindow::_renderHtml(const std::string& html)
{
	GtkTextBuffer * buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_wText));
	gtk_text_buffer_set_text(buf, "", -1);

	/* reuse the named style tags across page loads */
	GtkTextTagTable * tt = gtk_text_buffer_get_tag_table(buf);
	_clearLinkTags();
	auto getTag = [&](const char * name, auto... props) -> GtkTextTag *
	{
		GtkTextTag * t = gtk_text_tag_table_lookup(tt, name);
		return t ? t : gtk_text_buffer_create_tag(buf, name, props...);
	};
	GtkTextTag * tagH1 = getTag("h1",
		"scale", 1.5, "weight", PANGO_WEIGHT_BOLD,
		"pixels-above-lines", 8, "pixels-below-lines", 4, nullptr);
	GtkTextTag * tagH2 = getTag("h2",
		"scale", 1.25, "weight", PANGO_WEIGHT_BOLD,
		"pixels-above-lines", 6, "pixels-below-lines", 3, nullptr);
	GtkTextTag * tagB = getTag("b",
		"weight", PANGO_WEIGHT_BOLD, nullptr);
	GtkTextTag * tagI = getTag("i",
		"style", PANGO_STYLE_ITALIC, nullptr);
	GtkTextTag * tagMono = getTag("mono",
		"family", "monospace", nullptr);
	GtkTextTag * tagU = getTag("u",
		"underline", PANGO_UNDERLINE_SINGLE, nullptr);
	GtkTextTag * tagIndent = getTag("ind",
		"indent", 18, nullptr);

	std::string s = bodyOnly(html);
	/* (tag name, tag) pairs so </name> can pop its own entry */
	std::vector<std::pair<std::string, GtkTextTag*>> active;
	GtkTextIter iter;
	gtk_text_buffer_get_start_iter(buf, &iter);
	bool lastNl = true;

	auto emit = [&](const std::string& text)
	{
		if (text.empty())
			return;
		GtkTextMark * start =
			gtk_text_buffer_create_mark(buf, nullptr, &iter, TRUE);
		gtk_text_buffer_insert(buf, &iter, text.c_str(), -1);
		if (!active.empty())
		{
			GtkTextIter sIter;
			gtk_text_buffer_get_iter_at_mark(buf, &sIter, start);
			for (const auto& a : active)
				gtk_text_buffer_apply_tag(buf, a.second, &sIter, &iter);
		}
		gtk_text_buffer_delete_mark(buf, start);
	};
	auto newline = [&](int count)
	{
		if (lastNl)
		{
			while (count-- > 1)
			{
				gtk_text_buffer_insert(buf, &iter, "\n", 1);
			}
			return;
		}
		while (count-- > 0)
			gtk_text_buffer_insert(buf, &iter, "\n", 1);
		lastNl = true;
	};

	size_t i = 0, n = s.size();
	std::string text;
	while (i < n)
	{
		if (s[i] == '<')
		{
			size_t e = s.find('>', i);
			if (e == std::string::npos)
				break;
			std::string tag = s.substr(i + 1, e - i - 1);
			std::string tl;
			for (char c : tag)
				tl += g_ascii_tolower(c);
			bool closing = !tl.empty() && tl[0] == '/';
			std::string name = closing ? tl.substr(1) : tl;
			size_t sp = name.find_first_of(" \t/");
			if (sp != std::string::npos)
				name.resize(sp);
			while (!name.empty() && name.back() == '/')
				name.pop_back();

			if (!text.empty())
			{
				std::string dec = decodeEntities(text);
				emit(dec);
				lastNl = dec.size() && dec.back() == '\n';
				text.clear();
			}

			GtkTextTag * t = nullptr;
			if (name == "h1" || name == "h4")	t = tagH1;
			else if (name == "h2" || name == "h5" || name == "h6") t = tagH2;
			else if (name == "h3")				t = tagH2;
			else if (name == "b" || name == "strong") t = tagB;
			else if (name == "i" || name == "em")	t = tagI;
			else if (name == "tt" || name == "code" || name == "kbd" ||
					 name == "samp")			t = tagMono;
			else if (name == "u")				t = tagU;
			else if (name == "li")				t = tagIndent;

			if (closing)
			{
				/* pop the matching open tag */
				for (auto ait = active.rbegin(); ait != active.rend(); ++ait)
				{
					if (ait->first == name)
					{
						active.erase(std::next(ait).base());
						break;
					}
				}
				if (name == "p" || name == "div" || name == "li" ||
					name == "ul" || name == "ol" || name == "tr" ||
					name == "h1" || name == "h2" || name == "h3" ||
					name == "h4" || name == "h5" || name == "h6")
					newline(name == "p" ? 2 : 1);
			}
			else
			{
				if (name == "a")
				{
					std::string href = tagAttr(tag, "href");
					if (!href.empty())
					{
						t = gtk_text_buffer_create_tag(buf, nullptr,
							"underline", PANGO_UNDERLINE_SINGLE,
							"foreground", "#3465a4", nullptr);
						g_object_set_data_full(G_OBJECT(t), "help-href",
											   g_strdup(href.c_str()),
											   g_free);
					}
				}
				if (t)
					active.push_back({name, t});
				if (name == "p" || name == "div" || name == "tr" ||
					name == "h1" || name == "h2" || name == "h3" ||
					name == "h4" || name == "h5" || name == "h6")
					newline(name == "p" ? 2 : 1);
				else if (name == "br" || name == "ul" || name == "ol")
					newline(1);
				else if (name == "li")
				{
					newline(1);
					emit("  \xe2\x80\xa2 ");
					lastNl = false;
				}
				else if (name == "hr")
				{
					newline(1);
					emit("────────────────────────────────");
					newline(1);
				}
			}
			i = e + 1;
		}
		else
		{
			text += s[i++];
		}
	}
	if (!text.empty())
		emit(decodeEntities(text));

	GtkTextIter start;
	gtk_text_buffer_get_start_iter(buf, &start);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(m_wText), &start, 0, FALSE,
								 0, 0);
}

void XAP_UnixHelpWindow::_runSearch(const char * query)
{
	GtkTextBuffer * buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_wText));
	gtk_text_buffer_set_text(buf, "", -1);
	/* drop the previous result page's link tags */
	_clearLinkTags();
	GtkTextIter iter;
	gtk_text_buffer_get_start_iter(buf, &iter);

	GtkTextTagTable * tt = gtk_text_buffer_get_tag_table(buf);
	GtkTextTag * tagH1 = gtk_text_tag_table_lookup(tt, "search-h1");
	if (!tagH1)
		tagH1 = gtk_text_buffer_create_tag(buf, "search-h1",
			"scale", 1.4, "weight", PANGO_WEIGHT_BOLD, nullptr);
	gtk_text_buffer_insert_with_tags(buf, &iter, "Search results", -1,
									 tagH1, nullptr);
	gtk_text_buffer_insert(buf, &iter, "\n\n", -1);

	if (!query || !*query)
		return;

	gchar * needle = g_utf8_casefold(query, -1);
	if (!needle)
		return;

	/* scan every .html file of the current language */
	std::vector<std::string> files;
	std::vector<std::string> stack;
	stack.push_back("");
	while (!stack.empty())
	{
		std::string sub = stack.back();
		stack.pop_back();
		std::string dir = _langDir() + (sub.empty() ? "" : "/" + sub);
		GDir * d = g_dir_open(dir.c_str(), 0, nullptr);
		if (!d)
			continue;
		const gchar * name;
		while ((name = g_dir_read_name(d)))
		{
			std::string rel = sub.empty() ? name : sub + "/" + name;
			std::string full = dir + "/" + name;
			if (g_file_test(full.c_str(), G_FILE_TEST_IS_DIR))
				stack.push_back(rel);
			else if (g_str_has_suffix(name, ".html"))
				files.push_back(rel);
		}
		g_dir_close(d);
	}

	int nHits = 0;
	for (const std::string& rel : files)
	{
		std::string full = _langDir() + "/" + rel;
		gchar * contents = nullptr;
		if (!g_file_get_contents(full.c_str(), &contents, nullptr, nullptr))
			continue;
		std::string html = contents;
		g_free(contents);
		if (!g_utf8_validate(html.c_str(), -1, nullptr))
		{
			gchar * conv = g_utf8_make_valid(html.c_str(), -1);
			html = conv;
			g_free(conv);
		}
		std::string text = _stripTags(html);
		gchar * folded = g_utf8_casefold(text.c_str(), -1);
		const char * hit = folded ? strstr(folded, needle) : nullptr;
		if (!hit)
		{
			g_free(folded);
			continue;
		}
		++nHits;

		/* result link = page title */
		GtkTextTag * link = gtk_text_buffer_create_tag(buf, nullptr,
			"underline", PANGO_UNDERLINE_SINGLE,
			"foreground", "#3465a4",
			"weight", PANGO_WEIGHT_BOLD, nullptr);
		std::string title = _pageTitle(html, rel);
		g_object_set_data_full(G_OBJECT(link), "help-href",
							   g_strdup(rel.c_str()), g_free);
		GtkTextMark * start =
			gtk_text_buffer_create_mark(buf, nullptr, &iter, TRUE);
		gtk_text_buffer_insert(buf, &iter, title.c_str(), -1);
		GtkTextIter sIter;
		gtk_text_buffer_get_iter_at_mark(buf, &sIter, start);
		gtk_text_buffer_apply_tag(buf, link, &sIter, &iter);
		gtk_text_buffer_delete_mark(buf, start);
		gtk_text_buffer_insert(buf, &iter, "\n", -1);

		/* ~70 chars of context around the first match */
		size_t pos = hit - folded;
		size_t b = pos > 40 ? pos - 40 : 0;
		size_t e = std::min(pos + strlen(needle) + 70, strlen(folded));
		std::string snip = std::string(folded).substr(b, e - b);
		gtk_text_buffer_insert(buf, &iter, "    ", -1);
		gtk_text_buffer_insert(buf, &iter, snip.c_str(), -1);
		gtk_text_buffer_insert(buf, &iter, "\n\n", -1);
		g_free(folded);
	}
	g_free(needle);

	if (!nHits)
		gtk_text_buffer_insert(buf, &iter, "No matching help pages.", -1);
}

void XAP_UnixHelpWindow::_updateNavButtons()
{
	gtk_widget_set_sensitive(m_wBack, !m_history.empty());
}

void XAP_UnixHelpWindow::_s_back(GtkButton * /*btn*/, gpointer data)
{
	XAP_UnixHelpWindow * self = static_cast<XAP_UnixHelpWindow *>(data);
	if (self->m_history.empty())
		return;
	std::string prev = self->m_history.back();
	self->m_history.pop_back();
	self->m_page = prev;
	self->_loadPage(prev);
	self->_updateNavButtons();
}

void XAP_UnixHelpWindow::_s_home(GtkButton * /*btn*/, gpointer data)
{
	XAP_UnixHelpWindow * self = static_cast<XAP_UnixHelpWindow *>(data);
	self->_navigate("index.html", true);
}

void XAP_UnixHelpWindow::_s_search(GtkSearchEntry * e, gpointer data)
{
	XAP_UnixHelpWindow * self = static_cast<XAP_UnixHelpWindow *>(data);
	const char * q = gtk_editable_get_text(GTK_EDITABLE(e));
	if (q && *q)
	{
		/* search-changed fires per keystroke and a full scan takes
		 * noticeable time - debounce to ~200 ms of idle */
		if (self->m_iSearchTimer)
			g_source_remove(self->m_iSearchTimer);
		self->m_iSearchTimer = g_timeout_add(200,
			+[](gpointer d) -> gboolean
			{
				XAP_UnixHelpWindow * s =
					static_cast<XAP_UnixHelpWindow *>(d);
				s->m_iSearchTimer = 0;
				const char * t = gtk_editable_get_text(
					GTK_EDITABLE(s->m_wSearch));
				s->_runSearch(t ? t : "");
				return G_SOURCE_REMOVE;
			}, self);
	}
	else
		self->_loadPage(self->m_page);
}

static const char * linkAt(GtkTextView * tv, double x, double y)
{
	int bx, by;
	gtk_text_view_window_to_buffer_coords(tv, GTK_TEXT_WINDOW_TEXT,
										  (int)x, (int)y, &bx, &by);
	GtkTextIter iter;
	gtk_text_view_get_iter_at_location(tv, &iter, bx, by);
	GSList * tags = gtk_text_iter_get_tags(&iter);
	for (GSList * l = tags; l; l = l->next)
	{
		const char * href = static_cast<const char *>(
			g_object_get_data(G_OBJECT(l->data), "help-href"));
		if (href)
		{
			g_slist_free(tags);
			return href;
		}
	}
	if (tags)
		g_slist_free(tags);
	return nullptr;
}

void XAP_UnixHelpWindow::_s_click(GtkGestureClick * /*g*/, int /*n*/,
								  double x, double y, gpointer data)
{
	XAP_UnixHelpWindow * self = static_cast<XAP_UnixHelpWindow *>(data);
	const char * href =
		linkAt(GTK_TEXT_VIEW(self->m_wText), x, y);
	if (!href)
		return;
	/* external links still go to the browser; in-page anchors and
	 * fragment suffixes are stripped for local navigation */
	if (strstr(href, "://") || g_str_has_prefix(href, "mailto:"))
	{
		XAP_App::getApp()->openURL(href);
		return;
	}
	std::string rel = href;
	size_t hash = rel.find('#');
	if (hash == 0)
		return;		/* same-page anchor - nothing to load */
	if (hash != std::string::npos)
		rel.resize(hash);
	self->_navigate(rel, true);
}

void XAP_UnixHelpWindow::_s_motion(GtkEventControllerMotion * /*m*/,
								   double x, double y, gpointer data)
{
	XAP_UnixHelpWindow * self = static_cast<XAP_UnixHelpWindow *>(data);
	const char * href =
		linkAt(GTK_TEXT_VIEW(self->m_wText), x, y);
	gtk_widget_set_cursor_from_name(self->m_wText,
									href ? "pointer" : "text");
}

gboolean XAP_UnixHelpWindow::_s_destroy(GtkWidget * /*w*/, gpointer data)
{
	XAP_UnixHelpWindow * self = static_cast<XAP_UnixHelpWindow *>(data);
	self->m_wWindow = nullptr;
	return FALSE;
}


void XAP_UnixHelpWindow::show(const char * page, bool bFocusSearch)
{
	if (!m_wWindow)
	{
		GtkWindow * parent = nullptr;
		if (m_pFrame)
		{
			XAP_UnixFrameImpl * pImpl =
				static_cast<XAP_UnixFrameImpl *>(m_pFrame->getFrameImpl());
			if (pImpl && pImpl->getTopLevelWindow())
				parent = GTK_WINDOW(pImpl->getTopLevelWindow());
		}

		m_wWindow = gtk_window_new();
		gtk_window_set_title(GTK_WINDOW(m_wWindow), "Abinova Help");
		gtk_window_set_default_size(GTK_WINDOW(m_wWindow), 720, 560);
		if (parent)
			gtk_window_set_transient_for(GTK_WINDOW(m_wWindow), parent);
		g_signal_connect(m_wWindow, "destroy",
						 G_CALLBACK(_s_destroy), this);
		/* object lifetime = window lifetime */
		g_object_set_data_full(G_OBJECT(m_wWindow), "help-win-obj", this,
							   +[](gpointer p)
							   {
								   delete static_cast<XAP_UnixHelpWindow*>(p);
							   });

		GtkWidget * outer =
			gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

		/* toolbar row: back, home, language, search */
		GtkWidget * bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_margin_start(bar, 8);
		gtk_widget_set_margin_end(bar, 8);
		gtk_widget_set_margin_top(bar, 8);
		gtk_widget_set_margin_bottom(bar, 8);

		m_wBack = gtk_button_new_from_icon_name("go-previous");
		gtk_widget_set_tooltip_text(m_wBack, "Back");
		g_signal_connect(m_wBack, "clicked", G_CALLBACK(_s_back), this);
		gtk_box_append(GTK_BOX(bar), m_wBack);

		GtkWidget * home = gtk_button_new_from_icon_name("go-home");
		gtk_widget_set_tooltip_text(home, "Help contents");
		g_signal_connect(home, "clicked", G_CALLBACK(_s_home), this);
		gtk_box_append(GTK_BOX(bar), home);

		m_wSearch = gtk_search_entry_new();
		gtk_widget_set_hexpand(m_wSearch, TRUE);
		g_object_set(m_wSearch, "placeholder-text", "Search help…",
					 nullptr);
		g_signal_connect(m_wSearch, "activate",
						 G_CALLBACK(_s_search), this);
		g_signal_connect(m_wSearch, "search-changed",
						 G_CALLBACK(_s_search), this);
		gtk_box_append(GTK_BOX(bar), m_wSearch);

		gtk_box_append(GTK_BOX(outer), bar);
		gtk_box_append(GTK_BOX(outer),
					   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

		m_wText = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(m_wText), FALSE);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(m_wText),
								  GTK_WRAP_WORD);
		gtk_text_view_set_left_margin(GTK_TEXT_VIEW(m_wText), 12);
		gtk_text_view_set_right_margin(GTK_TEXT_VIEW(m_wText), 12);
		gtk_text_view_set_top_margin(GTK_TEXT_VIEW(m_wText), 8);
		gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(m_wText), 8);

		GtkGesture * click = gtk_gesture_click_new();
		g_signal_connect(click, "released", G_CALLBACK(_s_click), this);
		gtk_widget_add_controller(m_wText, GTK_EVENT_CONTROLLER(click));

		GtkEventController * motion = gtk_event_controller_motion_new();
		g_signal_connect(motion, "motion", G_CALLBACK(_s_motion), this);
		gtk_widget_add_controller(m_wText, motion);

		GtkWidget * scroll = gtk_scrolled_window_new();
		gtk_widget_set_vexpand(scroll, TRUE);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
									  m_wText);
		gtk_box_append(GTK_BOX(outer), scroll);

		gtk_window_set_child(GTK_WINDOW(m_wWindow), outer);
	}

	gtk_window_present(GTK_WINDOW(m_wWindow));

	if (page && *page)
		_navigate(page, false);
	else if (m_page.empty())
		_navigate("index.html", false);
	else
		_loadPage(m_page);

	if (bFocusSearch)
		gtk_widget_grab_focus(m_wSearch);
}
