/* AbiWord
 * Copyright (C) 2025 AbiWord contributors
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

#ifndef IE_EXP_MARKDOWN_H
#define IE_EXP_MARKDOWN_H

#include "ie_exp.h"
#include "pl_Listener.h"

class PD_Document;
class Markdown_Listener;

class IE_Exp_Markdown : public IE_Exp
{
	friend class Markdown_Listener;

public:
	IE_Exp_Markdown(PD_Document * pDocument);
	virtual ~IE_Exp_Markdown();

	virtual UT_Error _writeDocument(void) override;

protected:
	virtual PL_Listener * _constructListener(void);

	Markdown_Listener * m_pListener;
	UT_Error m_error;
};

class ABI_EXPORT IE_Exp_Markdown_Sniffer : public IE_ExpSniffer
{
public:
	IE_Exp_Markdown_Sniffer();
	virtual ~IE_Exp_Markdown_Sniffer();

	virtual bool recognizeSuffix(const char * szSuffix) override;
	virtual UT_Error constructExporter(PD_Document * pDocument,
									   IE_Exp ** ppie) override;
	virtual bool getDlgLabels(const char ** szDesc,
							  const char ** pszSuffixList,
							  IEFileType * ft) override;
	virtual UT_Confidence_t supportsMIME(const char * szMIME) override;
	virtual UT_UTF8String getPreferredSuffix() override;
};

#endif /* IE_EXP_MARKDOWN_H */
