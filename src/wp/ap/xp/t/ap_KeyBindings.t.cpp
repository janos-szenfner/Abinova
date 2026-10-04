/* -*- mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* Abinova
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

/* unit tests for the default (Word-compatible) key binding map:
 * loads the real "default" binding map through AP_BindingSet and
 * asserts that each documented shortcut resolves to the expected
 * edit method.  No document or GUI needed. */

#include "tf_test.h"

#include "ev_EditBits.h"
#include "ev_EditBinding.h"
#include "ev_EditMethod.h"
#include "ev_NamedVirtualKey.h"
#include "xap_EditMethods.h"
#include "ap_LoadBindings.h"

#include <cstring>
#include <memory>

#define TFSUITE "core.wp.ap.keybindings"

namespace {

static EV_EditBindingMap * s_pMap = nullptr;

/* resolve a character keypress (shift state is implicit in c) */
static bool expectChar(UT_uint32 c, EV_EditModifierState ems,
					   const char * szMethod)
{
	EV_EditBinding * b =
		s_pMap->findEditBinding(EV_EKP_PRESS | c | ems);
	return (b && b->getType() == EV_EBT_METHOD
			&& b->getMethod()
			&& !strcmp(b->getMethod()->getName(), szMethod));
}

/* resolve a named virtual key press */
static bool expectNVK(EV_EditBits nvk, EV_EditModifierState ems,
					  const char * szMethod)
{
	EV_EditBinding * b =
		s_pMap->findEditBinding(EV_EKP_PRESS | nvk | ems);
	return (b && b->getType() == EV_EBT_METHOD
			&& b->getMethod()
			&& !strcmp(b->getMethod()->getName(), szMethod));
}

}

TFTEST_MAIN("ap_KeyBindings")
{
	std::unique_ptr<EV_EditMethodContainer> pemc(AP_GetEditMethods());
	TFPASS(pemc != nullptr);

	AP_BindingSet bs(pemc.get());
	s_pMap = bs.getMap("default");
	TFPASS(s_pMap != nullptr);

	/* ---- file operations ---- */
	TFPASS(expectChar('n', EV_EMS_CONTROL, "fileNew"));
	TFPASS(expectChar('o', EV_EMS_CONTROL, "fileOpen"));
	TFPASS(expectChar('s', EV_EMS_CONTROL, "fileSave"));
	TFPASS(expectChar('S', EV_EMS_CONTROL, "fileSaveAs"));      /* Ctrl+Shift+S */
	TFPASS(expectNVK(EV_NVK_F12, 0, "fileSaveAs"));
	TFPASS(expectNVK(EV_NVK_F12, EV_EMS_SHIFT, "fileSave"));    /* Shift+F12 */
	TFPASS(expectNVK(EV_NVK_F12, EV_EMS_CONTROL, "fileOpen"));  /* Ctrl+F12 */
	TFPASS(expectNVK(EV_NVK_F12, EV_EMS_SHIFT | EV_EMS_CONTROL, "print"));
	TFPASS(expectChar('w', EV_EMS_CONTROL, "closeWindow"));
	TFPASS(expectChar('p', EV_EMS_CONTROL, "print"));
	TFPASS(expectNVK(EV_NVK_F4, EV_EMS_ALT, "querySaveAndExit"));
	TFPASS(expectNVK(EV_NVK_F2, EV_EMS_CONTROL, "printPreview"));

	/* ---- editing ---- */
	TFPASS(expectChar('z', EV_EMS_CONTROL, "undo"));
#ifdef __APPLE__
	TFPASS(expectChar('Z', EV_EMS_CONTROL, "redo"));    /* Cmd+Shift+Z */
#else
	TFPASS(expectChar('Z', EV_EMS_CONTROL, "undo"));    /* Ctrl+Shift+Z */
#endif
	TFPASS(expectChar('y', EV_EMS_CONTROL, "redo"));
	TFPASS(expectChar('x', EV_EMS_CONTROL, "cut"));
	TFPASS(expectChar('c', EV_EMS_CONTROL, "copy"));
	TFPASS(expectChar('v', EV_EMS_CONTROL, "paste"));
	TFPASS(expectChar('v', EV_EMS_ALT | EV_EMS_CONTROL, "pasteSpecial"));
	TFPASS(expectChar('V', EV_EMS_CONTROL, "formatPainter"));   /* Ctrl+Shift+V */
	TFPASS(expectChar('a', EV_EMS_CONTROL, "selectAll"));
	TFPASS(expectChar('f', EV_EMS_CONTROL, "find"));
	TFPASS(expectChar('h', EV_EMS_CONTROL, "replace"));
	TFPASS(expectChar('g', EV_EMS_CONTROL, "go"));
	TFPASS(expectNVK(EV_NVK_F5, 0, "go"));
	TFPASS(expectNVK(EV_NVK_BACKSPACE, EV_EMS_CONTROL, "delBOW"));
	TFPASS(expectNVK(EV_NVK_BACKSPACE, EV_EMS_ALT, "delBOW"));  /* Option+Delete */
	TFPASS(expectNVK(EV_NVK_DELETE, EV_EMS_CONTROL, "delEOW"));

	/* ---- character / paragraph formatting ---- */
	TFPASS(expectChar('b', EV_EMS_CONTROL, "toggleBold"));
	TFPASS(expectChar('i', EV_EMS_CONTROL, "toggleItalic"));
	TFPASS(expectChar('u', EV_EMS_CONTROL, "toggleUline"));
	TFPASS(expectChar('X', EV_EMS_CONTROL, "toggleStrike"));    /* Ctrl+Shift+X */
	TFPASS(expectChar('=', EV_EMS_CONTROL, "toggleSub"));       /* Ctrl+= */
	TFPASS(expectChar('+', EV_EMS_CONTROL, "toggleSuper"));     /* Ctrl+Shift+= */
	TFPASS(expectChar('^', EV_EMS_CONTROL, "toggleSuper"));
	TFPASS(expectNVK(EV_NVK_F3, EV_EMS_SHIFT, "rotateCase"));   /* Shift+F3 */
	TFPASS(expectChar('>', EV_EMS_CONTROL, "fontSizeIncrease"));
	TFPASS(expectChar('<', EV_EMS_CONTROL, "fontSizeDecrease"));
	TFPASS(expectChar('l', EV_EMS_CONTROL, "alignLeft"));
	TFPASS(expectChar('e', EV_EMS_CONTROL, "alignCenter"));
	TFPASS(expectChar('r', EV_EMS_CONTROL, "alignRight"));
	TFPASS(expectChar('j', EV_EMS_CONTROL, "alignJustify"));
	TFPASS(expectChar('1', EV_EMS_CONTROL, "singleSpace"));
	TFPASS(expectChar('5', EV_EMS_CONTROL, "middleSpace"));
	TFPASS(expectChar('2', EV_EMS_CONTROL, "doubleSpace"));
	TFPASS(expectChar('0', EV_EMS_CONTROL, "toggleParaBefore"));
	TFPASS(expectChar('m', EV_EMS_CONTROL, "toggleIndent"));
	TFPASS(expectChar('M', EV_EMS_CONTROL, "toggleUnIndent"));
	TFPASS(expectChar('q', EV_EMS_CONTROL, "clearParaFormatting"));
	TFPASS(expectNVK(EV_NVK_SPACE, EV_EMS_CONTROL, "togglePlain"));
	TFPASS(expectChar('N', EV_EMS_CONTROL, "setStyleNormal"));  /* Ctrl+Shift+N */
	TFPASS(expectChar('1', EV_EMS_ALT | EV_EMS_CONTROL, "setStyleHeading1"));
	TFPASS(expectChar('2', EV_EMS_ALT | EV_EMS_CONTROL, "setStyleHeading2"));
	TFPASS(expectChar('3', EV_EMS_ALT | EV_EMS_CONTROL, "setStyleHeading3"));
	TFPASS(expectChar('E', EV_EMS_CONTROL, "toggleMarkRevisions")); /* Ctrl+Shift+E */

	/* ---- navigation / selection ---- */
	TFPASS(expectNVK(EV_NVK_HOME, EV_EMS_CONTROL, "warpInsPtBOD"));
	TFPASS(expectNVK(EV_NVK_END, EV_EMS_CONTROL, "warpInsPtEOD"));
	TFPASS(expectNVK(EV_NVK_LEFT, EV_EMS_CONTROL, "warpInsPtBOW"));
	TFPASS(expectNVK(EV_NVK_RIGHT, EV_EMS_CONTROL, "warpInsPtEOW"));
	TFPASS(expectNVK(EV_NVK_LEFT, EV_EMS_ALT, "warpInsPtBOW")); /* Option+Left */
	TFPASS(expectNVK(EV_NVK_RIGHT, EV_EMS_ALT, "warpInsPtEOW"));/* Option+Right */
	TFPASS(expectNVK(EV_NVK_UP, EV_EMS_CONTROL, "warpInsPtBOB"));
	TFPASS(expectNVK(EV_NVK_DOWN, EV_EMS_CONTROL, "warpInsPtEOB"));
	TFPASS(expectNVK(EV_NVK_HOME, EV_EMS_SHIFT | EV_EMS_CONTROL, "extSelBOD"));
	TFPASS(expectNVK(EV_NVK_END, EV_EMS_SHIFT | EV_EMS_CONTROL, "extSelEOD"));

	/* ---- insertions ---- */
	TFPASS(expectNVK(EV_NVK_RETURN, EV_EMS_CONTROL, "insertPageBreak"));
	TFPASS(expectNVK(EV_NVK_RETURN, EV_EMS_SHIFT | EV_EMS_CONTROL, "insertColumnBreak"));
	TFPASS(expectNVK(EV_NVK_RETURN, EV_EMS_SHIFT, "insertLineBreak"));
	TFPASS(expectNVK(EV_NVK_SPACE, EV_EMS_SHIFT | EV_EMS_CONTROL, "insertNBSpace"));
	TFPASS(expectChar('_', EV_EMS_CONTROL, "insertNBHyphen"));  /* Ctrl+Shift+- */
	TFPASS(expectChar('-', EV_EMS_CONTROL, "insertSoftHyphen"));/* Ctrl+- */
	TFPASS(expectChar('-', EV_EMS_ALT | EV_EMS_CONTROL, "insertEmDash"));
	TFPASS(expectChar('_', EV_EMS_ALT | EV_EMS_CONTROL, "insertEnDash"));
	TFPASS(expectChar('r', EV_EMS_ALT | EV_EMS_CONTROL, "insertRegistered"));
	TFPASS(expectChar('t', EV_EMS_ALT | EV_EMS_CONTROL, "insertTrademark"));
	TFPASS(expectChar('c', EV_EMS_ALT | EV_EMS_CONTROL, "insertCopyright"));
	TFPASS(expectChar('f', EV_EMS_ALT | EV_EMS_CONTROL, "insFootnote"));
	TFPASS(expectChar('d', EV_EMS_ALT | EV_EMS_CONTROL, "insEndnote"));
	TFPASS(expectChar('m', EV_EMS_ALT | EV_EMS_CONTROL, "insAnnotation"));
	TFPASS(expectChar('k', EV_EMS_CONTROL, "insertHyperlink"));
	TFPASS(expectChar('P', EV_EMS_ALT, "insPageNo"));           /* Alt+Shift+P */
	TFPASS(expectChar('D', EV_EMS_ALT, "insDateTime"));         /* Alt+Shift+D */
	TFPASS(expectChar('X', EV_EMS_ALT, "refMarkEntry"));        /* Alt+Shift+X */
	TFPASS(expectChar('U', EV_EMS_ALT, "insertSumCols"));       /* Alt+Shift+U */

	/* ---- review ---- */
	TFPASS(expectNVK(EV_NVK_F7, 0, "dlgSpell"));
	TFPASS(expectChar(';', EV_EMS_CONTROL, "dlgSpell"));        /* Cmd+; */
	TFPASS(expectNVK(EV_NVK_UP, EV_EMS_ALT, "prevComment"));    /* Alt+Up */
	TFPASS(expectNVK(EV_NVK_DOWN, EV_EMS_ALT, "nextComment"));  /* Alt+Down */

	/* ---- view / windows ---- */
	TFPASS(expectChar('p', EV_EMS_ALT | EV_EMS_CONTROL, "viewPrintLayout"));
	TFPASS(expectChar('n', EV_EMS_ALT | EV_EMS_CONTROL, "viewNormalLayout"));
	TFPASS(expectChar('s', EV_EMS_ALT | EV_EMS_CONTROL, "viewSplit"));
	TFPASS(expectChar('C', EV_EMS_ALT, "viewSplit"));           /* Alt+Shift+C */
	TFPASS(expectNVK(EV_NVK_F6, EV_EMS_CONTROL, "cycleWindows"));
	TFPASS(expectNVK(EV_NVK_F6, EV_EMS_SHIFT | EV_EMS_CONTROL, "cycleWindowsBck"));
	TFPASS(expectNVK(EV_NVK_F8, EV_EMS_ALT, "executeScript"));  /* Alt+F8 */
	TFPASS(expectNVK(EV_NVK_F11, 0, "viewFullScreen"));
	TFPASS(expectChar(',', EV_EMS_CONTROL, "dlgOptions"));      /* Cmd+, */
}

namespace {

/* resolve a character keypress to a prefix sub-map (Ctrl+X, vi 'd'
 * operator, ...) and return it for further lookup */
static EV_EditBindingMap * expectPrefix(UT_uint32 c, EV_EditModifierState ems)
{
	EV_EditBinding * b =
		s_pMap->findEditBinding(EV_EKP_PRESS | c | ems);
	if (!b || b->getType() != EV_EBT_PREFIX)
		return nullptr;
	return b->getMap();
}

}

/* the emacs and vi compatibility maps ship enabled
 * (ENABLE_EMACS_KEYBINDING / ENABLE_VI_KEYBINDING) but nothing loaded
 * them in tests — load each ap_LB_* table through AP_BindingSet and
 * pin a representative binding per map plus the prefix-map chains. */
TFTEST_MAIN("ap_KeyBindings alt maps")
{
	std::unique_ptr<EV_EditMethodContainer> pemc(AP_GetEditMethods());
	TFPASS(pemc != nullptr);

	AP_BindingSet bs(pemc.get());

	/* ---- emacs ---- */
	s_pMap = bs.getMap("emacs");
	TFPASS(s_pMap != nullptr);
	TFPASS(expectChar('a', EV_EMS_CONTROL, "warpInsPtBOL"));
	TFPASS(expectChar('e', EV_EMS_CONTROL, "warpInsPtEOL"));
	TFPASS(expectChar('a', 0, "insertData"));      /* plain key self-inserts */
	TFPASS(expectChar('1', EV_EMS_CONTROL, "singleSpace"));

	/* Ctrl+X is a prefix into the emacsctrlx sub-map */
	{
		EV_EditBindingMap * sub = expectPrefix('x', EV_EMS_CONTROL);
		TFPASS(sub != nullptr);
		if (sub)
		{
			EV_EditBindingMap * outer = s_pMap;
			s_pMap = sub;
			TFPASS(expectChar('s', EV_EMS_CONTROL, "fileSave"));
			TFPASS(expectChar('c', EV_EMS_CONTROL, "querySaveAndExit"));
			TFPASS(expectChar('u', 0, "undo"));
			TFPASS(expectChar('k', 0, "closeWindow"));
			s_pMap = outer;
		}
	}

	/* ---- vi edit mode ---- */
	s_pMap = bs.getMap("viEdit");
	TFPASS(s_pMap != nullptr);
	TFPASS(expectChar('h', 0, "warpInsPtLeft"));
	TFPASS(expectChar('j', 0, "warpInsPtNextLine"));
	TFPASS(expectChar('k', 0, "warpInsPtPrevLine"));
	TFPASS(expectChar('l', 0, "warpInsPtRight"));
	TFPASS(expectChar('w', 0, "warpInsPtEOW"));
	TFPASS(expectChar('b', 0, "warpInsPtBOW"));
	TFPASS(expectChar('0', 0, "warpInsPtBOL"));
	TFPASS(expectChar('$', 0, "warpInsPtEOL"));
	TFPASS(expectChar('G', 0, "warpInsPtEOD"));
	TFPASS(expectChar('/', 0, "find"));

	/* vi operator prefixes chain into their own sub-maps */
	{
		EV_EditBindingMap * outer = s_pMap;
		EV_EditBindingMap * sub;

		sub = expectPrefix('d', 0);                /* 'd' delete operator */
		TFPASS(sub != nullptr);
		if (sub)
		{
			s_pMap = sub;
			TFPASS(expectChar('d', 0, "viCmd_dd"));
			TFPASS(expectChar('w', 0, "viCmd_dw"));
			TFPASS(expectChar('$', 0, "viCmd_d24"));
			s_pMap = outer;
		}

		sub = expectPrefix('y', 0);                /* 'y' yank operator */
		TFPASS(sub != nullptr);
		if (sub)
		{
			s_pMap = sub;
			TFPASS(expectChar('y', 0, "viCmd_yy"));
			TFPASS(expectChar('w', 0, "viCmd_yw"));
			s_pMap = outer;
		}

		sub = expectPrefix(':', 0);                /* ':' colon commands */
		TFPASS(sub != nullptr);
		if (sub)
		{
			s_pMap = sub;
			TFPASS(expectChar('w', 0, "fileSave"));
			TFPASS(expectChar('q', 0, "closeWindow"));
			TFPASS(expectChar('e', 0, "fileOpen"));
			s_pMap = outer;
		}

		sub = expectPrefix('r', 0);                /* 'r' replace-char */
		TFPASS(sub != nullptr);
		if (sub)
		{
			s_pMap = sub;
			TFPASS(expectChar('a', 0, "replaceChar"));
			s_pMap = outer;
		}
	}

	/* ---- vi input mode ---- */
	s_pMap = bs.getMap("viInput");
	TFPASS(s_pMap != nullptr);
	TFPASS(expectNVK(EV_NVK_ESCAPE, 0, "setEditVI")); /* Esc -> edit mode */
	TFPASS(expectNVK(EV_NVK_LEFT, 0, "warpInsPtLeft"));
	TFPASS(expectNVK(EV_NVK_RETURN, 0, "insertParagraphBreak"));

	/* ---- dead-key sub-maps (loaded lazily via getMap) ---- */
	static const char * const deadMaps[] = {
		"deadabovedot", "deadacute", "deadbreve", "deadcaron",
		"deadcedilla", "deadcircumflex", "deaddiaeresis",
		"deaddoubleacute", "deadgrave", "deadmacron", "deadogonek",
		"deadtilde"
	};
	for (size_t i = 0; i < G_N_ELEMENTS(deadMaps); i++)
		TFPASS(bs.getMap(deadMaps[i]) != nullptr);
}
