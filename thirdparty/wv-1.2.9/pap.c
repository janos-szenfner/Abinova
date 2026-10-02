/* wvWare
 * Copyright (C) Caolan McNamara, Dom Lachowicz, and others
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "wv.h"

/*
To apply a UPX.papx to a UPE.pap, set UPE.pap.istd equal to UPX.papx.istd, and
then apply the UPX.papx.grpprl to UPE.pap.
*/
void
wvAddPAPXFromBucket (PAP * apap, UPXF * upxf, STSH * stsh, wvStream * data)
{
    U8 *pointer;
    U16 i = 0;
    U16 sprm;
    apap->istd = upxf->upx.papx.istd;
    if (upxf->cbUPX <= 2)
	return;
    wvTrace (("no is %d\n", upxf->cbUPX));
#ifdef SPRMTEST
    fprintf (stderr, "\n");
    while (i < upxf->cbUPX - 2)
      {
	  fprintf (stderr, "%x (%d) ", *(upxf->upx.papx.grpprl + i),
		   *(upxf->upx.papx.grpprl + i));
	  i++;
      }
    fprintf (stderr, "\n");
    i = 0;
#endif
    /*
       while (i < upxf->cbUPX-2)
     */
    while (i < upxf->cbUPX - 4)	/* the end of the list is at -2, but there has to be a full sprm of
				   len 2 as well */
      {
	  U16 scratch;
	  int oplen;
	  sprm = bread_16ubit (upxf->upx.papx.grpprl + i, &i);
#ifdef SPRMTEST
	  wvError (("sprm is %x\n", sprm));
#endif
	  pointer = upxf->upx.papx.grpprl + i;
	  if (i < upxf->cbUPX - 2)
	    {
		/* reject operands that would run past the end of the
		   grpprl; the handler would read them out of bounds */
		scratch = i;
		oplen = wvEatSprm (sprm, pointer,
				   upxf->upx.papx.grpprl + upxf->cbUPX,
				   &scratch);
		if ((U32) i + (U32) oplen > (U32) upxf->cbUPX)
		    break;
		wvApplySprmFromBucket (WORD8, sprm, apap, NULL, NULL, stsh,
				       pointer, &i, data);
	    }
      }
}

void
wvAddPAPXFromBucket6 (PAP * apap, UPXF * upxf, STSH * stsh)
{
    U8 *pointer;
    U16 i = 0;
    U16 sprm;
    U8 sprm8;
    apap->istd = upxf->upx.papx.istd;
    if (upxf->cbUPX <= 2)
	return;
    wvTrace (("no is %d\n", upxf->cbUPX));

#ifdef SPRMTEST
    fprintf (stderr, "\n");
    while (i < upxf->cbUPX - 2)
      {
	  fprintf (stderr, "%x (%d) ", *(upxf->upx.papx.grpprl + i),
		   *(upxf->upx.papx.grpprl + i));
	  i++;
      }
    fprintf (stderr, "\n");
    i = 0;
#endif

    while (i < upxf->cbUPX - 3)	/* the end of the list is at -2, but there has to be a full sprm of
				   len 1 as well */
      {
	  sprm8 = bread_8ubit (upxf->upx.papx.grpprl + i, &i);
#ifdef SPRMTEST
	  wvError (("pap word 6 sprm is %x (%d)\n", sprm8, sprm8));
#endif
	  sprm = (U16) wvGetrgsprmWord6 (sprm8);
#ifdef SPRMTEST
	  wvError (("pap word 6 sprm is converted to %x\n", sprm));
#endif
	  pointer = upxf->upx.papx.grpprl + i;
	  /* hmm, maybe im wrong here, but there appears to be corrupt
	   * word 6 sprm lists being stored in the file
	   */
	  if (i < upxf->cbUPX - 2)
	    {
		U16 scratch;
		int oplen;
		scratch = i;
		oplen = wvEatSprm (sprm, pointer,
				   upxf->upx.papx.grpprl + upxf->cbUPX,
				   &scratch);
		if ((U32) i + (U32) oplen > (U32) upxf->cbUPX)
		    break;
		wvApplySprmFromBucket (WORD6, sprm, apap, NULL, NULL, stsh,
				       pointer, &i, NULL);
	    }
      }
}


void
wvInitPAPFromIstd (PAP * apap, U16 istdBase, STSH * stsh)
{
    if (istdBase == istdNil)
	wvInitPAP (apap);
    else
      {
	  if (istdBase >= stsh->Stshi.cstd || stsh->std == NULL)
	    {
		wvError (
			 ("ISTD out of bounds, requested %d of %d\n",
			  istdBase, stsh->Stshi.cstd));
		wvInitPAP (apap);	/*it can't hurt to try and start with a blank istd */
		return;
	    }
	  else
	    {
		if (stsh->std[istdBase].cupx == 0
		    || stsh->std[istdBase].grupe == NULL)	/*empty slot in the array, i don't think this should happen */
		  {
		      wvTrace (("Empty style slot used (chp)\n"));
		      wvInitPAP (apap);
		  }
		else
		  {
		    wvCopyPAP (apap, &(stsh->std[istdBase].grupe[0].apap));
		    strncpy(apap->stylename,stsh->std[istdBase].xstzName, sizeof(apap->stylename) - 1);
		    apap->stylename[sizeof(apap->stylename) - 1] = 0;
		  }
	    }
      }
}

void
wvCopyPAP (PAP * dest, PAP * src)
{
    memcpy (dest, src, sizeof (PAP));
}


void
wvInitPAP (PAP * item)
{
    int i;
    item->istd = 0;
    item->jc = 0;
    item->fKeep = 0;
    item->fKeepFollow = 0;
    item->fPageBreakBefore = 0;
    item->fBrLnAbove = 0;
    item->fBrLnBelow = 0;
    item->fUnused = 0;
    item->pcVert = 0;
    item->pcHorz = 0;
    item->brcp = 0;
    item->brcl = 0;
    item->reserved1 = 0;
    item->ilvl = 0;
    item->fNoLnn = 0;
    item->ilfo = 0;
    item->nLvlAnm = 0;
    item->reserved2 = 0;
    item->fSideBySide = 0;
    item->reserved3 = 0;
    item->fNoAutoHyph = 0;
    item->fWidowControl = 1;
    item->dxaRight = 0;
    item->dxaLeft = 0;
    item->dxaLeft1 = 0;
    /*
       wvInitLSPD(&item->lspd);
     */
    item->lspd.fMultLinespace = 1;
    item->lspd.dyaLine = 240;

    item->dyaBefore = 0;
    item->dyaAfter = 0;

    wvInitPHE (&item->phe, 0);

    item->fCrLf = 0;
    item->fUsePgsuSettings = 0;
    item->fAdjustRight = 0;
    item->reserved4 = 0;
    /* MS-DOC: these paragraph toggles default to 1 (enabled); init to
       the spec defaults so a sprm that explicitly disables them is
       distinguishable from an absent sprm */
    item->fKinsoku = 1;
    item->fWordWrap = 1;
    item->fOverflowPunct = 1;
    item->fTopLinePunct = 0;
    item->fAutoSpaceDE = 1;
    item->fAtuoSpaceDN = 1;
    item->wAlignFont = 4;
    item->fVertical = 0;
    item->fBackward = 0;
    item->fRotateFont = 0;
    item->reserved5 = 0;
    item->reserved6 = 0;
    item->fInTable = 0;
    item->fTtp = 0;
    item->wr = 0;
    item->fLocked = 0;

    wvInitTAP (&item->ptap);

    item->dxaAbs = 0;
    item->dyaAbs = 0;
    item->dxaWidth = 0;

    wvInitBRC (&item->brcTop);
    wvInitBRC (&item->brcLeft);
    wvInitBRC (&item->brcBottom);
    wvInitBRC (&item->brcRight);
    wvInitBRC (&item->brcBetween);
    wvInitBRC (&item->brcBar);

    item->dxaFromText = 0;
    item->dyaFromText = 0;
    item->dyaHeight = 0;
    item->fMinHeight = 0;

    wvInitSHD (&item->shd);
    wvInitDCS (&item->dcs);
    item->lvl = 9;
    item->fNumRMIns = 0;
    wvInitANLD (&item->anld);
    item->fPropRMark = 0;
    item->ibstPropRMark = 0;
    wvInitDTTM (&item->dttmPropRMark);
    wvInitNUMRM (&item->numrm);
    item->itbdMac = 0;
    for (i = 0; i < itbdMax; i++)
	item->rgdxaTab[i] = 0;
    for (i = 0; i < itbdMax; i++)
	wvInitTBD (&item->rgtbd[i]);

    item->fBidi = 0;
	item->stylename[0] = 0;

	memset(&item->linfo,0,sizeof(item->linfo));

    item->itap = 0;
    item->ipgp = 0;
    item->fInnerTableCell = 0;
    item->fInnerTtp = 0;
    item->fOpenTch = 0;
    item->fDyaBeforeAuto = 0;
    item->fDyaAfterAuto = 0;
    item->fNoAllowOverlap = 0;
    item->fContextualSpacing = 0;
    item->fMirrorIndents = 0;
    item->fWall = 0;
    item->tTwo = 0;
    item->dxcRight = 0;
    item->dxcLeft = 0;
    item->dxcLeft1 = 0;
    item->dylBefore = 0;
    item->dylAfter = 0;
}

/*
1) Having found the index i of the FC in an FKP that marks the character stored
in the file immediately after the paragraph's paragraph mark,

1 is done in Simple mode through wvGetSimpleParaBounds which places this index
in fcLim by default

2) it is necessary to use the word offset stored in the first byte of the
fkp.rgbx[i - 1] to find the PAPX for the paragraph.

3) Using papx.istd to index into the properties stored for the style sheet ,

4) the paragraph properties of the style are copied to a local PAP.

5) Then the grpprl stored in the PAPX is applied to the local PAP,

6) and papx.istd along with fkp.rgbx.phe are moved into the local PAP.

7) The process thus far has created a PAP that describes what the paragraph properties
of the paragraph were at the last full save.
*/

int
wvAssembleSimplePAP (wvVersion ver, PAP * apap, U32 fc, PAPX_FKP * fkp, wvParseStruct * ps)
{
    PAPX *papx;
    int index;
    UPXF upxf;
    int ret = 0;

    /*index is the i in the text above */
    index = wvGetIndexFCInFKP_PAPX (fkp, fc);

    wvTrace (("index is %d, using %d\n", index, index - 1));
    /* AbiWord: grppapx has crun entries -- an unreadable or crun==0 FKP
       must not be indexed */
    papx = (index > 0 && index <= fkp->crun && fkp->grppapx)
	? &(fkp->grppapx[index - 1]) : NULL;

    if (papx)
      {
	  wvTrace (("istd index is %d\n", papx->istd));
	  wvInitPAPFromIstd (apap, papx->istd, &ps->stsh);
      }
    else
	wvInitPAPFromIstd (apap, istdNil, &ps->stsh);

    if ((papx) && (papx->cb > 2))
      {
	  ret = 1;
#ifdef SPRMTEST
	  fprintf (stderr, "cbUPX is %d\n", papx->cb);
	  for (i = 0; i < papx->cb - 2; i++)
	      fprintf (stderr, "%x ", papx->grpprl[i]);
	  fprintf (stderr, "\n");
#endif
	  upxf.cbUPX = papx->cb;
	  upxf.upx.papx.istd = papx->istd;
	  upxf.upx.papx.grpprl = papx->grpprl;
	  if (ver == WORD8)
	      wvAddPAPXFromBucket (apap, &upxf, &ps->stsh, ps->data);
	  else
	      wvAddPAPXFromBucket6 (apap, &upxf, &ps->stsh);
      }

    if (papx)
	apap->istd = papx->istd;

    if (fkp->rgbx != NULL && index > 0 && index <= fkp->crun)
      wvCopyPHE (&apap->phe, &(fkp->rgbx[index - 1].phe), apap->fTtp);

	/*
	  By now we have assembled the paragraph properties based on the
	  info in the style associated with this pap and also in any of the
	  PAPX overrides; next step is to see if this paragraph is a part of
	  a list, and if so, to apply any list-specific overrides

	  The MS documentation on lists really sucks, but we've been able to decipher
	  some meaning from it and get simple lists to sorta work. This code mostly prints out
	  debug messages with useful information in them, but it will also append a list
	  and add a given paragraph to a given list
	*/

	if (!apap->ilfo)
		return ret;

	/* This is really silly, but it would seem that if there are both
	PAPX for the paragraph and the list, the paragraph ones take
	priority (basically, when a list is applied to a custom indented
	block, the block's indents become part of the list PAPX; if the
	indents of the block are subsequently modified, the PAPX of the
	list stays the same, and the PAPX of the block changes); this
	means that we now have to apply the list PAPX over what we have
	and then reapply the block PAPX (we had to apply the block's PAPX
	in order to find out if we are in a list !!!)*/

	if (!ps->lfo)
	  return ret;

	if (wvAssembleListPAP (ver, apap, ps, papx))
	    ret = 1;

	return (ret);
}

/*
  Resolve a paragraph's list formatting per MS-DOC "Determining List
  Numbering of a Paragraph": pap.ilfo indexes the PlfLfo (one-based);
  the LFO names the LST through lsid; an LFOLVL record can override
  the level's start-at value (fStartAt) and/or its whole LVL
  (fFormatting -- stored next to the LFOLVL inside the PlfLfo, parsed
  into ps->lvl).  The chosen level's grpprlPapx is applied to the
  paragraph underneath papx (the paragraph's own papx, reapplied so it
  wins; may be NULL), the number's grpprlChpx is folded into
  apap->linfo.chp, and a style linked through LSTF.rgistd replaces the
  paragraph style.

  Called from wvAssembleSimplePAP, and again from the complex decoder
  when a CLX grpprl changed ilfo/ilvl after the simple pass.
*/
int
wvAssembleListPAP (wvVersion ver, PAP * apap, wvParseStruct * ps,
		   PAPX * papx)
{
    U32 myListId = 0;
    LVLF * myLVLF = NULL;
    LVL * myLVL = NULL;
    LVL * lstLVL = NULL;
    LFO * myLFO = NULL;
    LST * myLST = NULL;
    LFOLVL * myLFOLVL = NULL;
    U32 iLFOLVL = 0;

    S32 myStartAt = -1;
    U8 * mygPAPX = NULL;
    U8 * mygCHPX = NULL;
    XCHAR * myNumberStr = NULL;
    S32 myNumberStr_count = 0;
    U32 mygPAPX_count = 0, mygCHPX_count = 0;

    PAPX myPAPX;
    UPXF upxf;

    S32 i = 0, j = 0, k = 0;

    int ret = 0;

    memset (&apap->linfo, 0, sizeof (apap->linfo));

    if (!ps->lfo || !ps->nolfo)
	return (0);

    wvTrace (("list: ilvl %d, ilfo %d\n", apap->ilvl, apap->ilfo));

    /* ilfo indexes ps->lfo one-based; MS-DOC reserves 0 (and 0x7FF for
       "not part of a list"), and a corrupt sprmPIlfo must not send us
       out of bounds */
    if (apap->ilfo < 0 || apap->ilfo == 2047)
      {
	  apap->ilfo = 0;
	  return (0);
      }
    if (apap->ilfo > (S32) ps->nolfo)
      {
	  wvWarning ("ilfo %d exceeds PlfLfo count %d, dropping list\n",
		      apap->ilfo, ps->nolfo);
	  return (0);
      }

    /* ilvl selects one of a list's nine levels; clamp garbage */
    if (apap->ilvl > 8)
      {
	  wvWarning ("list level %d out of range, clamping to 8\n",
		      apap->ilvl);
	  apap->ilvl = 8;
      }

    myLFO = &ps->lfo[apap->ilfo - 1];

    /* find this LFO's first LFOLVL: they are stored contiguously, in
       LFO order */
    while (i < (S32) apap->ilfo - 1 && i < (S32) ps->nolfo)
      {
	  j += ps->lfo[i].clfolvl;
	  i++;
      }

    /* remember how many overrides there are for this record */
    k = ps->lfo[i].clfolvl;

    /* if there are any overrides, see whether one applies to this level */
    if (k && ps->lfolvl)
      {
	  S32 m;
	  for (m = 0; m < k && j + m < (S32) ps->nooflvl; m++)
	    {
		if (ps->lfolvl[j + m].ilvl == apap->ilvl)
		  {
		      myLFOLVL = &ps->lfolvl[j + m];
		      iLFOLVL = j + m;
		      wvTrace (("list: lfolvl: iStartAt %d, fStartAt %d, "
				"fFormatting %d\n", myLFOLVL->iStartAt,
				myLFOLVL->fStartAt, myLFOLVL->fFormatting));
		      break;
		  }
	    }
	  if (!myLFOLVL)
	      wvTrace (("list: no LFOLVL found for this level\n"));
      }

    /* the LST is located through the LFO's lsid -- not through anything
       inside the LFOLVL */
    myListId = myLFO->lsid;
    if (ps->lst)
      {
	  for (i = 0; (S32) i < ps->noofLST; i++)
	    {
		if (ps->lst[i].lstf.lsid == myListId)
		  {
		      myLST = &ps->lst[i];
		      break;
		  }
	    }
      }
    if (!myLST)
	wvTrace (("error: could not locate LST entry\n"));

    wvTrace (("is a simple list? %d - requested level %d\n",
	      myLST ? myLST->lstf.fSimpleList : -1, apap->ilvl));
    if (myLST)
	lstLVL = myLST->lstf.fSimpleList ? myLST->lvl
				       : &myLST->lvl[apap->ilvl];

    /* an fFormatting LFOLVL completely replaces the LST's LVL for this
       level (MS-DOC) -- the overridden LVL was parsed into ps->lvl
       parallel to ps->lfolvl */
    if (myLFOLVL && myLFOLVL->fFormatting && ps->lvl)
      {
	  wvTrace (("list: using the LVL override from the LFO\n"));
	  myLVL = &ps->lvl[iLFOLVL];
      }
    else
	myLVL = lstLVL;

    if (!myLVL)
      {
	  wvWarning ("no LVL available for list %d level %d\n",
		      myListId, apap->ilvl);
	  return (0);
      }

    myLVLF = &myLVL->lvlf;

    if (myLFOLVL && myLFOLVL->fStartAt)
	myStartAt = (S32) myLFOLVL->iStartAt;
    else if (lstLVL)
	myStartAt = (S32) lstLVL->lvlf.iStartAt;
    else
	myStartAt = (S32) myLVLF->iStartAt;

    mygPAPX = myLVL->grpprlPapx;
    mygPAPX_count = myLVLF->cbGrpprlPapx;
    mygCHPX = myLVL->grpprlChpx;
    mygCHPX_count = myLVLF->cbGrpprlChpx;
    if (myLVL->numbertext)
      {
	  myNumberStr = myLVL->numbertext + 1;
	  myNumberStr_count = *(myLVL->numbertext);
      }

    wvTrace (("list: id %d, iStartAt %d, nfc %d, align %d, "
	      "ixchFollow %d, numbertext len %d, papx len %d, "
	      "chpx len %d\n", myListId, myStartAt, myLVLF->nfc,
	      myLVLF->jc, myLVLF->ixchFollow, myNumberStr_count,
	      mygPAPX_count, mygCHPX_count));

    apap->linfo.id = myListId;
    apap->linfo.start = myStartAt;
    apap->linfo.numberstr = myNumberStr;
    apap->linfo.numberstr_size = myNumberStr_count;
    apap->linfo.format = myLVLF->nfc;
    apap->linfo.align = myLVLF->jc;
    apap->linfo.ixchFollow = myLVLF->ixchFollow;

    /* apply the level's grpprlPapx to the paragraph */
    myPAPX.cb = mygPAPX_count;
    myPAPX.grpprl = mygPAPX;
    myPAPX.istd = apap->istd;

    if (myPAPX.cb > 2)
      {
	  ret = 1;
	  upxf.cbUPX = myPAPX.cb;
	  upxf.upx.papx.istd = myPAPX.istd;
	  upxf.upx.papx.grpprl = myPAPX.grpprl;
	  if (ver == WORD8)
	      wvAddPAPXFromBucket (apap, &upxf, &ps->stsh, ps->data);
	  else
	      wvAddPAPXFromBucket6 (apap, &upxf, &ps->stsh);

	  /* now we have to reapply the original PAPX, see note at top
	     of the list code */
	  if ((papx) && (papx->cb > 2))
	    {
		ret = 1;
		upxf.cbUPX = papx->cb;
		upxf.upx.papx.istd = papx->istd;
		upxf.upx.papx.grpprl = papx->grpprl;
		if (ver == WORD8)
		    wvAddPAPXFromBucket (apap, &upxf, &ps->stsh, ps->data);
		else
		    wvAddPAPXFromBucket6 (apap, &upxf, &ps->stsh);
	    }
      }

    /* a level can be linked to a paragraph style through the LSTF's
       rgistd (MS-DOC: the style applies to both the paragraph and the
       number text); 0x0FFF means "not linked" */
    if (myLST && myLST->lstf.rgistd[apap->ilvl] != istdNil &&
	myLST->lstf.rgistd[apap->ilvl] < ps->stsh.Stshi.cstd)
      {
	  U16 istdLink = myLST->lstf.rgistd[apap->ilvl];
	  wvTrace (("list: level %d linked to istd %d\n", apap->ilvl,
		    istdLink));
	  apap->istd = istdLink;
	  if (ps->stsh.std && ps->stsh.std[istdLink].xstzName)
	    {
		strncpy (apap->stylename,
			 ps->stsh.std[istdLink].xstzName,
			 sizeof (apap->stylename) - 1);
		apap->stylename[sizeof (apap->stylename) - 1] = 0;
	    }
      }
    else if (myPAPX.istd != istdNil)
	apap->istd = myPAPX.istd;

    /* the number text takes the paragraph's character properties plus
       the level's grpprlChpx (MS-DOC); always assemble it so the
       importer can pick up the number font even with no chpx */
    wvAssembleSimpleCHP (ver, &apap->linfo.chp, apap, 0, NULL, &ps->stsh);
    if (mygCHPX_count)
      {
	  ret = 1;
	  upxf.cbUPX = mygCHPX_count;
	  upxf.upx.chpx.grpprl = mygCHPX;
	  if (ver == WORD8)
	      wvAddCHPXFromBucket (&apap->linfo.chp, &upxf, &ps->stsh);
	  else
	      wvAddCHPXFromBucket6 (&apap->linfo.chp, &upxf, &ps->stsh);
      }

    return (ret);
}

void
wvReleasePAPX (PAPX * item)
{
    item->cb = 0;
    item->istd = 0;
    wvFree (item->grpprl);
    item->grpprl = NULL;
}

void
wvInitPAPX (PAPX * item)
{
    item->cb = 0;
    item->istd = 0;
    item->grpprl = NULL;
}

void
wvGetPAPX (wvVersion ver, PAPX * item, U8 * page, U16 * pos)
{
    U16 cw;
    /* AbiWord: page is a 512-byte FKP sector -- all reads must stay inside */
    if (*pos >= WV_PAGESIZE)
      {
	  item->cb = 0;
	  item->istd = 0;
	  item->grpprl = NULL;
	  return;
      }
    cw = bread_8ubit (&(page[*pos]), pos);
    if ((cw == 0) && (ver == WORD8))	/* only do this for word 97 */
      {
	  wvTrace (("cw was pad %d\n", cw));
	  if (*pos >= WV_PAGESIZE)
	    {
		item->cb = 0;
		item->istd = 0;
		item->grpprl = NULL;
		return;
	    }
	  cw = bread_8ubit (&(page[*pos]), pos);
	  wvTrace (("cw was %d\n", cw));
      }
    item->cb = cw * 2;
    if (*pos + 2 > WV_PAGESIZE)
      {
	  item->istd = 0;
	  item->grpprl = NULL;
	  return;
      }
    item->istd = bread_16ubit (&(page[*pos]), pos);
    wvTrace (("papx istd is %x\n", item->istd));
    wvTrace (("no of bytes is %d\n", item->cb));
    /* clamp cb to the bytes actually remaining in the page */
    if (*pos + item->cb > WV_PAGESIZE)
	item->cb = (U16) (WV_PAGESIZE - *pos);
    if (item->cb > 2)
      {
	  item->grpprl = (U8 *) wvMalloc (item->cb - 2);
	  memcpy (item->grpprl, &(page[*pos]), (item->cb) - 2);
      }
    else
	item->grpprl = NULL;
}


int
isPAPConform (PAP * current, PAP * previous)
{
    if ((current) && (previous))
	if (wvEqualBRC (&current->brcLeft, &previous->brcLeft))
	    if (wvEqualBRC (&current->brcRight, &previous->brcRight))
		if (current->dxaWidth == previous->dxaWidth)
		    if (current->fInTable == previous->fInTable)
			return (1);
    return (0);
}




void
wvCopyConformPAP (PAP * dest, PAP * src)
{
    if (src)
      {
#ifdef PURIFY
	  wvInitPAP (dest);
#endif
	  dest->brcLeft = src->brcLeft;
	  dest->brcRight = src->brcRight;
	  dest->dxaWidth = src->dxaWidth;
	  dest->fInTable = src->fInTable;
      }
    else
	wvInitPAP (dest);
}
