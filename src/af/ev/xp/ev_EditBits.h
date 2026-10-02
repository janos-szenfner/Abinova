/* AbiSource Program Utilities
 * Copyright (C) 1998 AbiSource, Inc.
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




#ifndef EV_EDITBITS_H
#define EV_EDITBITS_H

#include "ut_types.h"

/*****************************************************************
******************************************************************
** we compress virtual key code and mouse ops along with modifier
** keys into a single long word in order to facilitate binding lookups.
******************************************************************
*****************************************************************/


typedef UT_uint32 EV_EditMouseContext;								/* may not be ORed */
// NB: when adding new values, you have to also increase EV_COUNT_EMC
// defined at the bottom of this file -- if you dont you will spend
// hours trying to work out why your functions do not get called, as
// I did :) (Tomas)
//
// Foddex:	the values for the various contexts were bad. The EV_EMC_ToNumber
// 			define is used to make an index out of the context bits. It is
//			used assuming that it will never yield a value larger than or
//			equal to EV_COUNT_EMC. However, the original values were defined
//			in such a way that it often was bigger dan EV_COUNT_EMC. E.g.
//			0xe0000000 >> 27 = 28, which is bigger than EV_COUNT_EMC. I therefor
//			rearranged the values. I just hope noone ever was so stupid to use
//			hardcoded values...
#define EV_EMC__MASK__			(static_cast<EV_EditMouseContext>( 0xf8000000))
#define EV_EMC_UNKNOWN			(static_cast<EV_EditMouseContext>( 0x08000000))
#define EV_EMC_TEXT				(static_cast<EV_EditMouseContext>( 0x10000000))
#define EV_EMC_LEFTOFTEXT		(static_cast<EV_EditMouseContext>( 0x18000000))
#define EV_EMC_MISSPELLEDTEXT	(static_cast<EV_EditMouseContext>( 0x20000000))
#define EV_EMC_IMAGE			(static_cast<EV_EditMouseContext>( 0x28000000))
#define EV_EMC_IMAGESIZE		(static_cast<EV_EditMouseContext>( 0x30000000))
#define EV_EMC_FIELD			(static_cast<EV_EditMouseContext>( 0x38000000))
#define EV_EMC_HYPERLINK		(static_cast<EV_EditMouseContext>( 0x40000000))
#define EV_EMC_RIGHTOFTEXT		(static_cast<EV_EditMouseContext>( 0x48000000))
#define EV_EMC_REVISION		    (static_cast<EV_EditMouseContext>( 0x50000000))
#define EV_EMC_VLINE            (static_cast<EV_EditMouseContext>( 0x58000000))
#define EV_EMC_HLINE            (static_cast<EV_EditMouseContext>( 0x60000000))
#define EV_EMC_FRAME            (static_cast<EV_EditMouseContext>( 0x68000000))
#define EV_EMC_VISUALTEXTDRAG   (static_cast<EV_EditMouseContext>( 0x70000000))
#define EV_EMC_TOPCELL          (static_cast<EV_EditMouseContext>( 0x78000000))
#define EV_EMC_TOC              (static_cast<EV_EditMouseContext>( 0x80000000))
#define EV_EMC_POSOBJECT        (static_cast<EV_EditMouseContext>( 0x88000000))
#define EV_EMC_MATH             (static_cast<EV_EditMouseContext>( 0x90000000))
#define EV_EMC_EMBED            (static_cast<EV_EditMouseContext>( 0x98000000))
#define EV_EMC_TABLEDRAW        (static_cast<EV_EditMouseContext>( 0xa0000000))
#define EV_EMC_TABLEERASE       (static_cast<EV_EditMouseContext>( 0xa8000000))
#define EV_EMC_TABLEPAINT       (static_cast<EV_EditMouseContext>( 0xb0000000))
#define EV_EMC_TABLESAMPLE      (static_cast<EV_EditMouseContext>( 0xb8000000))
#define EV_EMC_TABLE            (static_cast<EV_EditMouseContext>( 0xc0000000))

// NB: the following two values are not included in EV_COUNT_EMC
// because they are not used in the bindings, and are, therefore,
// never processed by bet the mouse routines, consequently they
// do not have to follow the rules for EV_EMC ( and they dont so that
// they would not waste available values, which we will soon run
// out off); they are here so that contextHyperlink can create
// different menus in dependence on the nature of the text of
// the hyperlink

#define EV_EMC_HYPERLINKTEXT      (static_cast<EV_EditMouseContext>( 0x000000002))
#define EV_EMC_HYPERLINKMISSPELLED (static_cast<EV_EditMouseContext>( 0x000000001))
// RIVERA
#define EV_EMC_ANNOTATIONTEXT      (static_cast<EV_EditMouseContext>( 0x000000003))
#define EV_EMC_ANNOTATIONMISSPELLED (static_cast<EV_EditMouseContext>( 0x000000004))
#define EV_EMC_RDFANCHORTEXT      (static_cast<EV_EditMouseContext>( 0x000000005))
// dynamic values will be generated starting from EV_EMC_AVAIL, should
// be changed if needed.
#define EV_EMC_AVAIL			  (static_cast<EV_EditMouseContext>( 0x000000007))

#define EV_EMC_ToNumber(emc)			((((emc)&EV_EMC__MASK__)>>27)-1)
#define EV_EMC_FromNumber(n)			(((n+1)<<27)&EV_EMC__MASK__)

typedef UT_uint32 EV_EditModifierState;								/* may be ORed */
#define EV_EMS__MASK__			(static_cast<EV_EditModifierState>( 0x07000000))
#define EV_EMS_SHIFT			(static_cast<EV_EditModifierState>( 0x01000000))
#define EV_EMS_CONTROL			(static_cast<EV_EditModifierState>( 0x02000000))
#define EV_EMS_ALT				(static_cast<EV_EditModifierState>( 0x04000000))
#define EV_EMS_ToNumber(ems)			(((ems)&EV_EMS__MASK__)>>24)
#define EV_EMS_ToNumberNoShift(ems)		(((ems)&EV_EMS__MASK__)>>25)	/* exclude shift */
#define EV_EMS_FromNumber(n)			((((n)<<24)&EV_EMS__MASK__))
#define EV_EMS_FromNumberNoShift(n)		((((n)<<25)&EV_EMS__MASK__))


typedef UT_uint32 EV_EditKeyPress;									/* may be ORed */
#define EV_EKP__MASK__			(static_cast<EV_EditKeyPress>(		0x00880000))
#define EV_EKP_PRESS			(static_cast<EV_EditKeyPress>(		0x00800000))
#define EV_EKP_NAMEDKEY			(static_cast<EV_EditKeyPress>(		0x00080000))
#define EV_NamedKey(xxxx)	(EV_EKP_NAMEDKEY | (xxxx))


typedef UT_uint32 EV_EditMouseButton;								/* may not be ORed */
#define EV_EMB__MASK__			(static_cast<EV_EditMouseButton>(	0x00700000))
#define EV_EMB_BUTTON0			(static_cast<EV_EditMouseButton>(	0x00100000))	/* no buttons down */
#define EV_EMB_BUTTON1			(static_cast<EV_EditMouseButton>(	0x00200000))
#define EV_EMB_BUTTON2			(static_cast<EV_EditMouseButton>(	0x00300000))
#define EV_EMB_BUTTON3			(static_cast<EV_EditMouseButton>(	0x00400000))
#define EV_EMB_BUTTON4			(static_cast<EV_EditMouseButton>(	0x00500000))
#define EV_EMB_BUTTON5			(static_cast<EV_EditMouseButton>(	0x00600000))
#define EV_EMB_ToNumber(emb)	(((emb)&EV_EMB__MASK__)>>20)


typedef UT_uint32 EV_EditMouseOp;									/* may not be ORed */
#define EV_EMO__MASK__			(static_cast<EV_EditMouseOp>(		0x00070000))
#define EV_EMO_SINGLECLICK		(static_cast<EV_EditMouseOp>(		0x00010000))
#define EV_EMO_DOUBLECLICK		(static_cast<EV_EditMouseOp>(		0x00020000))
#define EV_EMO_DRAG				(static_cast<EV_EditMouseOp>(		0x00030000))	/* drag */
#define EV_EMO_DOUBLEDRAG		(static_cast<EV_EditMouseOp>(		0x00040000))	/* drag following doubleclick */
#define EV_EMO_RELEASE			(static_cast<EV_EditMouseOp>(		0x00050000))	/* release following singleclick */
#define EV_EMO_DOUBLERELEASE	(static_cast<EV_EditMouseOp>(		0x00060000))	/* release following doubleclick */
#define EV_EMO_ToNumber(emb)	(((emb)&EV_EMO__MASK__)>>16)
#define EV_EMO_FromNumber(n)	((((n)<<16)&EV_EMO__MASK__))


typedef UT_uint32 EV_EditVirtualKey;								/* may not be ORed */
#define EV_EVK__MASK__			(static_cast<EV_EditVirtualKey>(	0x0000ffff))
#define EV_EVK_ToNumber(evk)	(((evk)&EV_EVK__MASK__))
#define EV_NVK_ToNumber(nvk)	(((nvk)&EV_EVK__MASK__))


typedef UT_uint32 EV_EditBits;	/* union of all the above bits */

// the EV_COUNT_ give the number of unique combinations
// currently defined in the bits above.

#define EV_COUNT_EMS			8		// combinations under 'OR' (including 0)
#define EV_COUNT_EMS_NoShift	(EV_COUNT_EMS/2)

#define EV_COUNT_EMB			6		// simple count (not 'OR')
#define EV_COUNT_EMO			6		// simple count (not 'OR')
#define EV_COUNT_EMC			24		// simple count (not 'OR')

#define EV_IsMouse(eb)			(((eb) & EV_EMO__MASK__))
#define EV_IsKeyboard(eb)		(((eb) & EV_EKP__MASK__))

#endif /* EV_EDITBITS_H */
