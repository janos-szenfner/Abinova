/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiWord
 * Copyright (C) 2011 Hub Figuiere
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

#include "tf_test.h"
#include "pf_Fragments.h"
#include "pf_Frag.h"
#include "pt_PieceTable.h"

#define TFSUITE "core.text.ptbl.fragments"

/* document size via the public API: pos(last) + length(last) */
static PT_DocPosition docSize(pf_Fragments & frags)
{
	pf_Frag * last = frags.getLast();
	return last ? last->getPos() + last->getLength() : 0;
}

TFTEST_MAIN("pf_Fragments")
{
	/* ---- empty tree invariants ------------------------------------ */
	{
		pt_PieceTable pt(nullptr);
		TFPASS(pt.getFragments().getFirst() == nullptr);
		TFPASS(pt.getFragments().getLast() == nullptr);
		TFPASS(pt.getFragments().findFirstFragBeforePos(0) == nullptr);
		TFPASS(pt.getFragments().findFirstFragBeforePos(100) == nullptr);
	}

	/* ---- basic append, ordering and positions --------------------- */
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);
		pf_Fragments & frags = pt.getFragments();

		pf_Frag * f1 = new pf_Frag(&pt, pf_Frag::PFT_Text, 5, 0);
		pf_Frag * f2 = new pf_Frag(&pt, pf_Frag::PFT_Text, 3, 0);
		pf_Frag * f3 = new pf_Frag(&pt, pf_Frag::PFT_Text, 7, 0);
		frags.appendFrag(f1);
		frags.appendFrag(f2);
		frags.appendFrag(f3);

		TFPASS(frags.getFirst() == f1);
		TFPASS(frags.getLast() == f3);
		TFPASS(docSize(frags) == 15);
		TFPASS(f1->getPos() == 0);
		TFPASS(f2->getPos() == 5);
		TFPASS(f3->getPos() == 8);
		TFPASS(f1->getNext() == f2);
		TFPASS(f2->getNext() == f3);
		TFPASS(f3->getPrev() == f2);
		TFPASS(f1->getPrev() == nullptr);
		TFPASS(f3->getNext() == nullptr);
	}

	/* ---- zero-length fragments ------------------------------------ */
	/* appendFrag() used to compute find(sizeDocument()-1); with a
	 * tree of only zero-length fragments the document size is zero
	 * and the subtraction underflowed to find(UINT_MAX), yielding an
	 * invalid iterator — a crash or a silent corrupt insertion. */
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);
		pf_Fragments & frags = pt.getFragments();

		pf_Frag * z1 = new pf_Frag(&pt, pf_Frag::PFT_FmtMark, 0, 0);
		pf_Frag * z2 = new pf_Frag(&pt, pf_Frag::PFT_FmtMark, 0, 0);
		pf_Frag * t1 = new pf_Frag(&pt, pf_Frag::PFT_Text, 4, 0);
		frags.appendFrag(z1);
		frags.appendFrag(z2);	/* underflow path: sizeDocument()==0 */
		frags.appendFrag(t1);	/* must land after z2, tree intact */

		TFPASS(frags.getFirst() == z1);
		TFPASS(frags.getLast() == t1);
		TFPASS(z2->getPrev() == z1);
		TFPASS(t1->getPrev() == z2);
		TFPASS(docSize(frags) == 4);
		TFPASS(t1->getPos() == 0);
	}

	/* ---- unlink invalidates the frag's node back-pointer ---------- */
	/* erase() used to leave pf_Frag::m_pMyNode pointing at a node
	 * that was either freed or reassigned to the successor's frag.
	 * getPos() then reported garbage and a second unlinkFrag() could
	 * erase an innocent frag's node — the classic piece-table
	 * corruption vector. */
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);
		pf_Fragments & frags = pt.getFragments();

		pf_Frag * a = new pf_Frag(&pt, pf_Frag::PFT_Text, 2, 0);
		pf_Frag * b = new pf_Frag(&pt, pf_Frag::PFT_Text, 4, 0);
		pf_Frag * c = new pf_Frag(&pt, pf_Frag::PFT_Text, 6, 0);
		frags.appendFrag(a);
		frags.appendFrag(b);
		frags.appendFrag(c);

		frags.unlinkFrag(b);
		TFPASS(b->getPos() == 0);
		TFPASS(b->getNext() == nullptr);
		TFPASS(b->getPrev() == nullptr);

		/* survivors must stay consistent */
		TFPASS(a->getNext() == c);
		TFPASS(c->getPrev() == a);
		TFPASS(frags.getFirst() == a);
		TFPASS(frags.getLast() == c);
		TFPASS(c->getPos() == 2);
		TFPASS(docSize(frags) == 8);

		/* a second unlink of the same frag must be a no-op — before
		 * the fix it could erase c's node */
		frags.unlinkFrag(b);
		TFPASS(frags.getFirst() == a);
		TFPASS(frags.getLast() == c);
		TFPASS(docSize(frags) == 8);
		TFPASS(c->getPos() == 2);
		TFPASS(a->getNext() == c);

		delete b;	/* unlinked frags are caller-owned */
	}

	/* ---- insertFrag / insertFragBefore ---------------------------- */
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);
		pf_Fragments & frags = pt.getFragments();

		pf_Frag * a = new pf_Frag(&pt, pf_Frag::PFT_Text, 4, 0);
		frags.appendFrag(a);

		pf_Frag * b = new pf_Frag(&pt, pf_Frag::PFT_Text, 2, 0);
		frags.insertFragBefore(a, b);
		TFPASS(frags.getFirst() == b);
		TFPASS(b->getPos() == 0);
		TFPASS(a->getPos() == 2);
		TFPASS(b->getNext() == a);

		pf_Frag * c = new pf_Frag(&pt, pf_Frag::PFT_Text, 6, 0);
		frags.insertFrag(a, c);
		TFPASS(c->getPos() == 6);
		TFPASS(frags.getLast() == c);
		TFPASS(docSize(frags) == 12);

		frags.unlinkFrag(a);
		TFPASS(c->getPos() == 2);
		TFPASS(b->getNext() == c);
		TFPASS(c->getPrev() == b);
		TFPASS(docSize(frags) == 8);

		delete a;
	}

	/* ---- unlink first and last of a three-frag tree --------------- */
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);
		pf_Fragments & frags = pt.getFragments();

		pf_Frag * a = new pf_Frag(&pt, pf_Frag::PFT_Text, 1, 0);
		pf_Frag * b = new pf_Frag(&pt, pf_Frag::PFT_Text, 2, 0);
		pf_Frag * c = new pf_Frag(&pt, pf_Frag::PFT_Text, 3, 0);
		frags.appendFrag(a);
		frags.appendFrag(b);
		frags.appendFrag(c);

		frags.unlinkFrag(a);	/* head */
		TFPASS(frags.getFirst() == b);
		TFPASS(b->getPrev() == nullptr);
		TFPASS(b->getPos() == 0);

		frags.unlinkFrag(c);	/* tail */
		TFPASS(frags.getLast() == b);
		TFPASS(b->getNext() == nullptr);
		TFPASS(docSize(frags) == 2);
		TFPASS(frags.getFirst() == b && frags.getLast() == b);

		delete a;
		delete c;
	}

	/* ---- PTS_Editing transition keeps the tree valid -------------- */
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);
		pt.setPieceTableState(PTS_Editing);	/* appends an EOD frag */
		pf_Frag * eod = pt.getFragments().getFirst();
		TFPASS(eod != nullptr);
		TFPASS(eod->getType() == pf_Frag::PFT_EndOfDoc);
		/* getLast() returns the last *content* frag — an EOD-only
		 * document has none */
		TFPASS(pt.getFragments().getLast() == nullptr);
		TFPASS(docSize(pt.getFragments()) == 0);
	}
}
