/* AbiWord
 * Copyright (C) 2004 Tomas Frydrych <tomasfrydrych@yahoo.co.uk>
 * Copyright (C) 2025 Hubert Figuière
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

#include "ut_uuid.h"
#include "ut_std_string.h"

#include "xap_Dlg_DocComparison.h"
#include "xad_Document.h"
#include "xap_App.h"
#include "xap_Strings.h"

#include <locale.h>

XAP_Dialog_DocComparison::XAP_Dialog_DocComparison(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id):
	XAP_Dialog_NonPersistent(pDlgFactory,id),
	m_pDoc1(nullptr),
	m_pDoc2(nullptr),
	m_pSS(nullptr),
	m_iVersionOfDiff(0),
	m_tTimeOfDiff(0),
	m_iPosOfDiff(0),
	m_iPosOfFmtDiff(0),
	m_bStylesEqual(false)
{
	m_pSS = XAP_App::getApp()->getStringSet();
}

/*!
    returns true on success
*/
bool XAP_Dialog_DocComparison::calculate(AD_Document * pDoc1, AD_Document * pDoc2)
{
	UT_return_val_if_fail(pDoc1 && pDoc2, false);
	m_pDoc1 = pDoc1;
	m_pDoc2 = pDoc2;
	bool bEqual = false;

	if(pDoc1->areDocumentsRelated(*pDoc2))
	{
		bEqual = pDoc1->areDocumentHistoriesEqual(*pDoc2, m_iVersionOfDiff);

		if(!bEqual)
		{
			const AD_VersionData * v = pDoc1->findHistoryRecord(m_iVersionOfDiff);
			if(v)
			{
				m_tTimeOfDiff = v->getTime();
			}
			else
			{
				UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
				m_iVersionOfDiff = 0;
			}
		}
		else
		{
			m_iVersionOfDiff = 0xffffffff;
		}
	}

	if(!bEqual)
	{
		m_bStylesEqual   = pDoc1->areDocumentStylesheetsEqual(*pDoc2);

		if(pDoc1->areDocumentContentsEqual(*pDoc2, m_iPosOfDiff))
		{
			m_iPosOfDiff = 0xffffffff;
			if(pDoc1->areDocumentFormatsEqual(*pDoc2, m_iPosOfFmtDiff))
			{
				m_iPosOfFmtDiff = 0xffffffff;
			}
		}
	}

	return true;
}


const char * XAP_Dialog_DocComparison::getWindowLabel() const
{
	UT_return_val_if_fail(m_pSS, nullptr);
	return m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_WindowLabel);
}

const char * XAP_Dialog_DocComparison::getFrame1Label() const
{
	UT_return_val_if_fail(m_pSS, nullptr);
	return m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_DocsCompared);
}

const char * XAP_Dialog_DocComparison::getFrame2Label() const
{
	UT_return_val_if_fail(m_pSS, nullptr);
	return m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_Results);
}

std::string XAP_Dialog_DocComparison::getPath1() const
{
	UT_return_val_if_fail(m_pDoc1, "");
	return UT_ellipsisPath(m_pDoc1->getFilename(), 60, 50);
}

std::string XAP_Dialog_DocComparison::getPath2() const
{
	UT_return_val_if_fail(m_pDoc2, "");
	return UT_ellipsisPath(m_pDoc2->getFilename(), 60, 50);
}


const char * XAP_Dialog_DocComparison::getResultLabel(UT_uint32 indx) const
{
	UT_return_val_if_fail(m_pSS, nullptr);
	switch(indx)
	{
		case 0: return m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_Relationship);
		case 1: return m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_Content);
		case 2: return m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_Fmt);
		case 3: return m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_Styles);
		default:;
	}

	UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	return nullptr;
}


std::string XAP_Dialog_DocComparison::getResultValue(UT_uint32 indx) const
{
	UT_return_val_if_fail(m_pSS, nullptr);

	struct tm * tM;
	char * s;
	std::string resultValue;

	switch(indx)
	{
		case 0: // relationship
			if(m_iVersionOfDiff == 0xffffffff)
			{
				// equal
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_Identical, resultValue);
				return resultValue;
			}
			else if(m_iVersionOfDiff == 0)
			{
				// unrelated
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_Unrelated, resultValue);
				return resultValue;
			}
			else
			{
				// siblings
				std::string s1;
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_Siblings, s1);
				s1 += "; ";
				s1 += m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_Diverging);

				tM = localtime(&m_tTimeOfDiff);
				s = (char*)g_try_malloc(30);
				strftime(s,30,"%c",tM);

				resultValue = UT_std_string_sprintf(s1.c_str(), m_iVersionOfDiff, s);
				FREEP(s);
				return resultValue;
			}

		case 1: // content
			if(m_iVersionOfDiff == 0xffffffff)
			{
				// test skipped
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_TestSkipped, resultValue);
				return resultValue;
			}
			else if(m_iPosOfDiff == 0xffffffff)
			{
				// equal
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_Identical, resultValue);
				return resultValue;
			}
			else
			{
				const char *s2 = m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_DivergingPos);
				return UT_std_string_sprintf(s2, m_iPosOfDiff);
			}

		case 2: // fmt
			if(m_iVersionOfDiff == 0xffffffff || m_iPosOfDiff != 0xffffffff)
			{
				// either the docs are know to be equal, or the
				// content is not known not to be equal
				// test skipped
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_TestSkipped, resultValue);
				return resultValue;
			}
			else if(m_iPosOfFmtDiff == 0xffffffff)
			{
				// equal
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_Identical, resultValue);
				return resultValue;
			}
			else
			{
				const char * s2 = m_pSS->getValue(XAP_STRING_ID_DLG_DocComparison_DivergingPos);
				return UT_std_string_sprintf(s2, m_iPosOfFmtDiff);
			}

		case 3: // styles
			if(m_iVersionOfDiff == 0xffffffff)
			{
				// test skipped
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_TestSkipped, resultValue);
				return resultValue;
			}
			else if(m_bStylesEqual)
			{
				// equal
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_Identical, resultValue);
				return resultValue;
			}
			else
			{
				m_pSS->getValueUTF8(XAP_STRING_ID_DLG_DocComparison_Different, resultValue);
				return resultValue;
			}

		default:;
	}

	UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	return resultValue;
}


const char * XAP_Dialog_DocComparison::getButtonLabel() const
{
	UT_return_val_if_fail(m_pSS, nullptr);
	return m_pSS->getValue(XAP_STRING_ID_DLG_Close);
}



