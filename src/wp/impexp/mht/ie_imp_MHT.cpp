/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova: ie_imp_MHT - plugin for Multipart [X]HTML
 * 
 * Copyright (C) 2002 Francis James Franklin <fjf@alinameridon.com>
 * Copyright (C) 2025-2026 Abinova contributors
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


#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <string>
#include <utility>
#include <vector>

// Abinova includes

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_base64.h"
#include "ut_bytebuf.h"
#include "ut_hash.h"
#include "ut_vector.h"

#include "pd_Document.h"

#include "ie_impGraphic.h"
#include "ie_imp_MHT.h"
#include "ie_impexp_HTML.h"

#ifdef XHTML_HTML_TIDY_SUPPORTED
#include "tidyReader.h"
#endif
#ifdef XHTML_HTML_XML2_SUPPORTED
#include "ut_html.h"
#endif

/*****************************************************************/
/*****************************************************************/

IE_Imp_MHT_Sniffer::IE_Imp_MHT_Sniffer () :
	IE_ImpSniffer("AbiMHT::MHTML")
{
	// 
}

// supported suffixes
static IE_SuffixConfidence IE_Imp_MHT_Sniffer__SuffixConfidence[] = {
	{ "mht", 	UT_CONFIDENCE_GOOD 		},
	{ "mhtm", 	UT_CONFIDENCE_GOOD 		},
	{ "mhtml", 	UT_CONFIDENCE_GOOD 		},
	{ "", 	UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_Imp_MHT_Sniffer::getSuffixConfidence ()
{
	return IE_Imp_MHT_Sniffer__SuffixConfidence;
}

// supported mimetypes
static IE_MimeConfidence IE_Imp_MHT_Sniffer__MimeConfidence[] = {
	{ IE_MIME_MATCH_FULL, 	IE_MIMETYPE_RELATED, 	UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_FULL, 	"application/x-mimearchive", 	UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_FULL, 	"message/rfc822", 	UT_CONFIDENCE_SOSO 	},
	{ IE_MIME_MATCH_BOGUS, 	"", 					UT_CONFIDENCE_ZILCH }
};

const IE_MimeConfidence * IE_Imp_MHT_Sniffer::getMimeConfidence ()
{
	return IE_Imp_MHT_Sniffer__MimeConfidence;
}

static const char * s_strnstr (const char * haystack, UT_uint32 iNumbytes, const char * needle)
{
	UT_uint32 needle_length = static_cast<UT_uint32>(strlen (needle));
	UT_uint32 i = 0;

	if (needle_length > iNumbytes) return nullptr;

	const char * ptr = haystack;
	const char * match = nullptr;

	while (i <= (iNumbytes - needle_length))
		{
			if (*ptr == *needle)
				if (strncmp (ptr, needle, needle_length) == 0)
					{
						match = ptr;
						break;
					}
			ptr++;
			i++;
		}
	return match;
}

static int s_hexval (char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static std::string s_urlDecode (const char * s)
{
	std::string out;
	while (*s)
		{
			if (s[0] == '%' && s_hexval (s[1]) >= 0 && s_hexval (s[2]) >= 0)
				{
					out += static_cast<char>((s_hexval (s[1]) << 4) | s_hexval (s[2]));
					s += 3;
				}
			else out += *s++;
		}
	return out;
}

/* Normalize a Content-ID header value or a cid: reference for comparison:
 * trim surrounding whitespace and strip the RFC 2392 "<...>" msg-id
 * brackets that Content-ID values carry (and cid: URLs omit).
 */
static std::string s_normalizeCID (const char * s)
{
	while (*s == ' ' || *s == '\t') s++;
	std::string out (s);
	while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
	if (out.size() >= 2 && out.front() == '<' && out.back() == '>')
		{
			out = out.substr (1, out.size() - 2);
			while (!out.empty() && (out.front() == ' ' || out.front() == '\t')) out.erase (0, 1);
			while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
		}
	return out;
}

/* Scan the RFC 822 header block at the top of the buffer: a run of
 * "Field-Name: value" lines (folded continuations start with WSP) ended by
 * a blank line. Returns true and sets blockLen when every line before the
 * first blank line has a valid header shape — i.e. the buffer really is a
 * MIME message and not text that merely mentions MIME strings.
 */
static bool s_mimeHeaderBlock (const char * szBuf, UT_uint32 iNumbytes,
							   UT_uint32 & blockLen)
{
	blockLen = 0;
	UT_uint32 pos = 0;
	bool bSawHeader = false;

	while (pos < iNumbytes)
		{
			UT_uint32 linelen = 0;
			while (pos + linelen < iNumbytes &&
				   szBuf[pos + linelen] != '\r' && szBuf[pos + linelen] != '\n')
				linelen++;
			const char * line = szBuf + pos;

			if (linelen == 0)
				{
					blockLen = pos;
					return bSawHeader;
				}
			if (line[0] == ' ' || line[0] == '\t')
				{
					// folded continuation of the previous header
					if (!bSawHeader) return false;
				}
			else
				{
					const char * colon = static_cast<const char *>
						(memchr (line, ':', linelen));
					if (!colon || colon == line) return false;

					// header name: alnum + '-', tolerating stray WSP
					// before the colon (non-strict writers produce it)
					for (const char * p = line; p < colon; p++)
						if (!isalnum (static_cast<unsigned char>(*p)) &&
							*p != '-' && *p != ' ' && *p != '\t')
							return false;
					bSawHeader = true;
				}

			pos += linelen;
			if (pos < iNumbytes && szBuf[pos] == '\r') pos++;
			if (pos < iNumbytes && szBuf[pos] == '\n') pos++;
		}
	return false;
}

/* UT_MHTStream - self-contained MIME multipart parser for MHTML files
 * (RFC 2045/2046 multipart/related), replacing the obsolete libeps dependency.
 */

class UT_MHTStream
{
public:
	UT_MHTStream () :
		m_pos(0),
		m_partHeaderIdx(0),
		m_multipart(false),
		m_pendingBoundary(false),
		m_pendingClosing(false)
	{
		//
	}

	bool open (GsfInput * input);
	void close ();

	const std::vector<std::pair<std::string,std::string> > & headers () const { return m_headers; }
	bool isMultipart () const { return m_multipart; }

	bool nextPart ();
	bool nextHeader (std::string & name, std::string & value);
	bool nextBody (std::string & body);

private:
	bool readLine (std::string & out);
	bool isBoundaryLine (const std::string & line, bool & closing) const;
	void parseHeaders (std::vector<std::pair<std::string,std::string> > & out);
	static std::string getMIMEParam (const std::string & header, const char * param);

	std::string m_data;
	size_t m_pos;

	std::vector<std::pair<std::string,std::string> > m_headers;
	std::vector<std::pair<std::string,std::string> > m_partHeaders;
	size_t m_partHeaderIdx;

	std::string m_boundary;
	bool m_multipart;

	bool m_pendingBoundary;
	bool m_pendingClosing;
};

bool UT_MHTStream::open (GsfInput * input)
{
	gsf_input_seek (input, 0, G_SEEK_SET);

	gsf_off_t size = gsf_input_remaining (input);
	if (size <= 0) return false;

	m_data.resize (static_cast<size_t>(size));

	gsf_off_t done = 0;
	while (done < size &&
		   gsf_input_read (input, static_cast<size_t>(size - done),
						   reinterpret_cast<guint8 *>(&m_data[done])))
		done = size - gsf_input_remaining (input);
	m_data.resize (static_cast<size_t>(done));
	if (done == 0)
		{
			m_data.clear ();
			return false;
		}

	m_pos = 0;
	parseHeaders (m_headers);

	for (auto & h : m_headers)
		{
			if (g_ascii_strcasecmp (h.first.c_str(), "content-type") != 0) continue;

			const std::string & ct = h.second;
			if (s_strnstr (ct.c_str(), static_cast<UT_uint32>(ct.size()), "multipart/"))
				{
					std::string b = getMIMEParam (ct, "boundary");
					if (!b.empty())
						{
							m_boundary = "--" + b;
							m_multipart = true;
						}
				}
			break;
		}
	return true;
}

void UT_MHTStream::close ()
{
	m_data.clear ();
	m_headers.clear ();
	m_partHeaders.clear ();
	m_boundary.clear ();
	m_pos = 0;
	m_partHeaderIdx = 0;
	m_multipart = false;
	m_pendingBoundary = false;
	m_pendingClosing = false;
}

bool UT_MHTStream::readLine (std::string & out)
{
	out.clear ();
	if (m_pos >= m_data.size()) return false;

	size_t start = m_pos;
	size_t nl = m_data.find ('\n', m_pos);
	if (nl == std::string::npos)
		{
			m_pos = m_data.size();
			out.assign (m_data, start, m_data.size() - start);
		}
	else
		{
			out.assign (m_data, start, nl - start);
			m_pos = nl + 1;
		}
	if (!out.empty() && out.back() == '\r') out.pop_back();
	return true;
}

bool UT_MHTStream::isBoundaryLine (const std::string & line, bool & closing) const
{
	closing = false;
	if (m_boundary.empty()) return false;
	if (line.size() < m_boundary.size()) return false;
	if (line.compare (0, m_boundary.size(), m_boundary) != 0) return false;

	const char * rest = line.c_str() + m_boundary.size();
	if (rest[0] == '-' && rest[1] == '-')
		{
			closing = true;
			rest += 2;
		}
	while (*rest == ' ' || *rest == '\t') rest++;
	return *rest == '\0';
}

void UT_MHTStream::parseHeaders (std::vector<std::pair<std::string,std::string> > & out)
{
	out.clear ();
	std::string line;

	while (true)
		{
			size_t linestart = m_pos;
			if (!readLine (line)) break;
			if (line.empty()) break;

			bool closing = false;
			if (isBoundaryLine (line, closing))
				{
					// a delimiter directly after headers (no blank line):
					// rewind so nextBody/nextPart can see it
					m_pos = linestart;
					break;
				}

			if ((line[0] == ' ' || line[0] == '\t') && !out.empty())
				{
					// folded continuation line
					size_t i = 0;
					while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
					out.back().second += " ";
					out.back().second += line.substr (i);
					continue;
				}

			size_t colon = line.find (':');
			if (colon == std::string::npos) break;

			std::string name = line.substr (0, colon);
			std::string value = line.substr (colon + 1);

			while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
			size_t v = 0;
			while (v < value.size() && (value[v] == ' ' || value[v] == '\t')) v++;
			value.erase (0, v);

			out.emplace_back (name, value);
		}
}

std::string UT_MHTStream::getMIMEParam (const std::string & header, const char * param)
{
	size_t plen = strlen (param);
	size_t i = 0;

	while (i + plen < header.size())
		{
			i = header.find (';', i);
			if (i == std::string::npos) break;
			i++;

			while (i < header.size() && (header[i] == ' ' || header[i] == '\t')) i++;
			if (i + plen >= header.size()) break;
			if (g_ascii_strncasecmp (header.c_str() + i, param, plen) != 0) continue;
			if (header[i + plen] != '=') continue;

			i += plen + 1;
			while (i < header.size() && (header[i] == ' ' || header[i] == '\t')) i++;

			if (i < header.size() && header[i] == '"')
				{
					size_t end = header.find ('"', i + 1);
					if (end == std::string::npos) end = header.size();
					return header.substr (i + 1, end - i - 1);
				}
			size_t end = i;
			while (end < header.size() && header[end] != ';' && header[end] != ' ' && header[end] != '\t') end++;
			return header.substr (i, end - i);
		}
	return std::string ();
}

bool UT_MHTStream::nextPart ()
{
	if (!m_multipart) return false;

	if (m_pendingBoundary)
		{
			m_pendingBoundary = false;
			if (m_pendingClosing) return false;
		}
	else
		{
			// scan for next boundary line (skips preamble / skipped part bodies)
			std::string line;
			bool closing;
			bool found = false;

			while (readLine (line))
				{
					if (isBoundaryLine (line, closing))
						{
							if (closing) return false;
							found = true;
							break;
						}
				}
			if (!found) return false;
		}
	m_partHeaders.clear ();
	m_partHeaderIdx = 0;
	parseHeaders (m_partHeaders);
	return true;
}

bool UT_MHTStream::nextHeader (std::string & name, std::string & value)
{
	if (m_partHeaderIdx >= m_partHeaders.size()) return false;
	const auto & h = m_partHeaders[m_partHeaderIdx++];
	name = h.first;
	value = h.second;
	return true;
}

bool UT_MHTStream::nextBody (std::string & out)
{
	out.clear ();
	if (!m_multipart || m_boundary.empty() || m_pendingBoundary) return false;

	size_t start = m_pos;
	size_t cursor = m_pos;

	while (cursor < m_data.size())
		{
			// a delimiter is "--boundary" at a line start, followed by an
			// optional "--" close marker, optional WSP, then EOL/EOF
			size_t pos = m_data.find (m_boundary, cursor);
			if (pos == std::string::npos) break;
			if (pos != 0 && m_data[pos - 1] != '\n')
				{
					cursor = pos + 1;
					continue;
				}

			size_t rest = pos + m_boundary.size();
			bool closing = false;
			if (rest + 1 < m_data.size() && m_data[rest] == '-' && m_data[rest + 1] == '-')
				{
					closing = true;
					rest += 2;
				}
			while (rest < m_data.size() && (m_data[rest] == ' ' || m_data[rest] == '\t')) rest++;
			if (rest < m_data.size() && m_data[rest] != '\r' && m_data[rest] != '\n')
				{
					cursor = pos + 1;
					continue;
				}
			size_t lend = rest;
			if (lend < m_data.size() && m_data[lend] == '\r') lend++;
			if (lend < m_data.size() && m_data[lend] == '\n') lend++;

			// the CRLF preceding the delimiter belongs to it (RFC 2046 5.1.1)
			size_t body_end = pos;
			if (body_end > start && m_data[body_end - 1] == '\n')
				{
					body_end--;
					if (body_end > start && m_data[body_end - 1] == '\r') body_end--;
				}
			out.assign (m_data, start, body_end - start);
			m_pos = lend;
			m_pendingBoundary = true;
			m_pendingClosing = closing;
			return true;
		}

	// no further boundary — the rest of the archive is the body (malformed input)
	out.assign (m_data, start, std::string::npos);
	m_pos = m_data.size();
	return true;
}

UT_Confidence_t IE_Imp_MHT_Sniffer::recognizeContents (const char * szBuf, UT_uint32 iNumbytes)
{
	// A well-formed RFC 822 header block declaring multipart/related with an
	// (x)html document type is definitive MHTML — plain-text matchers (text,
	// markdown) must not win over it just because a boundary string or
	// escaped markup happens to look like their syntax.
	UT_uint32 hdrLen = 0;
	if (s_mimeHeaderBlock (szBuf, iNumbytes, hdrLen) &&
		s_strnstr (szBuf, hdrLen, IE_MIMETYPE_RELATED) &&
		(s_strnstr (szBuf, hdrLen, IE_MIMETYPE_HTML) ||
		 s_strnstr (szBuf, hdrLen, IE_MIMETYPE_XHTML)))
		{
			return UT_CONFIDENCE_PERFECT;
		}

	if (s_strnstr (szBuf, iNumbytes, IE_MIMETYPE_RELATED))
		if (s_strnstr (szBuf, iNumbytes, IE_MIMETYPE_HTML) ||
			s_strnstr (szBuf, iNumbytes, IE_MIMETYPE_XHTML))
			{
				return UT_CONFIDENCE_GOOD;
			}
	return UT_CONFIDENCE_ZILCH;
}

UT_Error IE_Imp_MHT_Sniffer::constructImporter (PD_Document * pDocument, IE_Imp ** ppie)
{
	IE_Imp_MHT * p = new IE_Imp_MHT (pDocument);
	*ppie = p;
	return UT_OK;
}

bool IE_Imp_MHT_Sniffer::getDlgLabels (const char ** pszDesc, const char ** pszSuffixList,
										IEFileType * ft)
{
	*pszDesc = "MHTML (.mht, .mhtm, .mhtml)";
	*pszSuffixList = "*.mht;*.mhtm;*.mhtml";
	*ft = getFileType ();
	return true;
}

/*****************************************************************/
/*****************************************************************/

IE_Imp_MHT::IE_Imp_MHT (PD_Document * pDocument) :
	IE_Imp_XHTML(pDocument),
	m_document(0),
	m_parts(new UT_Vector)
{
	// 
}

IE_Imp_MHT::~IE_Imp_MHT ()
{
	UT_VECTOR_PURGEALL(UT_Multipart *,(*m_parts));
	DELETEP(m_parts);
}

UT_Error IE_Imp_MHT::_loadFile (GsfInput * input)
{
	UT_MHTStream stream;

	if (!stream.open (input))
		{
			UT_DEBUGMSG(("Unable to read MHTML stream!\n"));
			return UT_IE_BOGUSDOCUMENT;
		}

	bool bValid = false;

	for (const auto & h : stream.headers ())
		{
			const char * name = h.first.c_str();
			const char * data = h.second.c_str();

			if (g_ascii_strcasecmp (name, "content-type") == 0)
				{
					UT_uint32 length = static_cast<UT_uint32>(h.second.size());
					if (s_strnstr (data, length, IE_MIMETYPE_RELATED))
						if (s_strnstr (data, length, IE_MIMETYPE_HTML) ||
							s_strnstr (data, length, IE_MIMETYPE_XHTML))
							{
								bValid = true;
							}
				}
		}
	if (!stream.isMultipart ()) bValid = false;

	UT_Error import_status = UT_OK;

	if (bValid)
		{
			while (stream.nextPart ())
				{
					UT_Multipart * part = importMultipart (stream);
					if (part == 0) break;

					if (part->isXHTML () || part->isHTML4 ())
						{
							if (m_document)
								{
									UT_DEBUGMSG(("Multipart HTML document has multiple HTML regions!\n"));
									DELETEP(part);
									import_status = UT_IE_BOGUSDOCUMENT;
									break;
								}
							m_document = part;
						}
					if (m_parts->addItem (part) < 0)
						{
							UT_DEBUGMSG(("Multipart HTML: error appending part!\n"));
							DELETEP(part);
							import_status = UT_OUTOFMEM;
							break;
						}
				}
		}
	stream.close ();

	if (m_document == 0)
		{
			UT_DEBUGMSG(("Multipart HTML document has no HTML regions!\n"));
			import_status = UT_IE_BOGUSDOCUMENT;
		}
	if (import_status == UT_OK)
		{
			if (m_document->isXHTML ())
				{
					import_status = importXHTML ();
				}
			else if (m_document->isHTML4 ())
				{
					import_status = importHTML4 ();
				}
			else import_status = UT_ERROR;
		}
	return import_status;
}

FG_ConstGraphicPtr IE_Imp_MHT::importImage(const gchar * szSrc)
{
	bool bContentID = (g_ascii_strncasecmp (szSrc, "cid:", 4) == 0);

	// decode %-escapes in the reference — cid: and location URLs arrive
	// URL-encoded per RFC 2392 / RFC 2557
	const std::string wanted = s_urlDecode (szSrc + (bContentID ? 4 : 0));

	const UT_Multipart * part = 0;

	UT_uint32 count = m_parts->getItemCount ();
	for (UT_uint32 i = 0; i < count; i++)
		{
			const UT_Multipart * ptr = reinterpret_cast<const UT_Multipart *>((*m_parts)[i]);
			if (!ptr->isImage ()) continue;

			if (bContentID && ptr->contentID ())
				{
					// Content-ID is a msg-id ("<id>"), the cid: reference
					// drops the brackets — compare full normalized strings
					if (wanted == s_normalizeCID (ptr->contentID ()))
						{
							part = ptr;
							break;
						}
				}
			if (ptr->contentLocation ())
				{
					const std::string loc = s_urlDecode (ptr->contentLocation ());
					// exact match, or the part's absolute location ending in
					// the document's relative reference (Word writes
					// file:///... locations against bare filenames)
					if (loc == wanted ||
						(loc.size() > wanted.size() && !wanted.empty() &&
						 loc.compare (loc.size() - wanted.size(), std::string::npos,
									  wanted) == 0 &&
						 loc[loc.size() - wanted.size() - 1] == '/'))
						{
							part = ptr;
							break;
						}
				}
		}
	if (part == 0)
		{
			UT_DEBUGMSG(("Multipart HTML: importImage: `%s' not an image, or not in archive\n",szSrc));
			return 0;
		}

	const UT_ConstByteBufPtr & pBB = part->getBuffer();
	if (!pBB)
		{
			UT_DEBUGMSG(("Multipart HTML: importImage: `%s' - image in archive but not (or no longer?) loaded!\n",szSrc));
			return 0;
		}
	if (pBB->getLength () == 0)
		{
			UT_DEBUGMSG(("Multipart HTML: importImage: `%s' - image in archive but has no size!\n",szSrc));
			return 0;
		}

	IE_ImpGraphic * pieg = 0;
	if (IE_ImpGraphic::constructImporter (pBB, IEGFT_Unknown, &pieg) != UT_OK)
		{
			UT_DEBUGMSG(("unable to construct image importer!\n"));
			return 0;
		}
	if (pieg == 0) return 0;

	UT_Multipart * vol_part = const_cast<UT_Multipart *>(part);

	FG_ConstGraphicPtr pfg;
	UT_Error import_status = pieg->importGraphic (vol_part->detachBuffer (), pfg);
	delete pieg;
	if (import_status != UT_OK)
		{
			UT_DEBUGMSG(("unable to import image!\n"));
			return 0;
		}
	UT_DEBUGMSG(("image loaded successfully\n"));

	return pfg;
}

UT_Error IE_Imp_MHT::importXHTML ()
{
	// the document part is already decoded in memory; IE_Imp_XML parses
	// buffers directly, so no UT_XML::Reader is needed here
	const UT_Byte * buffer = m_document->getBuffer()->getPointer (0);
	UT_uint32 length = m_document->getBuffer()->getLength ();

	return IE_Imp_XHTML::importFile (reinterpret_cast<const char *>(buffer), length);
}

UT_Error IE_Imp_MHT::importHTML4 ()
{
	UT_Error e = UT_ERROR;

	const UT_Byte * buffer = m_document->getBuffer()->getPointer (0);
	UT_uint32 length = m_document->getBuffer()->getLength ();

#ifdef XHTML_HTML_TIDY_SUPPORTED
	// run libtidy over the buffer, then import the resulting XHTML
	TidyReader reader(buffer,length);
	if (reader.openFile (""))
		{
			std::string tidied;
			char chunk[4096];
			UT_uint32 n;
			while ((n = reader.readBytes (chunk, sizeof (chunk))) > 0)
				tidied.append (chunk, n);
			reader.closeFile ();

			if (!tidied.empty())
				e = IE_Imp_XHTML::importFile (tidied.c_str(), static_cast<UT_uint32>(tidied.size()));
		}
#endif
#ifdef XHTML_HTML_XML2_SUPPORTED
	UT_HTML parser;
	setParser (&parser);

	e = IE_Imp_XHTML::importFile (reinterpret_cast<const char *>(buffer), length);

	setParser (0);
#endif
	return e;
}

UT_Multipart * IE_Imp_MHT::importMultipart (UT_MHTStream & stream)
{
	UT_Multipart * part = new UT_Multipart;
	if (part == 0) return 0;

	std::string name, value;

	while (stream.nextHeader (name, value))
		part->insert (name.c_str(), value.c_str());

	bool bLoad = (part->isImage () || part->isXHTML () || part->isHTML4 ());

	if (bLoad)
		{
			std::string body;
			if (stream.nextBody (body) && !body.empty())
				if (!part->append (body.data(), static_cast<UT_uint32>(body.size())))
					UT_DEBUGMSG(("Multipart HTML: importMultipart: failed to decode part body!\n"));
		}
	return part;
}

UT_Multipart::UT_Multipart () :
	m_map(new UT_StringPtrMap),
	m_buf(new UT_ByteBuf),
	m_location(0),
	m_id(0),
	m_type(0),
	m_encoding(0),
	m_cte(cte_other),
	m_ct(ct_other),
	m_b64length(0)
{
	// 
}

UT_Multipart::~UT_Multipart ()
{
	clear ();

	DELETEP(m_map);
}

bool UT_Multipart::insert (const char * name, const char * value)
{
	if (( name == 0) || ( value == 0)) return false;
	if ((*name == 0) || (*value == 0)) return false;

	char * new_value = g_strdup (value);
	if (new_value == 0) return false;

	if (!m_map->insert (name, new_value))
		{
			FREEP(new_value);
			return false;
		}

	if (g_ascii_strcasecmp (name, "content-transfer-encoding") == 0)
		{
			m_encoding = new_value;

			if (g_ascii_strcasecmp (new_value, "base64") == 0)
				{
					m_cte = cte_base64;
				}
			else if (g_ascii_strcasecmp (new_value, "quoted-printable") == 0)
				{
					m_cte = cte_quoted;
				}
			else m_cte = cte_other;
		}
	else if (g_ascii_strcasecmp (name, "content-location") == 0)
		{
			m_location = new_value;
		}
	else if (g_ascii_strcasecmp (name, "content-id") == 0)
		{
			m_id = new_value;
		}
	else if (g_ascii_strcasecmp (name, "content-type") == 0)
		{
			m_type = new_value;

			if (strncmp (new_value, IE_MIMETYPE_HTML, strlen (IE_MIMETYPE_HTML)) == 0)
				{
					m_ct = ct_html4;
				}
			else if (strncmp (new_value, IE_MIMETYPE_XHTML, strlen (IE_MIMETYPE_XHTML)) == 0)
				{
					m_ct = ct_xhtml;
				}
			else if (strncmp (new_value, "image/", 6) == 0)
				{
					m_ct = ct_image;
				}
			else m_ct = ct_other;
		}
	return true;
}

const char * UT_Multipart::lookup (const char * name)
{
	if ( name == 0) return 0;
	if (*name == 0) return 0;

	const void * vptr = m_map->pick (name);
	return reinterpret_cast<const char *>(vptr);
}

/* Append the raw (undecoded) body of a part; bytes are decoded per the
 * part's Content-Transfer-Encoding. Raw/7bit/8bit/binary bodies are stored
 * verbatim — no line re-wrapping.
 */
bool UT_Multipart::append (const char * buffer, UT_uint32 length)
{
	if (m_buf == 0) return false;

	if ((buffer == 0) || (length == 0)) return true; // ??

	if (isBase64 ()) return append_Base64 (buffer, length);
	if (isQuoted ()) return append_Quoted (buffer, length);

	return m_buf->append (reinterpret_cast<const UT_Byte *>(buffer), length);
}

bool UT_Multipart::append_Base64 (const char * buffer, UT_uint32 length)
{
	bool success = true;

	char binbuffer[60];

	const char * bufptr = buffer;
	const char * bufend = buffer + length;

	while (bufptr < bufend)
		{
			char c = *bufptr++;
			bool bEnd = (c == '=');

			unsigned char u = static_cast<unsigned char>(c);
			if (isspace (static_cast<int>( u))) continue;

			m_b64buffer[m_b64length++] = c;

			bool bFlush = (m_b64length == 80) || bEnd ||
						  ((bufptr == bufend) && m_b64length && ((m_b64length & 0x03) == 0));
			if (bFlush)
				{
					const char * b64bufptr = m_b64buffer;

					char * binbufptr = binbuffer;
					size_t binlength = sizeof (binbuffer);

					if (!UT_UTF8_Base64Decode (binbufptr, binlength, b64bufptr, m_b64length))
						success = false;
					if (m_b64length) memmove (m_b64buffer, b64bufptr, m_b64length);

					if (m_b64length > 3)
						{
							UT_DEBUGMSG(("Multipart HTML: append_Base64: oddness while decoding!\n"));
							success = false;
						}
					if (binlength < sizeof (binbuffer))
						if (!m_buf->append (reinterpret_cast<UT_Byte *>(binbuffer), sizeof (binbuffer) - binlength)) success = false;
				}
			if (bEnd || !success) break;
		}
	return success;
}

/* Decode a whole quoted-printable body (RFC 2045 6.7): =XX hex escapes and
 * =<CRLF>/=<LF> soft line breaks. A truncated or non-hex escape is emitted
 * literally rather than reading past the buffer.
 */
bool UT_Multipart::append_Quoted (const char * buffer, UT_uint32 length)
{
	std::string out;
	out.reserve (length);

	const char * bufptr = buffer;
	const char * bufend = buffer + length;

	while (bufptr < bufend)
		{
			char c = *bufptr;
			if (c != '=')
				{
					out += c;
					bufptr++;
					continue;
				}
			if (bufptr + 1 == bufend) break; // trailing '=' — soft break at EOF
			if (bufptr[1] == '\r' || bufptr[1] == '\n')
				{
					bufptr++;
					if (*bufptr == '\r')
						{
							bufptr++;
							if (bufptr < bufend && *bufptr == '\n') bufptr++;
						}
					else bufptr++;
					continue; // soft line break
				}
			if (bufptr + 2 >= bufend)
				{
					// '=' followed by a single trailing char — not a valid escape
					out += c;
					bufptr++;
					continue;
				}
			int hi = s_hexval (bufptr[1]);
			int lo = s_hexval (bufptr[2]);
			if (hi < 0 || lo < 0)
				{
					out += c; // invalid escape — literal '='
					bufptr++;
					continue;
				}
			out += static_cast<char>((hi << 4) | lo);
			bufptr += 3;
		}
	return m_buf->append (reinterpret_cast<const UT_Byte *>(out.data()), static_cast<UT_uint32>(out.size()));
}

UT_ByteBufPtr && UT_Multipart::detachBuffer ()
{
    return std::move(m_buf);
}

void UT_Multipart::clear ()
{
	// values are g_strdup'd - must go through g_free, not C++ delete
	m_map->freeData();
	m_map->clear ();

	if (m_buf) m_buf->truncate (0);
}
