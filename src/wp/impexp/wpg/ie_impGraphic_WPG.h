/* Abinova
 * Copyright (C) 2006 Marc Maurer <uwog@uwog.net>
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

#ifndef IE_IMP_WPG_H
#define IE_IMP_WPG_H

#include <map>
#include <string>

#include "ut_compiler.h"
ABI_W_NO_SUGGEST_OVERRIDE
#include <libwpg/libwpg.h>
ABI_W_POP
#include "ie_impGraphic_SVG.h"

/*! librevenge input-stream adapter over a GsfInput: exposes OLE/zip
 * containers (a .wpg embedded in a PerfectOffice package) as structured
 * streams so libwpg can reach the PerfectOffice_MAIN member.  Declared
 * here rather than file-local so the substream API is unit-testable.
 */
class ABI_EXPORT AbiWordPerfectGraphicsInputStream : public librevenge::RVNGInputStream
{
public:
	AbiWordPerfectGraphicsInputStream(GsfInput *input);
	virtual ~AbiWordPerfectGraphicsInputStream();

	virtual bool isStructured() override;
	virtual unsigned subStreamCount() override;
	virtual const char* subStreamName(unsigned) override;
	virtual bool existsSubStream(const char*) override;
	virtual librevenge::RVNGInputStream* getSubStreamByName(const char*) override;
	virtual librevenge::RVNGInputStream* getSubStreamById(unsigned) override;
	virtual const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead) override;
	virtual int seek(long offset, librevenge::RVNG_SEEK_TYPE seekType) override;
	virtual long tell() override;
	virtual bool isEnd() override;

private:
	/* lazily wraps m_input in an OLE-then-zip container view */
	GsfInfile * container();

	GsfInput *m_input;
	GsfInfile *m_ole;
	std::map<unsigned, std::string> m_substreams;
};

class IE_Imp_WordPerfectGraphics_Sniffer final : public IE_ImpGraphicSniffer
{
	friend class IE_Imp;
	friend class IE_Imp_WordPerfectGraphics;

public:
	IE_Imp_WordPerfectGraphics_Sniffer();
	virtual ~IE_Imp_WordPerfectGraphics_Sniffer();

	virtual const IE_SuffixConfidence * getSuffixConfidence() override;
	using IE_ImpGraphicSniffer::recognizeContents;

	virtual UT_Confidence_t recognizeContents(GsfInput * input) override;
	virtual const IE_MimeConfidence * getMimeConfidence() override { return nullptr; }
	virtual bool getDlgLabels(const char ** szDesc,
			const char ** szSuffixList,
			IEGraphicFileType *ft) override;
	virtual UT_Error constructImporter(IE_ImpGraphic **ppieg) override;
};

class IE_Imp_WordPerfectGraphics : public IE_ImpGraphic
{
public:
  using IE_ImpGraphic::importGraphic;

  virtual UT_Error	importGraphic(GsfInput *input, FG_ConstGraphicPtr& pfg) override;
};

#endif /* IE_IMP_WPG_H */
