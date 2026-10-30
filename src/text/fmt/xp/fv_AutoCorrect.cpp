/* Abinova
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

#include <string>
#include <vector>

#include "fv_View.h"
#include "fl_BlockLayout.h"
#include "pd_Document.h"
#include "pp_AttrProp.h"
#include "ut_growbuf.h"
#include "ut_string.h"
#include "xap_App.h"
#include "xap_Prefs.h"
#include "ap_Prefs_SchemeIds.h"

/*!
 * AutoCorrect / AutoFormat-as-you-type (Word parity, W59).
 *
 * After the user types a word-boundary trigger char (space or
 * ",.;:!?"), or a char that can complete a replacement entry
 * (')', '>', '*', '_'), the word ending at the caret is examined and,
 * inside one user-atomic glob (= one undo step), rewritten:
 *
 *  - `*bold*` / `_italic_` spans: the marks are removed and the text
 *    between them gets real bold/italic, when the pair wraps a
 *    non-empty span inside one paragraph;
 *  - TWo INitial CApitals ("THis" -> "This") and cAPS LOCK
 *    ("wORD" -> "Word");
 *  - the replacement table (typos, (c) -> copyright sign, --> -> arrow,
 *    1/2 -> fraction, smileys, "..." -> ellipsis);
 *
 * Reverting an automatic case fix (undo, or deleting and retyping the
 * same word at the same spot) adds the word to the initial-caps
 * exception list so it is never corrected again; the list persists in
 * the AutoCorrectExceptions profile pref.
 */

namespace
{

struct AC_Pair { const char * szFrom; const char * szTo; };

/// Built-in AutoCorrect replacements (same list Word parity pins:
/// common typos and symbols; English words apply to English text
/// only, symbols/fractions to every language).
static const AC_Pair s_acTable[] =
{
	{ "teh", "the" },
	{ "adn", "and" },
	{ "recieve", "receive" },
	{ "recieved", "received" },
	{ "seperate", "separate" },
	{ "definately", "definitely" },
	{ "occured", "occurred" },
	{ "untill", "until" },
	{ "wich", "which" },
	{ "becuase", "because" },
	{ "thier", "their" },
	{ "alot", "a lot" },
	{ "accomodate", "accommodate" },
	{ "acheive", "achieve" },
	{ "beleive", "believe" },
	{ "calender", "calendar" },
	{ "goverment", "government" },
	{ "tommorow", "tomorrow" },
	{ "wierd", "weird" },
	{ "i", "I" },
	{ "dont", "don't" },
	{ "cant", "can't" },
	{ "wont", "won't" },
	{ "didnt", "didn't" },
	{ "doesnt", "doesn't" },
	{ "isnt", "isn't" },
	{ "(c)", "\xC2\xA9" },			/* © */
	{ "(r)", "\xC2\xAE" },			/* ® */
	{ "(tm)", "\xE2\x84\xA2" },		/* ™ */
	{ "...", "\xE2\x80\xA6" },		/* … */
	{ "->", "\xE2\x86\x92" },		/* → */
	{ "-->", "\xE2\x86\x92" },		/* → */
	{ "<-", "\xE2\x86\x90" },		/* ← */
	{ "=>", "\xE2\x87\x92" },		/* ⇒ */
	{ ":)", "\xE2\x98\xBA" },		/* ☺ */
	{ ":-)", "\xE2\x98\xBA" },		/* ☺ */
	{ ":(", "\xE2\x98\xB9" },		/* ☹ */
	{ ":-(", "\xE2\x98\xB9" },		/* ☹ */
	{ ":|", "\xF0\x9F\x98\x90" },	/* 😐 */
	{ "1/2", "\xC2\xBD" },			/* ½ */
	{ "1/4", "\xC2\xBC" },			/* ¼ */
	{ "3/4", "\xC2\xBE" },			/* ¾ */
};

/// Smileys replaced even when typed straight after a word ("ok:)").
static const char * const s_smileys[] = { ":-)", ":-(", ":)", ":(", ":|" };

/// Built-in words that keep two initial capitals.
static const char * const s_initialCapsExceptions[] =
{
	"CDs", "IDs", "PCs", "TVs", "DJs", "GBs", "KBs", "MBs", "TBs", "VMs"
};

/// Words with a lowercase first letter and the rest in capitals that
/// are not cAPS LOCK slips.
static const char * const s_capsLockExceptions[] =
{
	"iOS", "mRNA", "tRNA", "rRNA", "cDNA", "eSIM", "eSATA", "iSCSI", "vCPU"
};

/// Word-boundary trigger characters: autocorrect fires after typing
/// one of these.
static bool s_isBoundaryTrigger(UT_UCS4Char c)
{
	return c == UCS_SPACE || c == ',' || c == '.' || c == ';' ||
		c == ':' || c == '!' || c == '?';
}

/// Characters that may complete a replacement entry or an emphasis
/// span, so they fire the pass immediately.
static bool s_isCompleteTrigger(UT_UCS4Char c)
{
	return c == ')' || c == '>' || c == '*' || c == '_';
}

/// Word-edge characters: openers that may precede the letter run of a
/// word (" ' “ ‘ ( [ {), closers that may follow it (" ' ” ’ ) ] }).
static bool s_isOpenEdge(UT_UCS4Char c)
{
	return c == '"' || c == '\'' || c == 0x201C || c == 0x2018 ||
		c == '(' || c == '[' || c == '{';
}

static bool s_isCloseEdge(UT_UCS4Char c)
{
	return c == '"' || c == '\'' || c == 0x201D || c == 0x2019 ||
		c == ')' || c == ']' || c == '}';
}

/// A built-in entry that is an English word (letters and apostrophes
/// only), not a symbol — those apply to English text only, like Word
/// keeps the word list per language.
static bool s_isEnglishEntry(const char * sz)
{
	bool bAlpha = false;
	for (const unsigned char * p = reinterpret_cast<const unsigned char *>(sz); *p; ++p)
	{
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '\''))
			return false;
		bAlpha = true;
	}
	return bAlpha;
}

/// UTF-8 word -> ASCII-bag compare helper: word is UCS4, keys ASCII.
static bool s_wordEquals(const UT_UCS4Char * pWord, UT_uint32 iLen, const char * szKey)
{
	UT_uint32 i = 0;
	for (; szKey[i] && i < iLen; ++i)
	{
		if (pWord[i] != static_cast<UT_UCS4Char>(static_cast<unsigned char>(szKey[i])))
			return false;
	}
	return szKey[i] == '\0' && i == iLen;
}

/// word matches key case-insensitively and starts with a capital
/// ("Teh" matches "teh" -> "The").
static bool s_wordCapMatch(const UT_UCS4Char * pWord, UT_uint32 iLen, const char * szKey)
{
	if (!iLen || !UT_UCS4_isupper(pWord[0]))
		return false;
	UT_uint32 i = 0;
	for (; szKey[i] && i < iLen; ++i)
	{
		if (UT_UCS4_tolower(pWord[i]) != static_cast<UT_UCS4Char>(static_cast<unsigned char>(szKey[i])))
			return false;
	}
	return szKey[i] == '\0' && i == iLen;
}

static bool s_isAllLowerASCII(const char * sz)
{
	for (const char * p = sz; *p; ++p)
		if (*p >= 'A' && *p <= 'Z')
			return false;
	return true;
}

} // anonymous namespace

/*!
 * Called from cmdCharInsert after a successful character insert.
 * Cheap gate: only a word-boundary or entry-completing last char can
 * trigger a pass.
 */
void FV_View::_autoCorrectAfterInsert(const UT_UCS4Char * text, UT_uint32 count)
{
	if (!text || !count)
		return;
	const UT_UCS4Char last = text[count - 1];
	if (!s_isBoundaryTrigger(last) && !s_isCompleteTrigger(last))
		return;
	_autoCorrectRun(false);
}

/// Called from insertParagraphBreak: the word ending at the caret is
/// corrected before the break lands ("teh" + Enter -> "the" + Enter).
void FV_View::_autoCorrectOnEnter(void)
{
	_autoCorrectRun(true);
}

/*!
 * Add a word (UCS4) to the initial-caps exception list — session set
 * plus the persisted AutoCorrectExceptions pref (space-separated).
 */
void FV_View::_autoCorrectAddException(const UT_UCS4Char * pWord, UT_uint32 iLen)
{
	std::string sWord = UT_UCS4String(pWord, iLen).utf8_str();
	for (const auto & e : m_vecAutoCapExceptions)
	{
		if (e == sWord)
			return;
	}
	for (const char * b : s_initialCapsExceptions)
	{
		if (sWord == b)
			return;
	}
	m_vecAutoCapExceptions.push_back(sWord);

	XAP_Prefs * pPrefs = m_pApp ? m_pApp->getPrefs() : nullptr;
	if (!pPrefs)
		return;
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	if (!pScheme)
		return;
	std::string sList;
	for (const auto & e : m_vecAutoCapExceptions)
	{
		if (!sList.empty())
			sList += ' ';
		sList += e;
	}
	pScheme->setValue(AP_PREF_KEY_AutoCorrectExceptions, sList.c_str());
}

/*!
 * The AutoCorrect pass.  Returns true when the document was changed.
 * All rewrites happen inside one user-atomic glob so they undo as a
 * single step separate from the typed text (Word parity).
 */
bool FV_View::_autoCorrectRun(bool bOnEnter)
{
	bool bEnabled = true;
	if (m_pApp)
		m_pApp->getPrefsValueBool(AP_PREF_KEY_AutoCorrectEnable, bEnabled);
	if (!bEnabled || !isSelectionEmpty() || m_FrameEdit.isActive() ||
		m_pDoc->isDoingTheDo() || m_pDoc->isDoingPaste() ||
		m_pDoc->isEditingLocked())
		return false;

	fl_BlockLayout * pBlock = getCurrentBlock();
	UT_return_val_if_fail(pBlock, false);
	PT_DocPosition posBlock = pBlock->getPosition(true);
	PT_DocPosition posCaret = getPoint();
	if (posCaret <= posBlock + 1)
		return false;

	UT_GrowBuf buf;
	if (!pBlock->getBlockBuf(&buf))
		return false;
	// a second buffer holds the block text after the emphasis marks
	// are deleted (UT_GrowBuf copies are shallow — never assign)
	UT_GrowBuf bufAfter;
	const UT_UCS4Char * pText =
		reinterpret_cast<const UT_UCS4Char *>(buf.getPointer(0));
	UT_return_val_if_fail(pText, false);

	UT_sint32 iOff = static_cast<UT_sint32>(posCaret - posBlock - 1);
	if (iOff < 1 || iOff > static_cast<UT_sint32>(buf.getLength()))
		return false;

	UT_uint32 wordEnd;
	if (bOnEnter)
	{
		wordEnd = static_cast<UT_uint32>(iOff);
	}
	else
	{
		const UT_UCS4Char last = pText[iOff - 1];
		if (s_isBoundaryTrigger(last))
			wordEnd = static_cast<UT_uint32>(iOff - 1);
		else if (s_isCompleteTrigger(last))
			wordEnd = static_cast<UT_uint32>(iOff);
		else
			return false;
	}

	// lazily load the persisted exception list once per view
	if (!m_bAutoCapExceptionsLoaded)
	{
		m_bAutoCapExceptionsLoaded = true;
		XAP_Prefs * pPrefs = m_pApp ? m_pApp->getPrefs() : nullptr;
		std::string sList;
		if (pPrefs && pPrefs->getPrefsValue(AP_PREF_KEY_AutoCorrectExceptions, sList))
		{
			size_t i = 0;
			while (i < sList.size())
			{
				size_t j = sList.find(' ', i);
				std::string w = sList.substr(i, j == std::string::npos ? j : j - i);
				if (!w.empty())
					m_vecAutoCapExceptions.push_back(w);
				if (j == std::string::npos)
					break;
				i = j + 1;
			}
		}
	}

	bool bGlob = false;
	UT_sint32 iDelta = 0;      // net text-length change before the caret

	// ---- *bold* / _italic_ span ending at the word end ----
	if (wordEnd >= 1 &&
		(pText[wordEnd - 1] == '*' || pText[wordEnd - 1] == '_'))
	{
		const UT_UCS4Char mark = pText[wordEnd - 1];
		UT_sint32 iOpen = -1;
		for (UT_sint32 i = static_cast<UT_sint32>(wordEnd) - 2; i >= 0; --i)
		{
			if (pText[i] == mark)
			{
				iOpen = i;
				break;
			}
		}
		if (iOpen >= 0)
		{
			const UT_uint32 iIn0 = static_cast<UT_uint32>(iOpen) + 1;
			const UT_uint32 iIn1 = wordEnd - 1;
			bool bOk = iIn1 > iIn0 &&
				!UT_UCS4_isspace(pText[iIn0]) &&
				!UT_UCS4_isspace(pText[iIn1 - 1]);
			bool bAlnum = false;
			for (UT_uint32 i = iIn0; bOk && i < iIn1; ++i)
			{
				if (pText[i] == UCS_ABI_OBJECT || pText[i] == 0)
				{
					bOk = false;
					break;
				}
				if (UT_UCS4_isalnum(pText[i]))
					bAlnum = true;
			}
			bOk = bOk && bAlnum;
			// the opening mark must start a word
			if (bOk)
			{
				bOk = iOpen == 0 || UT_UCS4_isspace(pText[iOpen - 1]) ||
					s_isOpenEdge(pText[iOpen - 1]);
			}
			if (bOk)
			{
				if (!bGlob)
				{
					m_pDoc->beginUserAtomicGlob();
					bGlob = true;
				}
				const PT_DocPosition posOpen = posBlock + 1 + iOpen;
				const PT_DocPosition posClose = posBlock + 1 + wordEnd - 1;
				PP_PropertyVector props;
				props.push_back(mark == '*' ? "font-weight" : "font-style");
				props.push_back(mark == '*' ? "bold" : "italic");
				m_pDoc->changeSpanFmt(PTC_AddFmt, posOpen + 1, posClose,
									  PP_NOPROPS, props);
				UT_uint32 iReal = 0;
				m_pDoc->deleteSpan(posClose, posClose + 1, nullptr, iReal);
				iReal = 0;
				m_pDoc->deleteSpan(posOpen, posOpen + 1, nullptr, iReal);
				iDelta -= 2;
				wordEnd -= 2;
				// refresh the text view after the deletions
				if (pBlock->getBlockBuf(&bufAfter))
				{
					pText = reinterpret_cast<const UT_UCS4Char *>(bufAfter.getPointer(0));
				}
			}
		}
	}

	// ---- the word ending at wordEnd ----
	UT_uint32 wordStart = wordEnd;
	while (wordStart > 0 && !UT_UCS4_isspace(pText[wordStart - 1]))
		--wordStart;
	// a leading emphasis mark that opens a span is outside the word
	if (wordEnd - wordStart >= 2 &&
		(pText[wordStart] == '*' || pText[wordStart] == '_'))
	{
		const UT_UCS4Char mark = pText[wordStart];
		bool bNoMark = true;
		for (UT_uint32 i = wordStart + 1; i < wordEnd; ++i)
		{
			if (pText[i] == mark)
			{
				bNoMark = false;
				break;
			}
		}
		if (bNoMark)
			++wordStart;
	}
	UT_uint32 wordLen = wordEnd - wordStart;
	if (wordLen == 0)
	{
		if (bGlob)
			m_pDoc->endUserAtomicGlob();
		return iDelta != 0;
	}

	std::vector<UT_UCS4Char> vWord(pText + wordStart, pText + wordEnd);
	const PT_DocPosition posWord = posBlock + 1 + wordStart;

	// ---- case fix: TWo INitial CApitals / cAPS LOCK ----
	// the word may be wrapped in quotes/brackets; fix only the letters
	UT_uint32 at = 0;
	while (at < wordLen && !UT_UCS4_isalpha(vWord[at]))
	{
		if (!s_isOpenEdge(vWord[at]))
		{
			at = wordLen;
			break;
		}
		++at;
	}
	UT_uint32 iLetEnd = at;
	while (iLetEnd < wordLen && UT_UCS4_isalpha(vWord[iLetEnd]))
		++iLetEnd;
	UT_uint32 nLet = iLetEnd - at;
	bool bTailOk = true;
	for (UT_uint32 i = iLetEnd; i < wordLen; ++i)
	{
		if (!s_isCloseEdge(vWord[i]))
		{
			bTailOk = false;
			break;
		}
	}
	if (at < wordLen && nLet >= 3 && bTailOk)
	{
		const UT_UCS4Char * let = &vWord[at];
		bool bTwoCaps = UT_UCS4_isupper(let[0]) && UT_UCS4_isupper(let[1]);
		for (UT_uint32 k = 2; bTwoCaps && k < nLet; ++k)
			bTwoCaps = UT_UCS4_islower(let[k]);
		bool bCapsLock = UT_UCS4_islower(let[0]);
		for (UT_uint32 k = 1; bCapsLock && k < nLet; ++k)
			bCapsLock = UT_UCS4_isupper(let[k]);

		if (bTwoCaps || bCapsLock)
		{
			const std::string sLet = UT_UCS4String(let, nLet).utf8_str();
			bool bExcept = false;
			if (bTwoCaps)
			{
				for (const char * e : s_initialCapsExceptions)
					bExcept = bExcept || sLet == e;
				for (const auto & e : m_vecAutoCapExceptions)
					bExcept = bExcept || sLet == e;
			}
			else
			{
				for (const char * e : s_capsLockExceptions)
					bExcept = bExcept || sLet == e;
			}
			const PT_DocPosition posLet = posWord + at;
			if (!bExcept &&
				m_bLastAutoCapFixValid && posLet == m_posLastAutoCapFix &&
				m_typedLastAutoCapFix == UT_UCS4String(let, nLet))
			{
				// the user reverted this fix once — it becomes an
				// exception (Word parity: auto-add on revert)
				_autoCorrectAddException(let, nLet);
				m_bLastAutoCapFixValid = false;
			}
			else if (!bExcept)
			{
				std::vector<UT_UCS4Char> vFixed(nLet);
				if (bTwoCaps)
				{
					vFixed[0] = let[0];
					vFixed[1] = UT_UCS4_tolower(let[1]);
					for (UT_uint32 k = 2; k < nLet; ++k)
						vFixed[k] = let[k];
				}
				else
				{
					vFixed[0] = UT_UCS4_toupper(let[0]);
					for (UT_uint32 k = 1; k < nLet; ++k)
						vFixed[k] = UT_UCS4_tolower(let[k]);
				}
				bool bDiff = false;
				for (UT_uint32 k = 0; k < nLet; ++k)
					bDiff = bDiff || vFixed[k] != let[k];
				if (bDiff)
				{
					if (!bGlob)
					{
						m_pDoc->beginUserAtomicGlob();
						bGlob = true;
					}
					const PP_AttrProp * pAP = nullptr;
					m_pDoc->getSpanAttrProp(pBlock->getStruxDocHandle(),
											wordStart + at, true, &pAP);
					UT_uint32 iReal = 0;
					m_pDoc->deleteSpan(posLet, posLet + nLet, nullptr, iReal);
					m_pDoc->insertSpan(posLet, vFixed.data(), nLet,
									   const_cast<PP_AttrProp *>(pAP));
					m_posLastAutoCapFix = posLet;
					m_typedLastAutoCapFix = UT_UCS4String(let, nLet);
					m_bLastAutoCapFixValid = true;
					for (UT_uint32 k = 0; k < nLet; ++k)
						vWord[at + k] = vFixed[k];
				}
			}
		}
	}

	// ---- replacement table (whole word, then suffix fallbacks) ----
	// English typo entries apply to English text only (Word keeps the
	// list per language); symbols apply in every language.
	bool bEnglish = true;
	{
		UT_UTF8String sLang;
		bool bExplicit = false;
		if (queryCharFormat("lang", sLang, bExplicit, posWord) &&
			sLang.byteLength() >= 2 &&
			!(sLang.utf8_str()[0] == 'e' && sLang.utf8_str()[1] == 'n'))
		{
			bEnglish = false;
		}
	}

	std::string sRep;
	UT_uint32 repOff = 0, repLen = wordLen;
	const UT_UCS4Char * pWord = vWord.data();
	for (const auto & e : s_acTable)
	{
		if (!bEnglish && s_isEnglishEntry(e.szFrom))
			continue;
		if (s_wordEquals(pWord, wordLen, e.szFrom))
		{
			sRep = e.szTo;
			break;
		}
		if (s_isAllLowerASCII(e.szFrom) && s_wordCapMatch(pWord, wordLen, e.szFrom))
		{
			// capitalised word -> capitalise the replacement's first
			// letter when it is a lowercase ASCII letter
			sRep = e.szTo;
			if (sRep[0] >= 'a' && sRep[0] <= 'z')
				sRep[0] = static_cast<char>(sRep[0] - 'a' + 'A');
			break;
		}
	}
	if (sRep.empty())
	{
		if (wordLen > 3 && pWord[wordLen - 1] == '.' &&
			pWord[wordLen - 2] == '.' && pWord[wordLen - 3] == '.')
		{
			// "wait..." -> "wait…"
			repOff = wordLen - 3;
			repLen = 3;
			sRep = "\xE2\x80\xA6";
		}
		else
		{
			for (const char * m : s_smileys)
			{
				const UT_uint32 mLen = static_cast<UT_uint32>(strlen(m));
				if (wordLen > mLen &&
					s_wordEquals(pWord + wordLen - mLen, mLen, m))
				{
					repOff = wordLen - mLen;
					repLen = mLen;
					for (const auto & e : s_acTable)
					{
						if (strcmp(e.szFrom, m) == 0)
						{
							sRep = e.szTo;
							break;
						}
					}
					break;
				}
			}
		}
	}
	if (!sRep.empty())
	{
		if (!bGlob)
		{
			m_pDoc->beginUserAtomicGlob();
			bGlob = true;
		}
		UT_UCS4String uRep(sRep.c_str());
		const PP_AttrProp * pAP = nullptr;
		m_pDoc->getSpanAttrProp(pBlock->getStruxDocHandle(),
								wordStart + repOff, true, &pAP);
		const PT_DocPosition posRep = posWord + repOff;
		UT_uint32 iReal = 0;
		m_pDoc->deleteSpan(posRep, posRep + repLen, nullptr, iReal);
		m_pDoc->insertSpan(posRep, uRep.ucs4_str(),
						   uRep.length(), const_cast<PP_AttrProp *>(pAP));
		iDelta += static_cast<UT_sint32>(uRep.length()) -
			static_cast<UT_sint32>(repLen);
	}

	if (bGlob)
	{
		m_pDoc->endUserAtomicGlob();
		/* every rewrite above ends with an insertSpan that leaves the
		 * caret at the end of the inserted text — i.e. *before* the
		 * boundary char the user just typed ('thequick ' instead of
		 * 'the quick' when they type on).  Put it back where their
		 * typing left it; same-length fixes (iDelta == 0) need this
		 * just as much as length-changing ones. */
		_setPoint(posCaret + iDelta);
		_generalUpdate();
		notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	}
	return iDelta != 0;
}
