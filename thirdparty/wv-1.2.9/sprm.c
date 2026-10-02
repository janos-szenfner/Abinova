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

#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "wv.h"

/*
void wvToggle(int ret,CHP *in,STSH *stsh,U8 toggle,type)

When the parameter of the sprm is set to 0 or 1, then
the CHP property is set to the parameter value.

*/

/*
When the parameter of the sprm is 128, then the CHP property is set to the
value that is stored for the property in the style sheet. CHP When the
parameter of the sprm is 129, the CHP property is set to the negation of the
value that is stored for the property in the style sheet CHP.
sprmCFBold through sprmCFVanish are stored only in grpprls linked to piece table
entries.


*/

/*
an argument might be made that instead of in being returned or negated that
it should be the looked up in the original chp through the istd that should
be used, in which case this should be a macro that does the right thing.
but im uncertain as to which is the correct one to do, ideas on a postcard
to... etc etc

This argument which i left as a comment to the original function has been
bourne out in practice, so i converted this to a macro and did a lookup
on the original unmodified chp in the stylesheet to check against

Interestingly enough, even though the spec says that these are only used
in piece table grpprls this is untrue, examples/doc-that-needs-utf8.doc
has them in the stylesheet definition portion, which is a serious problem
as the style that must be checked is not generated before this modifier
comes along, a real nuisance.
*/

#define wvTOGGLE(ret,in,stsh,toggle,type) \
	{ \
	CHP ctemp; \
	if ((toggle == 0) || (toggle == 1))  \
		ret = toggle; \
	else \
		{ \
		\
		wvInitCHPFromIstd(&ctemp,in->istd,stsh); \
	\
		if (toggle == 128) \
			ret = ctemp.type; \
		else if (toggle == 129) \
			ret = !ctemp.type; \
		else \
			wvWarning("Strangle sprm toggle value, ignoring\n"); \
		} \
	}


/*
 spra value operand size
 0          1 byte (operand affects 1 bit)
 1          1 byte
 2          2 bytes
 3          4 bytes
 4          2 bytes
 5          2 bytes
 6          variable length -- following byte is size of operand
 7          3 bytes
*/
int
wvSprmLen (int spra)
{
    switch (spra)
      {
      case 0:
      case 1:
	  return (1);
      case 2:
      case 4:
      case 5:
	  return (2);
      case 7:
	  return (3);
      case 3:
	  return (4);
      case 6:
	  return (-1);
	  /*variable length -- following byte is size of operand */
      default:
	  wvError (("Incorrect spra value %d\n", spra));
      }
    return (-2);
}

void
wvInitSprm (Sprm * aSprm)
{
    aSprm->ispmd = 0;
    aSprm->fSpec = 0;
    aSprm->sgc = 0;
    aSprm->spra = 0;
}

void
wvGetSprmFromU16 (Sprm * aSprm, U16 sprm)
{
#ifdef PURIFY
    wvInitSprm (aSprm);
#endif
    aSprm->ispmd = sprm & 0x01ff;
    aSprm->fSpec = (sprm & 0x0200) >> 9;
    aSprm->sgc = (sprm & 0x1c00) >> 10;
    aSprm->spra = (sprm & 0xe000) >> 13;
}

#undef EXAMINE_SPRM
int
wvEatSprm (U16 sprm, U8 * pointer, const U8 * end, U16 * pos)
{
    /* returns the operand length in bytes; callers must verify that the
       returned length does not run past the end of the grpprl buffer.
       end is one-past-the-last buffer byte (NULL = unknown) and lets the
       measurement of count-derived operands stay inside the buffer */
    int len;
    Sprm aSprm;
#ifdef EXAMINE_SPRM
	U8 temp[256];
	U16 p;
	U8  *pi;
	int i;
#endif
    wvTrace (("Eating sprm %x\n", sprm));
    wvGetSprmFromU16 (&aSprm, sprm);
    if (sprm == sprmPChgTabs)
      {
	  wvTrace (("sprmPChgTabs\n"));
	  if (end != NULL)
	    {
		U8 dmx, amx;

		/* cch < 255 is the operand length itself; cch == 255 means
		   the counts define it -- probe only the two count bytes,
		   clamped to itbdMax, never reading outside the buffer.
		   An unmeasurable operand returns a length past the end so
		   every caller's bound check stops the walk */
		if (pointer >= end)
		    return (0x40000000);
		if (pointer[0] != 255)
		  {
		      len = pointer[0] + 1;
		      (*pos) += len;
		      return (len);
		  }
		if (pointer + 2 > end)
		    return (0x40000000);
		dmx = pointer[1];
		if (dmx > itbdMax)
		    dmx = itbdMax;
		if (pointer + 2 + 4 * (U32) dmx >= end)
		    return (0x40000000);
		amx = pointer[2 + 4 * dmx];
		if (amx > itbdMax)
		    amx = itbdMax;
		len = 3 + 4 * dmx + 3 * amx;
		(*pos) += len;
		return (len);
	    }
	  len = wvApplysprmPChgTabs (NULL, pointer, pos);
	  len++;
	  return (len);
      }
    else if ((sprm == sprmTDefTable) || (sprm == sprmTDefTable10))
      {
	  wvTrace (("sprmTDefTable\\sprmTDefTable10\n"));
	  len = bread_16ubit (pointer, pos);
	  len--;
      }
	else
      {
	  len = wvSprmLen (aSprm.spra);
#ifdef EXAMINE_SPRM
	  i = 0;
	  p = *pos;
	  pi = pointer;
#endif
	  wvTrace (("wvSprmLen len is %d\n", len));
	  if (len < 0)
	    {
		len = bread_8ubit (pointer, pos);
		/* bread increased pos, but in order to keep len and pos in
		   sync later on, we have to decreased it again */
		(*pos)--;
#ifdef EXAMINE_SPRM
		pi++;
		while(i < sizeof(temp) && i < len)
		{
			temp[i] = bread_8ubit(pi, &p);
			pi++;
			i++;
		}
#endif
		len++;
	    }
#ifdef EXAMINE_SPRM
	  else
	  {
		while(i < sizeof(temp) && i < len)
		{
			temp[i] = bread_8ubit(pi, &p);
			pi++;
			i++;
		}
	  }
#endif
      }
    (*pos) += len;
    return (len);
}
#undef EXAMINE_SPRM

Sprm
wvApplySprmFromBucket (wvVersion ver, U16 sprm, PAP * apap, CHP * achp,
		       SEP * asep, STSH * stsh, U8 * pointer, U16 * pos,
		       wvStream * data)
{
    BRC10 tempBRC10;
    U16 temp16;
    U8 temp8;
    PAP temppap;
    CHP tempchp;
    SEP tempsep;
    U8 toggle;
    Sprm RetSprm;

    /*bullet proofing */
    if (apap == NULL)
      {
	  wvInitPAP (&temppap);
	  apap = &temppap;
      }
    if (achp == NULL)
      {
	  wvInitCHP (&tempchp);
	  achp = &tempchp;
      }
    if (asep == NULL)
      {
#ifdef PURIFY
	  wvInitSEP (&tempsep);
#endif
	  asep = &tempsep;
      }
#ifdef SPRMTEST
    wvError (("sprm is %x\n", sprm));
#endif

    switch (sprm)
      {
	  /*Beginning of PAP */
      case sprmPIstd:
	  apap->istd = bread_16ubit (pointer, pos);
	  break;
      case sprmPIstdPermute:
	  wvApplysprmPIstdPermute (apap, pointer, pos);
	  break;
      case sprmPIncLvl:
	  wvApplysprmPIncLvl (apap, pointer, pos);
	  break;
      case sprmPJc80:
      case sprmPJc:		/* 0x2461; same operand as sprmPJc80 but
				   also carries jc values 5..9 */
	  apap->jc = bread_8ubit (pointer, pos);
	  wvTrace (("jc is now %d\n", apap->jc));
	  break;
      case sprmPFSideBySide:
	  apap->fSideBySide = bread_8ubit (pointer, pos);
	  break;
      case sprmPFKeep:
	  apap->fKeep = bread_8ubit (pointer, pos);
	  break;
      case sprmPFKeepFollow:
	  apap->fKeepFollow = bread_8ubit (pointer, pos);
	  break;
      case sprmPFPageBreakBefore:
	  apap->fPageBreakBefore = bread_8ubit (pointer, pos);
	  break;
      case sprmPBrcl:
	  apap->brcl = bread_8ubit (pointer, pos);
	  break;
      case sprmPBrcp:
	  apap->brcp = bread_8ubit (pointer, pos);
	  break;
      case sprmPIlvl:
	  apap->ilvl = bread_8ubit (pointer, pos);
	  break;
      case sprmPIlfo:
	  apap->ilfo = (S16) bread_16ubit (pointer, pos);
	  wvTrace (("ilfo is %d\n", apap->ilfo));
	  break;
      case sprmPFNoLineNumb:
	  apap->fNoLnn = bread_8ubit (pointer, pos);
	  break;
      case sprmPChgTabsPapx:
	  wvApplysprmPChgTabsPapx (apap, pointer, pos);
	  break;
      case sprmPDxaRight80:
      case sprmPDxaRight:	/* 0x845D */
	  apap->dxaRight = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDxaLeft80:
      case sprmPDxaLeft:	/* 0x845E */
	  apap->dxaLeft = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPNest80:
      case sprmPNest:		/* 0x465F */
	  /*
	     sprmPNest causes its operand, a two-byte dxa value to be
	     added to pap.dxaLeft. If the result of the addition is less than 0, 0 is
	     stored into pap.dxaLeft.
	   */
	  temp16 = (S16) bread_16ubit (pointer, pos);
	  apap->dxaLeft += temp16;
	  if (apap->dxaLeft < 0)
	      apap->dxaLeft = 0;
	  break;
      case sprmPDxaLeft180:
      case sprmPDxaLeft1:	/* 0x8460 */
	  apap->dxaLeft1 = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDyaLine:
	  wvGetLSPDFromBucket (&apap->lspd, pointer);
	  (*pos) += 4;
	  break;
      case sprmPDyaBefore:
	  apap->dyaBefore = bread_16ubit (pointer, pos);
	  break;
      case sprmPDyaAfter:
	  apap->dyaAfter = bread_16ubit (pointer, pos);
	  break;
      case sprmPChgTabs:
	  wvApplysprmPChgTabs (apap, pointer, pos);
	  break;
      case sprmPFInTable:
	  apap->fInTable = bread_8ubit (pointer, pos);
	  break;
      case sprmPFTtp:
	  apap->fTtp = bread_8ubit (pointer, pos);
	  break;
      case sprmPDxaAbs:
	  apap->dxaAbs = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDyaAbs:
	  apap->dyaAbs = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDxaWidth:
	  apap->dxaWidth = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPPc:
	  wvApplysprmPPc (apap, pointer, pos);
	  break;
      case sprmPBrcTop10:
	  wvGetBRC10FromBucket (&tempBRC10, pointer);
	  (*pos) += 2;
	  wvConvertBRC10ToBRC (&apap->brcTop, &tempBRC10);
	  break;
      case sprmPBrcLeft10:
	  wvGetBRC10FromBucket (&tempBRC10, pointer);
	  (*pos) += 2;
	  wvConvertBRC10ToBRC (&apap->brcLeft, &tempBRC10);
	  break;
      case sprmPBrcBottom10:
	  wvGetBRC10FromBucket (&tempBRC10, pointer);
	  (*pos) += 2;
	  wvConvertBRC10ToBRC (&apap->brcBottom, &tempBRC10);
	  break;
      case sprmPBrcRight10:
	  wvGetBRC10FromBucket (&tempBRC10, pointer);
	  (*pos) += 2;
	  wvConvertBRC10ToBRC (&apap->brcRight, &tempBRC10);
	  break;
      case sprmPBrcBetween10:
	  wvGetBRC10FromBucket (&tempBRC10, pointer);
	  (*pos) += 2;
	  wvConvertBRC10ToBRC (&apap->brcBetween, &tempBRC10);
	  break;
      case sprmPBrcBar10:
	  wvGetBRC10FromBucket (&tempBRC10, pointer);
	  (*pos) += 2;
	  wvConvertBRC10ToBRC (&apap->brcBar, &tempBRC10);
	  break;
      case sprmPDxaFromText10:
	  apap->dxaFromText = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPWr:
	  apap->wr = bread_8ubit (pointer, pos);
	  break;
      case sprmPBrcTop80:
	  (*pos) += wvGetBRCFromBucket (ver, &apap->brcTop, pointer);
	  break;
      case sprmPBrcLeft80:
	  (*pos) += wvGetBRCFromBucket (ver, &apap->brcLeft, pointer);
	  break;
      case sprmPBrcBottom80:
	  (*pos) += wvGetBRCFromBucket (ver, &apap->brcBottom, pointer);
	  break;
      case sprmPBrcRight80:
	  (*pos) += wvGetBRCFromBucket (ver, &apap->brcRight, pointer);
	  break;
      case sprmPBrcBetween80:
	  (*pos) += wvGetBRCFromBucket (ver, &apap->brcBetween, pointer);
	  break;
      case sprmPBrcBar80:
	  (*pos) += wvGetBRCFromBucket (ver, &apap->brcBar, pointer);
	  break;
      case sprmPBrcTop:		/* 0xC64E: BrcOperand, full COLORREF */
	  (*pos) += wvGetBRCOperandFromBucket (&apap->brcTop, pointer);
	  break;
      case sprmPBrcLeft:	/* 0xC64F */
	  (*pos) += wvGetBRCOperandFromBucket (&apap->brcLeft, pointer);
	  break;
      case sprmPBrcBottom:	/* 0xC650 */
	  (*pos) += wvGetBRCOperandFromBucket (&apap->brcBottom, pointer);
	  break;
      case sprmPBrcRight:	/* 0xC651 */
	  (*pos) += wvGetBRCOperandFromBucket (&apap->brcRight, pointer);
	  break;
      case sprmPBrcBetween:	/* 0xC652 */
	  (*pos) += wvGetBRCOperandFromBucket (&apap->brcBetween, pointer);
	  break;
      case sprmPBrcBar:		/* 0xC653 */
	  (*pos) += wvGetBRCOperandFromBucket (&apap->brcBar, pointer);
	  break;
      case sprmPFNoAutoHyph:
	  apap->fNoAutoHyph = bread_8ubit (pointer, pos);
	  break;
      case sprmPWHeightAbs:
	  /* ???? apap->wHeightAbs */
	  (*pos) += 2;
	  break;
      case sprmPDcs:
	  wvGetDCSFromBucket (&apap->dcs, pointer);
	  (*pos) += 2;
	  break;
      case sprmPShd80:
	  wvGetSHDFromBucket (&apap->shd, pointer);
	  (*pos) += 2;
	  break;
      case sprmPShd:		/* 0xC64D: SHDOperand, COLORREF colors */
	  (*pos) += wvGetSHDOperandFromBucket (&apap->shd, pointer);
	  break;
      case sprmPDyaFromText:
	  apap->dyaFromText = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDxaFromText:
	  apap->dxaFromText = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPFLocked:
	  apap->fLocked = bread_8ubit (pointer, pos);
	  break;
      case sprmPFWidowControl:
	  apap->fWidowControl = bread_8ubit (pointer, pos);
	  break;
      case sprmPFKinsoku:
	  apap->fKinsoku = bread_8ubit (pointer, pos);
	  break;
      case sprmPFWordWrap:
	  apap->fWordWrap = bread_8ubit (pointer, pos);
	  break;
      case sprmPFOverflowPunct:
	  apap->fOverflowPunct = bread_8ubit (pointer, pos);
	  break;
      case sprmPFTopLinePunct:
	  apap->fTopLinePunct = bread_8ubit (pointer, pos);
	  break;
      case sprmPFAutoSpaceDE:
	  apap->fAutoSpaceDE = bread_8ubit (pointer, pos);
	  break;
      case sprmPFAutoSpaceDN:
	  apap->fAtuoSpaceDN = bread_8ubit (pointer, pos);
	  break;
      case sprmPWAlignFont:
	  apap->wAlignFont = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPFrameTextFlow:
	  wvApplysprmPFrameTextFlow (apap, pointer, pos);
	  break;
      case sprmPISnapBaseLine:
	  /*obsolete: not applicable in Word97 and later versions */
	  (*pos)++;
	  break;
      case sprmPNLvlAnm:
	  /*obsolete: not applicable in Word97 and later version */
	  apap->nLvlAnm = bread_8ubit (pointer, pos);
	  wvTrace (("%d\n", apap->nLvlAnm));
	  break;
      case sprmPAnld:
	  wvApplysprmPAnld (ver, apap, pointer, pos);
	  break;
      case sprmPPropRMark:
	  wvApplysprmPPropRMark (apap, pointer, pos);
	  break;
      case sprmPOutLvl:
	  /*has no effect if pap.istd is < 1 or is > 9 */
	  temp8 = bread_8ubit (pointer, pos);
	  if ((apap->istd >= 1) && (apap->istd <= 9))
	      apap->lvl = temp8;
	  break;
      case sprmPFBiDi:
	apap->fBidi = bread_8ubit (pointer, pos);
	break;
      case sprmPFNumRMIns:
	  apap->fNumRMIns = bread_8ubit (pointer, pos);
	  break;
      case sprmPCrLf:
	  /* ???? */
	  (*pos)++;
	  break;
      case sprmPNumRM:
	  wvApplysprmPNumRM (apap, pointer, pos);
	  break;
      case sprmPHugePapx2:
      case sprmPHugePapx:
	  wvApplysprmPHugePapx (apap, pointer, pos, data, stsh);
	  break;
      case sprmPFUsePgsuSettings:
	  apap->fUsePgsuSettings = bread_8ubit (pointer, pos);
	  break;
      case sprmPFAdjustRight:
	  apap->fAdjustRight = bread_8ubit (pointer, pos);
	  break;
      case sprmPRsid:
	  /*  apap->rsid = */ bread_32ubit (pointer, pos);
	  break;
      case sprmPItap:
	  apap->itap = (S32) bread_32ubit (pointer, pos);
	  /* itap is the nested-table depth; fInTable stays a boolean
	     since consumers test it with == 1 */
	  apap->fInTable = (apap->itap > 0) ? 1 : 0;
		/* apap->fTtp++;   this line fixed bug #11433 but caused #12476 */
	  break;
      case sprmPDtap:		/* 0x664A: table depth delta */
	  apap->itap += (S32) bread_32ubit (pointer, pos);
	  if (apap->itap < 0)
	      apap->itap = 0;
	  apap->fInTable = (apap->itap > 0) ? 1 : 0;
	  break;
      case sprmPFInnerTableCell:
	  apap->fInnerTableCell = bread_8ubit (pointer, pos);
	  break;
      case sprmPFInnerTtp:
	  apap->fInnerTtp = bread_8ubit (pointer, pos);
	  break;
      case sprmPDxcRight:	/* 0x4455: right indent, 1/100 chars */
	  apap->dxcRight = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDxcLeft:	/* 0x4456 */
	  apap->dxcLeft = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDxcLeft1:	/* 0x4457 */
	  apap->dxcLeft1 = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDylBefore:	/* 0x4458: space before, 1/100 lines */
	  apap->dylBefore = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPDylAfter:	/* 0x4459 */
	  apap->dylAfter = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmPFOpenTch:
	  apap->fOpenTch = bread_8ubit (pointer, pos);
	  break;
      case sprmPFDyaBeforeAuto:
	  apap->fDyaBeforeAuto = bread_8ubit (pointer, pos);
	  break;
      case sprmPFDyaAfterAuto:
	  apap->fDyaAfterAuto = bread_8ubit (pointer, pos);
	  break;
      case sprmPFNoAllowOverlap:
	  apap->fNoAllowOverlap = bread_8ubit (pointer, pos);
	  break;
      case sprmPWall:
	  apap->fWall = bread_8ubit (pointer, pos);
	  break;
      case sprmPIpgp:
	  apap->ipgp = (S32) bread_32ubit (pointer, pos);
	  break;
      case sprmPCnf:		/* conditional table-style formatting;
				   only valid inside table styles */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmPIstdListPermute:	/* MUST be ignored (MS-DOC) */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmPTIstdInfo:	/* MUST be ignored (MS-DOC) */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmPTableProps:	/* 0x646B: PrcData in Data stream;
				   same handling as sprmPHugePapx */
	  wvApplysprmPHugePapx (apap, pointer, pos, data, stsh);
	  break;
      case sprmPFContextualSpacing:
	  apap->fContextualSpacing = bread_8ubit (pointer, pos);
	  break;
      case sprmPFMirrorIndents:
	  apap->fMirrorIndents = bread_8ubit (pointer, pos);
	  break;
      case sprmPTtwo:
	  apap->tTwo = bread_8ubit (pointer, pos);
	  break;
	  /*End of PAP */


	  /*Begin of CHP */
      case sprmCFRMarkDel:
	  achp->fRMarkDel = bread_8ubit (pointer, pos);
	  break;
      case sprmCFRMarkIns:
	  achp->fRMark = bread_8ubit (pointer, pos);
	  break;
      case sprmCFFldVanish:
	  achp->fFldVanish = bread_8ubit (pointer, pos);
	  break;
      case sprmCPicLocation:
	  if (ver != WORD8)
	    {
		wvTrace (("byte is %x\n", bread_8ubit (pointer, pos)));
		pointer++;
	    }
	  /*
	     This sprm moves the 4-byte operand of the sprm into the
	     chp.fcPic field. It simultaneously sets chp.fSpec to 1.
	   */
	  achp->fcPic_fcObj_lTagObj = bread_32ubit (pointer, pos);
	  wvTrace (("Len is %x\n", achp->fcPic_fcObj_lTagObj));
	  achp->fSpec = 1;
	  break;
      case sprmCIbstRMark:
	  achp->ibstRMark = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmCDttmRMark:
	  wvGetDTTMFromBucket (&achp->dttmRMark, pointer);
	  (*pos) += 4;
	  break;
      case sprmCFData:
	  achp->fData = bread_8ubit (pointer, pos);
	  break;
      case sprmCIdslRMark:
	  achp->idslRMReason = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmCChs:
	  wvApplysprmCChs (achp, pointer, pos);
	  break;
      case sprmCSymbol:
	  wvApplysprmCSymbol (ver, achp, pointer, pos);
	  break;
      case sprmCFOle2:
	  achp->fOle2 = bread_8ubit (pointer, pos);
	  break;
      case sprmCHighlight:
	  /* ico (fHighlight is set to 1 iff ico is not 0) */
	  achp->icoHighlight = bread_8ubit (pointer, pos);
	  if (achp->icoHighlight)
	      achp->fHighlight = 1;	/*? */

	  /*another possibility is... */
	  /* if (achp->ico) achp->fHighlight = 1; */
	  /*
	     or is it something else, who knows the entire documentation on
	     the topic consist of the if and only if (iff) line, or maybe
	     iff is a type for if, who knows eh ?
	   */
	  break;
      case sprmCObjLocation:
	  achp->fcPic_fcObj_lTagObj = (S32) bread_32ubit (pointer, pos);
	  break;
      case sprmCIdCharType:	/* obsolete; 2-byte operand must be consumed */
	  bread_16ubit (pointer, pos);
	  break;
      case sprmCFWebHidden:
	  achp->fWebHidden = bread_8ubit (pointer, pos);
	  break;
      case sprmCRsidProp:
      case sprmCRsidRMDel:
	  bread_32ubit (pointer, pos);
	  break;
      case sprmCFSpecVanish:
	  achp->fSpecVanish = bread_8ubit (pointer, pos);
	  break;
      case sprmCFMathPr:	/* MathPrOperand; equation justification */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmCIstd:
	  achp->istd = bread_16ubit (pointer, pos);
	  break;
      case sprmCIstdPermute:
	  wvApplysprmCIstdPermute (achp, pointer, pos);	/*unfinished */
	  break;
      case sprmCDefault:
	  wvApplysprmCDefault (achp, pointer, pos);
	  break;
      case sprmCPlain:
	  /* operand is a single byte that MUST be 0 and is ignored */
	  bread_8ubit (pointer, pos);
	  wvApplysprmCPlain (achp, stsh);
	  break;
      case sprmCKcd:
	  /* emphasis-mark kind (East Asian), stored for completeness */
	  achp->kcd = bread_8ubit (pointer, pos);
	  break;
      case sprmCFBold:
	  toggle = bread_8ubit (pointer, pos);
	  wvTrace (("toggle here is %d, istd is %d\n", toggle, achp->istd));
	  wvTOGGLE (achp->fBold, achp, stsh, toggle, fBold) break;
      case sprmCFItalic:
	  toggle = bread_8ubit (pointer, pos);
	  wvTrace (("Italic is %d, sprm val is %d\n", achp->fItalic, toggle));
	  wvTOGGLE (achp->fItalic, achp, stsh, toggle, fItalic)
	      wvTrace (("Italic is now %d\n", achp->fItalic));
	  break;
      case sprmCFStrike:
	  toggle = bread_8ubit (pointer, pos);
	  wvTOGGLE (achp->fStrike, achp, stsh, toggle, fStrike) break;
      case sprmCFOutline:
	  toggle = bread_8ubit (pointer, pos);
	  wvTOGGLE (achp->fOutline, achp, stsh, toggle, fOutline) break;
      case sprmCFShadow:
	  toggle = bread_8ubit (pointer, pos);
	  wvTOGGLE (achp->fShadow, achp, stsh, toggle, fShadow) break;
      case sprmCFSmallCaps:
	  toggle = bread_8ubit (pointer, pos);
	  wvTOGGLE (achp->fSmallCaps, achp, stsh, toggle, fSmallCaps) break;
      case sprmCFCaps:
	  toggle = bread_8ubit (pointer, pos);
	  wvTOGGLE (achp->fCaps, achp, stsh, toggle, fCaps) break;
      case sprmCFVanish:
	  wvTrace (("vanish modified\n"));
	  toggle = bread_8ubit (pointer, pos);
	  wvTOGGLE (achp->fVanish, achp, stsh, toggle, fVanish) break;
      case sprmCFtcDefault:
	  wvApplysprmCFtcDefault (achp, stsh, pointer, pos);
	  break;
      case sprmCKul:
	  achp->kul = bread_8ubit (pointer, pos);
	  break;
      case sprmCSizePos:
	  wvApplysprmCSizePos (achp, pointer, pos);
	  break;
      case sprmCDxaSpace:
	  achp->dxaSpace = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmCIco:
	  achp->ico = bread_8ubit (pointer, pos);
	  break;
      case sprmCHps:
	  /*incorrect marked as being a byte in docs ? */
	  achp->hps = bread_16ubit (pointer, pos);
	  break;
      case sprmCHpsInc:
	  wvApplysprmCHpsInc (achp, pointer, pos);
	  break;
      case sprmCHpsPos:
	  /*incorrect marked as being a byte in docs ? */
	  achp->hpsPos = bread_16ubit (pointer, pos);
	  break;
      case sprmCHpsPosAdj:
	  wvApplysprmCHpsPosAdj (achp, pointer, pos);
	  break;
      case sprmCMajority:
	  wvApplysprmCMajority (achp, stsh, pointer, pos);
	  break;
      case sprmCIss:
	  achp->iss = bread_8ubit (pointer, pos);
	  break;
      case sprmCHpsNew50:
	  /* spra 6: cb byte followed by the operand (a U16 hps) */
	  temp8 = bread_8ubit (pointer, pos);
	  if (temp8 >= 2)
	      achp->hps = bread_16ubit (pointer, pos);
	  if (temp8 > 2)
	      (*pos) += temp8 - 2;
	  break;
      case sprmCHpsInc1:
	  wvApplysprmCHpsInc1 (achp, pointer, pos);
	  break;
      case sprmCHpsKern:
	  /*the spec would you have you believe that this is a U8 */
	  achp->hpsKern = bread_16ubit (pointer, pos);
	  break;
      case sprmCMajority50:
	  wvApplysprmCMajority50 (achp, stsh, pointer, pos);
	  break;
      case sprmCHpsMul:
	  /*percentage to grow hps ?? */
	  /* U16*U16 can exceed INT_MAX -- do the multiply in U32 */
	  achp->hps = (U16) ((achp->hps * (U32) bread_16ubit (pointer, pos)) / 100);
	  break;
      case sprmCHresi:
	  /* HresiOperand: hres byte + chHres byte */
	  achp->ysr = bread_8ubit (pointer, pos);
	  achp->chYsr = bread_8ubit (pointer, pos);
	  break;
      case sprmCRgFtc0:
	  achp->ftcAscii = bread_16ubit (pointer, pos);
	  break;
      case sprmCRgFtc1:
	  achp->ftcFE = bread_16ubit (pointer, pos);
	  break;
      case sprmCRgFtc2:
	  achp->ftcOther = bread_16ubit (pointer, pos);
	  break;
      case sprmCCharScale:
	  /* horizontal scale, percent (1-600); 100 is normal */
	  achp->wCharScale = bread_16ubit (pointer, pos);
	  break;
      case sprmCFDStrike:
	  achp->fDStrike = bread_8ubit (pointer, pos);
	  break;
      case sprmCFImprint:
	  achp->fImprint = bread_8ubit (pointer, pos);
	  break;
      case sprmCFSpec:
	  achp->fSpec = bread_8ubit (pointer, pos);
	  break;
      case sprmCFObj:
	  achp->fObj = bread_8ubit (pointer, pos);
	  break;
      case sprmCPropRMark90:
      case sprmCPropRMark:
	  wvApplysprmCPropRMark (achp, pointer, pos);
	  break;
      case sprmCFEmboss:
	  achp->fEmboss = bread_8ubit (pointer, pos);
	  break;
      case sprmCSfxText:
	  achp->sfxtText = bread_8ubit (pointer, pos);
	  break;
      case sprmCDispFldRMark:
	  wvApplysprmCDispFldRMark (achp, pointer, pos);
	  break;
      case sprmCIbstRMarkDel:
	  achp->ibstRMarkDel = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmCDttmRMarkDel:
	  wvGetDTTMFromBucket (&achp->dttmRMarkDel, pointer);
	  (*pos) += 4;
	  break;
      case sprmCBrc80:
	  (*pos) += wvGetBRCFromBucket (ver, &achp->brc, pointer);
	  break;
      case sprmCShd80:
	  wvGetSHDFromBucket (&achp->shd, pointer);
	  (*pos) += 2;
	  break;
      case sprmCIdslRMarkDel:
	  /* achp->idslRMReasonDel ???? */
	  (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmCFUsePgsuSettings:
	  achp->fUsePgsuSettings = bread_8ubit (pointer, pos);
	  break;
      case sprmCRgLid0_80:
	  achp->lidDefault = bread_16ubit (pointer, pos);
	  break;
      case sprmCRgLid1_80:
	  achp->lidFE = bread_16ubit (pointer, pos);
	  break;
      case sprmCIdctHint:
	  achp->idctHint = bread_8ubit (pointer, pos);
	  break;
      case sprmCFFtcAsciSymb:	/* not fully mentioned in spec */
	  achp->fFtcAsciSym = bread_8ubit (pointer, pos);
	  break;
      case sprmCCpg:		/* not fully mentioned in spec */
	  achp->cpg = bread_16ubit (pointer, pos);
	  break;
      case sprmCLid:		/*
				   only used internally, never stored ( word 97 )
				   but exists in earlier versions so...
				 */
	  achp->lid = bread_16ubit (pointer, pos);
	  achp->lidDefault = achp->lid;
	  achp->lidFE = achp->lid;
	  wvTrace (("lid is %x\n", achp->lidDefault));
	  break;
      case sprmCRsidText:
		 bread_32ubit (pointer, pos);
	  break;

	  /* Word 2000+ (MS-DOC) */
      case sprmCCv:
	  achp->cv = bread_32ubit (pointer, pos);
	  break;
      case sprmCShd:
	  (*pos) += wvGetSHDOperandFromBucket (&achp->shd, pointer);
	  break;
      case sprmCBrc:
	  (*pos) += wvGetBRCOperandFromBucket (&achp->brc, pointer);
	  break;
      case sprmCRgLid0:
	  achp->lidDefault = bread_16ubit (pointer, pos);
	  break;
      case sprmCRgLid1:
	  achp->lidFE = bread_16ubit (pointer, pos);
	  break;
      case sprmCFNoProof:
	  achp->fNoProof = bread_8ubit (pointer, pos);
	  break;
      case sprmCFitText:	/* CFitTextOperand; no Abi equivalent */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmCCvUl:
	  achp->cvUl = bread_32ubit (pointer, pos);
	  break;
      case sprmCFELayout:	/* FarEastLayoutOperand */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmCLbcCRJ:		/* LBCOperand; line-break char kind */
	  bread_8ubit (pointer, pos);
	  break;
      case sprmCFComplexScripts:
	  /* complex-script formatting applies regardless of the Unicode
	     coverage; same prop selection as sprmCFBiDi */
	  achp->fBidi = bread_8ubit (pointer, pos);
	  break;
      case sprmCWall:
	  bread_8ubit (pointer, pos);
	  break;
      case sprmCCnf:		/* conditional table-style formatting */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmCNeedFontFixup:
	  achp->fNeedFontFixup = bread_8ubit (pointer, pos);
	  break;
      case sprmCPbiIBullet:
	  bread_32ubit (pointer, pos);
	  break;
      case sprmCPbiGrf:
	  bread_16ubit (pointer, pos);
	  break;
      case sprmCFSdtVanish:
	  achp->fSdtVanish = bread_8ubit (pointer, pos);
	  break;

	  /* BiDi */

      case sprmCFBiDi:		/* is this run BiDi */
	  achp->fBidi = bread_8ubit (pointer, pos);
	  break;

      case sprmCFDiacColor: /* ???? spra is 0, so the operand is 1 byte */
	bread_8ubit (pointer, pos);
	break;

      case sprmCFBoldBi:	
	achp->fBoldBidi = bread_8ubit (pointer, pos);
	break;

      case sprmCFItalicBi:
	achp->fItalicBidi = bread_8ubit (pointer, pos);
	break;

      case sprmCFtcBi:
	achp->ftcBidi = bread_16ubit (pointer, pos);
	break;

      case sprmCLidBi:
	achp->lidBidi = bread_16ubit (pointer, pos);
	break;

      case sprmCIcoBi:
	achp->icoBidi = (U8) bread_16ubit (pointer, pos);
	break;

      case sprmCHpsBi:
	achp->hpsBidi = bread_16ubit (pointer, pos);
	break;
	  /* End of CHP */


	  /* Begin of SEP */
      case sprmScnsPgn:
	  asep->cnsPgn = bread_8ubit (pointer, pos);
	  break;
      case sprmSiHeadingPgn:
	  asep->iHeadingPgn = bread_8ubit (pointer, pos);
	  break;
      case sprmSOlstAnm:
	  wvApplysprmSOlstAnm (ver, asep, pointer, pos);
	  break;
      case sprmSDxaColWidth:
      case sprmSDxaColSpacing:
	  {
	      /* ColWidthOperand / ColSpacingOperand (MS-DOC 2.4.2.19):
	         byte 0 is the column index, bytes 1-2 the signed dxa */
	      U8 iCol = bread_8ubit (pointer, pos);
	      S32 dxaCol = (S32) (S16) bread_16ubit (pointer, pos);
	      if (iCol <= 43)
		  asep->rgdxaColumnWidthSpacing[iCol * 2
						+ (sprm == sprmSDxaColWidth ? 0 : 1)]
			  = dxaCol;
	  }
	  break;
      case sprmSFEvenlySpaced:
	  asep->fEvenlySpaced = bread_8ubit (pointer, pos);
	  break;
      case sprmSFProtected:
	  asep->fUnlocked = bread_8ubit (pointer, pos);
	  break;
      case sprmSDmBinFirst:
	  asep->dmBinFirst = bread_16ubit (pointer, pos);
	  break;
      case sprmSDmBinOther:
	  asep->dmBinOther = bread_16ubit (pointer, pos);
	  break;
      case sprmSBkc:
	  asep->bkc = bread_8ubit (pointer, pos);
	  break;
      case sprmSFTitlePage:
	  asep->fTitlePage = bread_8ubit (pointer, pos);
	  break;
      case sprmSCcolumns:
	  asep->ccolM1 = bread_16ubit (pointer, pos);
	  break;
      case sprmSDxaColumns:
	  asep->dxaColumns = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSFAutoPgn:
	  asep->fAutoPgn = bread_8ubit (pointer, pos);
	  break;
      case sprmSNfcPgn:
	  asep->nfcPgn = bread_8ubit (pointer, pos);
	  break;
      case sprmSDyaPgn:
	  asep->dyaPgn = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSDxaPgn:
	  asep->dxaPgn = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSFPgnRestart:
	  asep->fPgnRestart = bread_8ubit (pointer, pos);
	  break;
      case sprmSFEndnote:
	  asep->fEndNote = bread_8ubit (pointer, pos);
	  break;
      case sprmSLnc:
	  asep->lnc = bread_8ubit (pointer, pos);
	  break;
      case sprmSGprfIhdt:
	  asep->grpfIhdt = bread_8ubit (pointer, pos);
	  break;
      case sprmSNLnnMod:
	  asep->nLnnMod = bread_16ubit (pointer, pos);
	  break;
      case sprmSDxaLnn:
	  asep->dxaLnn = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSDyaHdrTop:
	  asep->dyaHdrTop = bread_16ubit (pointer, pos);
	  break;
      case sprmSDyaHdrBottom:
	  asep->dyaHdrBottom = bread_16ubit (pointer, pos);
	  break;
      case sprmSLBetween:
	  asep->fLBetween = bread_8ubit (pointer, pos);
	  break;
      case sprmSVjc:
	  asep->vjc = bread_8ubit (pointer, pos);
	  break;
      case sprmSLnnMin:
	  asep->lnnMin = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSPgnStart97:
	  asep->pgnStart = bread_16ubit (pointer, pos);
	  break;
      case sprmSPgnStart:
	  asep->pgnStart = bread_32ubit (pointer, pos);
	  break;
      case sprmSBOrientation:
	  asep->dmOrientPage = bread_8ubit (pointer, pos);
	  break;
      case sprmSBCustomize:
	  /*noone knows what this is */
	  bread_8ubit (pointer, pos);
	  break;
      case sprmSXaPage:
	  asep->xaPage = bread_16ubit (pointer, pos);
	  break;
      case sprmSYaPage:
	  asep->yaPage = bread_16ubit (pointer, pos);
	  break;
      case sprmSDxaLeft:
	  asep->dxaLeft = bread_16ubit (pointer, pos);
	  break;
      case sprmSDxaRight:
	  asep->dxaRight = bread_16ubit (pointer, pos);
	  break;
      case sprmSDyaTop:
	  asep->dyaTop = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSDyaBottom:
	  asep->dyaBottom = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSDzaGutter:
	  asep->dzaGutter = bread_16ubit (pointer, pos);
	  break;
      case sprmSDmPaperReq:
	  asep->dmPaperReq = bread_16ubit (pointer, pos);
	  break;
      case sprmSPropRMark97:
      case sprmSPropRMark:
	  wvApplysprmSPropRMark (asep, pointer, pos);
	  break;
      case sprmSFBiDi:
	  asep->fBidi = bread_8ubit (pointer, pos);
	  break;
      case sprmSFFacingCol:
	  bread_8ubit (pointer, pos);
	  break;
      case sprmSFRTLGutter:
	  asep->fRTLGutter = bread_8ubit (pointer, pos);
	  break;
      case sprmSBrcTop80:
	  (*pos) += wvGetBRCFromBucket (ver, &asep->brcTop, pointer);
	  break;
      case sprmSBrcLeft80:
	  (*pos) += wvGetBRCFromBucket (ver, &asep->brcLeft, pointer);
	  break;
      case sprmSBrcBottom80:
	  (*pos) += wvGetBRCFromBucket (ver, &asep->brcBottom, pointer);
	  break;
      case sprmSBrcRight80:
	  (*pos) += wvGetBRCFromBucket (ver, &asep->brcRight, pointer);
	  break;
      case sprmSBrcTop:
	  (*pos) += wvGetBRCOperandFromBucket (&asep->brcTop, pointer);
	  break;
      case sprmSBrcLeft:
	  (*pos) += wvGetBRCOperandFromBucket (&asep->brcLeft, pointer);
	  break;
      case sprmSBrcBottom:
	  (*pos) += wvGetBRCOperandFromBucket (&asep->brcBottom, pointer);
	  break;
      case sprmSBrcRight:
	  (*pos) += wvGetBRCOperandFromBucket (&asep->brcRight, pointer);
	  break;
      case sprmSPgbProp:
	  {
	      /* SPgbPropOperand: low byte packs pgbApplyTo (bits 0-2),
	         pgbPageDepth (3-4) and pgbOffsetFrom (5-7); byte 1 is
	         reserved */
	      U16 pgb = bread_16ubit (pointer, pos);
	      asep->pgbProp = (S16) pgb;
	      asep->pgbApplyTo = pgb & 0x7;
	      asep->pgbPageDepth = (pgb >> 3) & 0x3;
	      asep->pgbOffsetFrom = (pgb >> 5) & 0x7;
	  }
	  break;
      case sprmSDxtCharSpace:
	  asep->dxtCharSpace = (S32) bread_32ubit (pointer, pos);
	  break;
      case sprmSDyaLinePitch:
	  /* incorrectly documented; is only word size */
	  asep->dyaLinePitch = (S32) bread_16ubit (pointer, pos);
	  break;
      case sprmSClm:
	  asep->clm = bread_16ubit (pointer, pos);
	  break;
      case sprmSTextFlow:
	  asep->wTextFlow = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmSWall:
	  /* Bool8 "barrier": sprms after this must override, so
	     plain sequential application is already correct */
	  bread_8ubit (pointer, pos);
	  break;
      case sprmSRsid:
	  bread_32ubit (pointer, pos);
	  break;
      case sprmSFpc:
      case sprmSRncFtn:
      case sprmSRncEdn:
	  bread_8ubit (pointer, pos);
	  break;
      case sprmSNFtn:
      case sprmSNfcFtnRef:
      case sprmSNEdn:
      case sprmSNfcEdnRef:
	  bread_16ubit (pointer, pos);
	  break;
	  /* End of SEP */

	  /* Begin of TAP */
      case sprmTJc90:
      case sprmTJc:
	  apap->ptap.jc = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTFCantSplit90:
      case sprmTFCantSplit:
	  apap->ptap.fCantSplit = bread_8ubit (pointer, pos);
	  break;
      case sprmTTableHeader:
	  apap->ptap.fTableHeader = bread_8ubit (pointer, pos);
	  break;
      case sprmTDyaRowHeight:
	  /* NOTE: this previously wrote to asep->dyaLinePitch, dumping
	     row heights into the SEP; it belongs in the TAP */
	  apap->ptap.dyaRowHeight = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTDiagLine:
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmTHTMLProps:
	  apap->ptap.lwHTMLProps = (S32) bread_32ubit (pointer, pos);
	  break;
      case sprmTFBiDi:
      case sprmTFBiDi90:
	  apap->ptap.fBiDi = bread_16ubit (pointer, pos);
	  break;
      case sprmTPc:
	  {
	      U8 pc = bread_8ubit (pointer, pos);
	      apap->ptap.pcVert = (pc & 0x30) >> 4;
	      apap->ptap.pcHorz = (pc & 0xC0) >> 6;
	  }
	  break;
      case sprmTDxaAbs:
	  apap->ptap.dxaAbs = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTDyaAbs:
	  apap->ptap.dyaAbs = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTDxaFromText:
	  apap->ptap.dxaFromText = bread_16ubit (pointer, pos);
	  break;
      case sprmTDyaFromText:
	  apap->ptap.dyaFromText = bread_16ubit (pointer, pos);
	  break;
      case sprmTDxaFromTextRight:
	  apap->ptap.dxaFromTextRight = bread_16ubit (pointer, pos);
	  break;
      case sprmTDyaFromTextBottom:
	  apap->ptap.dyaFromTextBottom = bread_16ubit (pointer, pos);
	  break;
      case sprmTTableWidth:
	  apap->ptap.ftsTableWidth = bread_8ubit (pointer, pos);
	  apap->ptap.wTableWidth = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTWidthBefore:
	  apap->ptap.ftsWidthBefore = bread_8ubit (pointer, pos);
	  apap->ptap.wWidthBefore = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTWidthAfter:
	  apap->ptap.ftsWidthAfter = bread_8ubit (pointer, pos);
	  apap->ptap.wWidthAfter = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTWidthIndent:
	  apap->ptap.ftsWidthIndent = bread_8ubit (pointer, pos);
	  apap->ptap.wWidthIndent = (S16) bread_16ubit (pointer, pos);
	  break;
      case sprmTFAutofit:
	  apap->ptap.fAutofit = bread_8ubit (pointer, pos);
	  break;
      case sprmTFKeepFollow:
	  apap->ptap.fKeepFollow = bread_8ubit (pointer, pos);
	  break;
      case sprmTFNoAllowOverlap:
	  apap->ptap.fNoAllowOverlap = bread_8ubit (pointer, pos);
	  break;
      case sprmTWall:
	  apap->ptap.fWall = bread_8ubit (pointer, pos);
	  break;
      case sprmTIstd:
	  apap->ptap.istdTable = bread_16ubit (pointer, pos);
	  break;
      case sprmTDxaLeft:
	  wvApplysprmTDxaLeft (&apap->ptap, pointer, pos);
	  break;
      case sprmTDxaGapHalf:
	  wvApplysprmTDxaGapHalf (&apap->ptap, pointer, pos);
	  break;
      case sprmTTableBorders80:
	  wvApplysprmTTableBorders (ver, &apap->ptap, pointer, pos);
	  break;
      case sprmTTableBorders:
	  wvApplysprmTTableBorders97 (&apap->ptap, pointer, pos);
	  break;
      case sprmTDefTable10:
	  wvApplysprmTDefTable10 (&apap->ptap, pointer, pos);
	  break;
      case sprmTDefTable:
	  wvApplysprmTDefTable (&apap->ptap, pointer, pos);
	  break;
      case sprmTDefTableShd80:
	  /*
	     wvApplysprmTDefTableShd follows the written spec, but
	     it isnt't working out for me, maybe its my own fault,
	     anyhow Im trying wv2 out temporarily
	   */
	  wv2ApplysprmTDefTableShd (&apap->ptap, pointer, pos);
	  /*
	     wvApplysprmTDefTableShd(&apap->ptap,pointer,pos);
	   */
	  break;
      case sprmTDefTableShd:
      case sprmTDefTableShdRaw:
	  wvApplysprmTDefTableShdNew (&apap->ptap, pointer, pos, 0);
	  break;
      case sprmTDefTableShd2nd:
      case sprmTDefTableShdRaw2nd:
	  wvApplysprmTDefTableShdNew (&apap->ptap, pointer, pos, 22);
	  break;
      case sprmTDefTableShd3rd:
      case sprmTDefTableShdRaw3rd:
	  wvApplysprmTDefTableShdNew (&apap->ptap, pointer, pos, 44);
	  break;
      case sprmTTlp:
	  wvGetTLPFromBucket (&(apap->ptap.tlp), pointer);
	  (*pos) += cbTLP;
	  break;
      case sprmTSetBrc80:
	  wvApplysprmTSetBrc (ver, &apap->ptap, pointer, pos);
	  break;
      case sprmTSetBrc:
	  wvApplysprmTSetBrcNew (&apap->ptap, pointer, pos);
	  break;
      case sprmTInsert:
	  wvApplysprmTInsert (&apap->ptap, pointer, pos);
	  break;
      case sprmTDelete:
	  wvApplysprmTDelete (&apap->ptap, pointer, pos);
	  break;
      case sprmTDxaCol:
	  wvApplysprmTDxaCol (&apap->ptap, pointer, pos);
	  break;
      case sprmTMerge:
	  wvApplysprmTMerge (&apap->ptap, pointer, pos);
	  break;
      case sprmTSplit:
	  wvApplysprmTSplit (&apap->ptap, pointer, pos);
	  break;
      case sprmTSetBrc10:
	  wvApplysprmTSetBrc10 (&apap->ptap, pointer, pos);
	  break;
      case sprmTSetShd95:
	  wvApplysprmTSetShd (&apap->ptap, pointer, pos);
	  break;
      case sprmTSetShdOdd95:
	  wvApplysprmTSetShdOdd (&apap->ptap, pointer, pos);
	  break;
      case sprmTSetShd:
	  wvApplysprmTSetShdNew (&apap->ptap, pointer, pos, 0);
	  break;
      case sprmTSetShdOdd:
	  wvApplysprmTSetShdNew (&apap->ptap, pointer, pos, 1);
	  break;
      case sprmTTextFlow:
	  wvApplysprmTTextFlow (&apap->ptap, pointer, pos);
	  break;
      case sprmTVertMerge:
	  wvApplysprmTVertMerge (&apap->ptap, pointer, pos);
	  break;
      case sprmTVertAlign:
	  wvApplysprmTVertAlign (&apap->ptap, pointer, pos);
	  break;
      case sprmTCellPadding:
	  wvApplysprmTCellPadding (&apap->ptap, pointer, pos, 0);
	  break;
      case sprmTCellSpacingDefault:
	  wvApplysprmTCellSpacing (&apap->ptap, pointer, pos);
	  apap->ptap.fCellSpacing = 1;
	  break;
      case sprmTCellPaddingDefault:
      case sprmTCellPaddingStyle:
	  wvApplysprmTCellPadding (&apap->ptap, pointer, pos, 1);
	  break;
      case sprmTCellWidth:
	  wvApplysprmTCellWidth (&apap->ptap, pointer, pos);
	  break;
      case sprmTFitText:
	  wvApplysprmTFitText (&apap->ptap, pointer, pos);
	  break;
      case sprmTFCellNoWrap:
	  wvApplysprmTFlagRange (&apap->ptap, pointer, pos, 0);
	  break;
      case sprmTCellFHideMark:
	  wvApplysprmTFlagRange (&apap->ptap, pointer, pos, 1);
	  break;
      case sprmTSetShdTable:
	  wvApplysprmTSetShdTable (ver, &apap->ptap, pointer, pos);
	  break;
      case sprmTCellBrcType:
	  wvApplysprmTCellBrcType (&apap->ptap, pointer, pos);
	  break;
      case sprmTBrcTopCv:
	  wvApplysprmTBrcCv (&apap->ptap, pointer, pos, 0);
	  break;
      case sprmTBrcLeftCv:
	  wvApplysprmTBrcCv (&apap->ptap, pointer, pos, 1);
	  break;
      case sprmTBrcBottomCv:
	  wvApplysprmTBrcCv (&apap->ptap, pointer, pos, 2);
	  break;
      case sprmTBrcRightCv:
	  wvApplysprmTBrcCv (&apap->ptap, pointer, pos, 3);
	  break;
      case sprmTIpgp:
      case sprmTRsid:
	  bread_32ubit (pointer, pos);
	  break;
      case sprmTPropRMark:
      case sprmTCnf:
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmTCellVertAlignStyle:
      case sprmTCellNoWrapStyle:
      case sprmTCHorzBands:
      case sprmTCVertBands:
	  bread_8ubit (pointer, pos);
	  break;
      case sprmTUNKNOWN1:
	  /* read wv.h and word 6 sprm 204
	     further down in this file to understand this
	   */
	  bread_8ubit (pointer, pos);
	  bread_16ubit (pointer, pos);
	  break;

	  /* end of TAP */

	  /*
	     case sprmPicBrcl
	   */

      case sprmPRuler:		/* ???? variable-length; skip it safely */
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      case sprmNoop:		/* no operand */
	  break;
      default:
	wvTrace(("unknown sprm: %d\n", sprm));
	  wvEatSprm (sprm, pointer, NULL, pos);
	  break;
      }

    wvGetSprmFromU16 (&RetSprm, sprm);
    return (RetSprm);
}

void
wvApplysprmPIstdPermute (PAP * apap, U8 * pointer, U16 * pos)
{
    U8 cch;
    U8 fLongg;
    U8 fSpare;
    U16 istdFirst;
    U16 istdLast;
    U16 *rgistd;
    U16 i;

    cch = dread_8ubit (NULL, &pointer);
    (*pos)++;
    fLongg = dread_8ubit (NULL, &pointer);
    (*pos)++;
    fSpare = dread_8ubit (NULL, &pointer);
    (*pos)++;
    istdFirst = dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    istdLast = dread_16ubit (NULL, &pointer);
    (*pos) += 2;

    if ( cch > 6)
      {
	  rgistd = (U16 *) wvMalloc (sizeof (U16) * ((cch - 6) / 2));
	  if (rgistd == NULL)
	    {
		wvError (
			 ("Could not allocate %d\n",
			  sizeof (U16) * ((cch - 6) / 2)));
		return;
	    }
	  for (i = 0; i < (cch - 6) / 2; i++)
	    {
		rgistd[i] = dread_16ubit (NULL, &pointer);
		(*pos) += 2;
	    }
      }
    else
      return;

    /*
       First check if pap.istd is greater than the istdFirst recorded in the sprm
       and less than or equal to the istdLast recorded in the sprm If not, the sprm
       has no effect. If it is, pap.istd is set to rgistd[pap.istd - istdFirst]
     */

    /* AbiWord: istdLast is file-controlled and can claim more entries
       than cch actually supplied -- never index past rgistd */
    if ((apap->istd > istdFirst) && (apap->istd <= istdLast)
	&& ((U32) (apap->istd - istdFirst) < (U32) ((cch - 6) / 2)))
      {
	  wvTrace (("%d %d %d\n", apap->istd, istdFirst, istdLast));
	  apap->istd = rgistd[apap->istd - istdFirst];
      }
    wvFree (rgistd);
}

void
wvApplysprmPIncLvl (PAP * apap, U8 * pointer, U16 * pos)
{
    U8 temp8;
    S8 tempS8;
    temp8 = bread_8ubit (pointer, pos);
    /*
       If pap.stc is < 1 or > 9, sprmPIncLvl has no effect. Otherwise, if the value
       stored in the byte has its highest order bit off, the value is a positive
       difference which should be added to pap.istd and pap.lvl and then pap.stc
       should be set to min(pap.istd, 9). If the byte value has its highest order
       bit on, the value is a negative difference which should be sign extended to
       a word and then subtracted from pap.istd and pap.lvl. Then pap.stc should be
       set to max(1, pap.istd).

       Now... hang on a sec coz

       Note that the storage and behavior of styles has changed radically since
       Word 2 for Windows, beginning with nFib 63. Some of the differences are:
       <chomp>
       * The style code is called an istd, rather than an stc.

       So, for the purposes of this filter, we ignore the stc component of the
       instructions

     */

    if ((apap->istd < 1) || (apap->istd > 9))
	return;

    if ((temp8 & 0x80) >> 7 == 0)
      {
	  apap->istd += temp8;
	  apap->lvl += temp8;
	  /*
	     apap->stc = min(apap->istd, 9);
	   */
      }
    else
      {
	  tempS8 = (S8) temp8;
	  apap->istd += tempS8;
	  apap->lvl += tempS8;
	  /*
	     apap->stc = max(1, apap->istd);
	   */
      }
}

void
wvApplysprmPChgTabsPapx (PAP * apap, U8 * pointer, U16 * pos)
{
    S16 temp_rgdxaTab[itbdMax];
    TBD temp_rgtbd[itbdMax];
    int i, j, k = 0, oldpos;
    U8 cch, itbdDelMax;
    S16 *rgdxaDel;
    U8 itbdAddMax;
    S16 *rgdxaAdd;
    int add = 0;
    TBD *rgtbdAdd;
    U32 rem;

    oldpos = *pos;
    cch = dread_8ubit (NULL, &pointer);
    (*pos)++;
    /* cch counts the operand bytes that follow it; the del/add counts
       of a corrupt operand must not drive reads past it */
    rem = cch;

    if (rem == 0)
	itbdDelMax = 0;
    else
      {
	  itbdDelMax = dread_8ubit (NULL, &pointer);
	  (*pos)++;
	  rem--;
      }
    if ((U32) itbdDelMax * 2 > rem)
	itbdDelMax = (U8) (rem / 2);
    if (itbdDelMax != 0)
      {
	  rgdxaDel = (S16 *) wvMalloc (sizeof (U16) * itbdDelMax);
	  for (i = 0; i < itbdDelMax; i++)
	    {
		rgdxaDel[i] = (S16) dread_16ubit (NULL, &pointer);
		(*pos) += 2;
	    }
	  rem -= (U32) itbdDelMax * 2;
      }
    else
	rgdxaDel = NULL;
    if (rem == 0)
	itbdAddMax = 0;
    else
      {
	  itbdAddMax = dread_8ubit (NULL, &pointer);
	  wvTrace (("itbdAddMax is %d\n", itbdAddMax));
	  (*pos)++;
	  rem--;
      }
    /* each add entry costs 2 rgdxa bytes + 1 TBD byte */
    if ((U32) itbdAddMax * 3 > rem)
	itbdAddMax = (U8) (rem / 3);
    if (itbdAddMax != 0)
      {
	  rgdxaAdd = (S16 *) wvMalloc (sizeof (U16) * itbdAddMax);
	  for (i = 0; i < itbdAddMax; i++)
	    {
		rgdxaAdd[i] = (S16) dread_16ubit (NULL, &pointer);
		wvTrace (("stops are %d\n", rgdxaAdd[i]));
		(*pos) += 2;
	    }
	  rem -= (U32) itbdAddMax * 2;
	  rgtbdAdd = (TBD *) wvMalloc (itbdAddMax * sizeof (TBD));
	  for (i = 0; i < itbdAddMax; i++)
	    {
		wvGetTBDFromBucket (&rgtbdAdd[i], pointer);
		pointer++;
		(*pos)++;
	    }
      }
    else
      {
	  rgdxaAdd = NULL;
	  rgtbdAdd = NULL;
      }

    /* the sprm occupies exactly cch+1 bytes no matter what the counts
       claimed -- keep the caller's sprm walk in sync */
    *pos = oldpos + cch + 1;

    /*
       When sprmPChgTabsPapx is interpreted, the rgdxaDel of the sprm is applied
       first to the pap that is being transformed. This is done by deleting from
       the pap the rgdxaTab entry and rgtbd entry of any tab whose rgdxaTab value
       is equal to one of the rgdxaDel values in the sprm. It is guaranteed that
       the entries in pap.rgdxaTab and the sprm's rgdxaDel and rgdxaAdd are
       recorded in ascending dxa order.

       Then the rgdxaAdd and rgtbdAdd entries are merged into the pap's rgdxaTab
       and rgtbd arrays so that the resulting pap rgdxaTab is sorted in ascending
       order with no duplicates.
     */
    for (j = 0; j < apap->itbdMac && k < itbdMax; j++)
      {
	  add = 1;
	  for (i = 0; i < itbdDelMax; i++)
	    {
		if (rgdxaDel[i] == apap->rgdxaTab[j])
		  {
		      add = 0;
		      break;
		  }
	    }
	  if (add)
	    {
		temp_rgdxaTab[k] = apap->rgdxaTab[j];
		wvCopyTBD (&temp_rgtbd[k++], &apap->rgtbd[j]);
	    }
      }
    /*temp_rgdxaTab now contains all the tab stops to be retained after the delete */
    apap->itbdMac = k;
    k = 0;
    j = 0;
    i = 0;
    /* a corrupt itbdAddMax can push the merged count past itbdMax; the
       pap's rgdxaTab/rgtbd only hold itbdMax entries, so stop there */
    while (((j < apap->itbdMac) || (i < itbdAddMax)) && (k < itbdMax))
      {
#if 0
	  wvTrace (("i %d j apap->itbdMac %d %d\n", i, j, apap->itbdMac));
	  wvTrace (("temp_rgdxaTab[j] %d\n", temp_rgdxaTab[j]));
	  wvTrace (("rgdxaAdd[i] %d\n", rgdxaAdd[i]));
#endif
	  if ((j < apap->itbdMac)
	      && (i >= itbdAddMax || temp_rgdxaTab[j] < rgdxaAdd[i]))
	    {
		/* if we have one from the retained group that should be added */
		apap->rgdxaTab[k] = temp_rgdxaTab[j];
		wvCopyTBD (&apap->rgtbd[k++], &temp_rgtbd[j++]);
	    }
	  else if ((j < apap->itbdMac) && (temp_rgdxaTab[j] == rgdxaAdd[i]))
	    {
		/* if we have one from the retained group that should be added
		   which is the same as one from the new group */
		apap->rgdxaTab[k] = rgdxaAdd[i];
		wvCopyTBD (&apap->rgtbd[k++], &rgtbdAdd[i++]);
		j++;
	    }
	  else			/*if (i < itbdAddMax) */
	    {
		/* if we have one from the new group to be added */
		apap->rgdxaTab[k] = rgdxaAdd[i];
		wvCopyTBD (&apap->rgtbd[k++], &rgtbdAdd[i++]);
	    }
      }
    wvTrace (("k is %d\n", k));

    apap->itbdMac = k;

    for (i = 0; i < apap->itbdMac; i++)
      {
	  wvTrace (
		   ("tab %d rgdxa %d %x\n", i, apap->rgdxaTab[i],
		    apap->rgdxaTab[i]));
      }

    wvFree (rgtbdAdd);
    wvFree (rgdxaAdd);
    wvFree (rgdxaDel);
}

int
wvApplysprmPChgTabs (PAP * apap, U8 * pointer, U16 * pos)
{
    S16 temp_rgdxaTab[itbdMax];
    TBD temp_rgtbd[itbdMax];
    U8 cch;
    U8 itbdDelMax;
    S16 *rgdxaDel;
    S16 *rgdxaClose;
    U8 itbdAddMax;
    S16 *rgdxaAdd;
    TBD *rgtbdAdd;
    int add = 0;
    int retlen;
    U8 i, j, k = 0;
    U32 rem;
    int oldpos;

    wvTrace (("entering wvApplysprmPChgTabs\n"));
    /*
       itbdDelMax and itbdAddMax are defined to be equal to 50. This means that the
       largest possible instance of sprmPChgTabs is 354. When the length of the
       sprm is greater than or equal to 255, the cch field will be set equal to
       255. When cch == 255, the actual length of the sprm can be calculated as
       follows: length = 2 + itbdDelMax * 4 + itbdAddMax * 3.
     */

    cch = dread_8ubit (NULL, &pointer);
    wvTrace (("cch is %d\n", cch));
    (*pos)++;
    oldpos = *pos;

    /* bound every internal count: for cch < 255 by the declared
       operand payload, for cch == 255 only by itbdMax (the counts
       define the length then; the caller measured the operand against
       the buffer already, and the pap cannot merge more than itbdMax
       stops anyway) */
    rem = (cch == 255) ? 0xffffffffU : (U32) cch;

    if (rem == 0)
	itbdDelMax = 0;
    else
      {
	  itbdDelMax = dread_8ubit (NULL, &pointer);
	  (*pos)++;
	  rem--;
      }
    if (itbdDelMax > itbdMax)
	itbdDelMax = itbdMax;
    /* each del entry costs 2 rgdxa + 2 rgdxaClose bytes */
    if ((U32) itbdDelMax * 4 > rem)
	itbdDelMax = (U8) (rem / 4);

    wvTrace (("itbdDelMax is %d\n", itbdDelMax));
    if (itbdDelMax != 0)
      {
	  rgdxaDel = (S16 *) wvMalloc (sizeof (S16) * itbdDelMax);
	  rgdxaClose = (S16 *) wvMalloc (sizeof (S16) * itbdDelMax);
	  for (i = 0; i < itbdDelMax; i++)
	    {
		rgdxaDel[i] = (S16) dread_16ubit (NULL, &pointer);
		(*pos) += 2;
	    }
	  for (i = 0; i < itbdDelMax; i++)
	    {
		rgdxaClose[i] = dread_16ubit (NULL, &pointer);
		(*pos) += 2;
	    }
	  rem -= (U32) itbdDelMax * 4;
      }
    else
      {
	  rgdxaDel = NULL;
	  rgdxaClose = NULL;
      }
    if (rem == 0)
	itbdAddMax = 0;
    else
      {
	  itbdAddMax = dread_8ubit (NULL, &pointer);
	  (*pos)++;
	  rem--;
      }
    if (itbdAddMax > itbdMax)
	itbdAddMax = itbdMax;
    /* each add entry costs 2 rgdxa + 1 TBD bytes */
    if ((U32) itbdAddMax * 3 > rem)
	itbdAddMax = (U8) (rem / 3);
    wvTrace (("itbdAddMax is %d\n", itbdAddMax));
    if (itbdAddMax != 0)
      {
	  rgdxaAdd = (S16 *) wvMalloc (sizeof (S16) * itbdAddMax);
	  rgtbdAdd = (TBD *) wvMalloc (itbdAddMax * sizeof (TBD));
	  for (i = 0; i < itbdAddMax; i++)
	    {
		rgdxaAdd[i] = (S16) dread_16ubit (NULL, &pointer);
		wvTrace (("rgdxaAdd %d is %x\n", i, rgdxaAdd[i]));
		(*pos) += 2;
	    }
	  for (i = 0; i < itbdAddMax; i++)
	    {
		wvGetTBDFromBucket (&rgtbdAdd[i], pointer);
		pointer++;
		(*pos)++;
	    }
      }
    else
      {
	  rgdxaAdd = NULL;
	  rgtbdAdd = NULL;
      }

    /* when cch == 255 the real operand length is computed from the
       counts; it can exceed 255, so keep it in an int (storing it back
       into the U8 cch would wrap) */
    retlen = cch;
    if (cch == 255)
	retlen = 2 + itbdDelMax * 4 + itbdAddMax * 3;

    /* the sprm occupies exactly retlen+1 bytes no matter what the
       counts claimed -- keep the caller's sprm walk in sync */
    *pos = (U16) (oldpos + retlen);

    /*
       When sprmPChgTabs is interpreted, the rgdxaDel of the sprm is applied first
       to the pap that is being transformed. This is done by deleting from the pap
       the rgdxaTab entry and rgtbd entry of any tab whose rgdxaTab value is within
       the interval [rgdxaDel[i] - rgdxaClose[i], rgdxaDel[i] + rgdxaClose[i]] It
       is guaranteed that the entries in pap.rgdxaTab and the sprm's rgdxaDel and
       rgdxaAdd are recorded in ascending dxa order.

       Then the rgdxaAdd and rgtbdAdd entries are merged into the pap's rgdxaTab
       and rgtbd arrays so that the resulting pap rgdxaTab is sorted in ascending
       order with no duplicates.
     */
    if (apap == NULL)
      {
	  wvFree (rgdxaDel);
	  wvFree (rgtbdAdd);
	  wvFree (rgdxaAdd);
	  wvFree (rgdxaClose);
	  return (retlen);
      }

    wvTrace (("here %d\n", apap->itbdMac));
    for (j = 0; j < apap->itbdMac && k < itbdMax; j++)
      {
	  add = 1;
	  for (i = 0; i < itbdDelMax; i++)
	    {
		wvTrace (
			 ("examing %x against %x\n", apap->rgdxaTab[j],
			  rgdxaDel[i]));
		if ((apap->rgdxaTab[j] >= rgdxaDel[i] - rgdxaClose[i])
		    && (apap->rgdxaTab[j] <= rgdxaDel[i] + rgdxaClose[i]))
		  {
		      wvTrace (("deleting\n"));
		      add = 0;
		      break;
		  }
	    }
	  if (add)
	    {
		temp_rgdxaTab[k] = apap->rgdxaTab[j];
		wvCopyTBD (&temp_rgtbd[k++], &apap->rgtbd[j]);
	    }
      }
    apap->itbdMac = k;
    wvTrace (("here %d\n", apap->itbdMac));

    k = 0;
    j = 0;
    i = 0;
    /* a corrupt itbdAddMax can push the merged count past itbdMax; the
       pap's rgdxaTab/rgtbd only hold itbdMax entries, so stop there */
    while (((j < apap->itbdMac) || (i < itbdAddMax)) && (k < itbdMax))
      {
	  if ((j < apap->itbdMac)
	      && (i >= itbdAddMax || temp_rgdxaTab[j] < rgdxaAdd[i]))
	    {
		wvTrace (("adding from nondeleted tab stops\n"));
		apap->rgdxaTab[k] = temp_rgdxaTab[j];
		wvCopyTBD (&apap->rgtbd[k++], &temp_rgtbd[j++]);
	    }
	  else if ((j < apap->itbdMac) && (temp_rgdxaTab[j] == rgdxaAdd[i]))
	    {
		wvTrace (("adding from new tab stops\n"));
		apap->rgdxaTab[k] = rgdxaAdd[i];
		wvCopyTBD (&apap->rgtbd[k++], &rgtbdAdd[i++]);
		j++;
	    }
	  else			/*if (i < itbdAddMax) */
	    {
		wvTrace (("adding from new tab stops\n"));
		apap->rgdxaTab[k] = rgdxaAdd[i];
		wvCopyTBD (&apap->rgtbd[k++], &rgtbdAdd[i++]);
	    }
      }

    apap->itbdMac = k;
    wvTrace (("here %d\n", apap->itbdMac));

    for (i = 0; i < apap->itbdMac; i++)
      {
	  wvTrace (
		   ("tab %d rgdxa %d %x\n", i, apap->rgdxaTab[i],
		    apap->rgdxaTab[i]));
      }

    wvFree (rgdxaDel);
    wvFree (rgtbdAdd);
    wvFree (rgdxaAdd);
    wvFree (rgdxaClose);
    wvTrace (("Exiting Successfully\n"));

    return (retlen);
}

void
wvApplysprmPPc (PAP * apap, U8 * pointer, U16 * pos)
{
    U8 temp8;
    struct _temp {
	U32 reserved:4;
	U32 pcVert:2;
	U32 pcHorz:2;
    } temp;


    temp8 = bread_8ubit (pointer, pos);
#ifdef PURIFY
    temp.pcVert = 0;
    temp.pcHorz = 0;
#endif
    temp.pcVert = (temp8 & 0x0C) >> 4;
    temp.pcHorz = (temp8 & 0x03) >> 6;

    /*
       sprmPPc is interpreted by moving pcVert to pap.pcVert if pcVert != 3 and by
       moving pcHorz to pap.pcHorz if pcHorz != 3.
     */

    if (temp.pcVert != 3)
	apap->pcVert = temp.pcVert;
    if (temp.pcHorz != 3)
	apap->pcHorz = temp.pcHorz;
}

void
wvApplysprmPFrameTextFlow (PAP * apap, U8 * pointer, U16 * pos)
{
    U16 temp16 = bread_16ubit (pointer, pos);

    apap->fVertical = temp16 & 0x0001;
    apap->fBackward = (temp16 & 0x0002) >> 1;
    apap->fRotateFont = (temp16 & 0x0004) >> 2;
}

void
wvApplysprmPAnld (wvVersion ver, PAP * apap, U8 * pointer, U16 * pos)
{
    dread_8ubit (NULL, &pointer);
    (*pos)++;
    wvGetANLD_FromBucket (ver, &apap->anld, pointer);
    if (ver == WORD8)
	(*pos) += cbANLD;
    else
	(*pos) += cb6ANLD;
}

void
wvApplysprmPPropRMark (PAP * apap, U8 * pointer, U16 * pos)
{
    dread_8ubit (NULL, &pointer);
    /*
       sprmPPropRMark is interpreted by moving the first parameter
       byte to pap.fPropRMark, the next two bytes to pap.ibstPropRMark, and the
       remaining four bytes to pap.dttmPropRMark.
     */
    apap->fPropRMark = dread_8ubit (NULL, &pointer);
    (*pos)++;
    apap->ibstPropRMark = dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    wvGetDTTMFromBucket (&apap->dttmPropRMark, pointer);
    (*pos) += 4;
}

void
wvApplysprmPNumRM (PAP * apap, U8 * pointer, U16 * pos)
{
    dread_8ubit (NULL, &pointer);
    (*pos)++;
    wvGetNUMRMFromBucket (&apap->numrm, pointer);
    (*pos) += cbNUMRM;
}

void
wvApplysprmPHugePapx (PAP * apap, U8 * pointer, U16 * pos, wvStream * data,
		      STSH * stsh)
{
    U32 offset;
    U16 len, i, sprm;
    U8 *grpprl, *pointer2;
    /*
       sprmPHugePapx is stored in PAPX FKPs in place of the grpprl of a PAPX which
       would otherwise be too big to fit in an FKP (as of this writing, 488 bytes
       is the size of the largest PAPX which can fit in an FKP). The parameter fc
       gives the location of the grpprl in the data stream. The first word at that
       fc counts the number of bytes in the grpprl (not including the byte count
       itself). A sprmPHugePapx should therefore only be found in a PAPX FKP and
       should be the only sprm in that PAPX's grpprl.
     */
    offset = dread_32ubit (NULL, &pointer);
    (*pos) += 4;
    wvTrace (("Offset is %x in data stream\n", offset));
    if (!(data))
      {
	  wvError (("No data stream!!\n"));
	  return;
      }
    if (0 > wvStream_goto (data, offset))
      {
	  wvError (("Couldn't seek data stream!!\n"));
	  apap->fTtp++;
	  return;
      }
    len = read_16ubit (data);
    if (!len)
      {
	  wvWarning ("sprmPHugePapx len is 0, seems unlikely\n");
	  return;
      }

    grpprl = (U8 *) wvMalloc (len);

    for (i = 0; i < len; i++)
	grpprl[i] = read_8ubit (data);

    i = 0;
    while (i + 2 <= len)
      {
	  U16 scratch;
	  int oplen;
	  sprm = bread_16ubit (grpprl + i, &i);
#ifdef SPRMTEST
	  wvError (("sprm is %x\n", sprm));
#endif
	  pointer2 = grpprl + i;
	  /* reject operands that would run past the end of the grpprl */
	  scratch = i;
	  oplen = wvEatSprm (sprm, pointer2, grpprl + len, &scratch);
	  if ((U32) i + (U32) oplen > (U32) len)
	      break;
	  wvApplySprmFromBucket (WORD8, sprm, apap, NULL, NULL, stsh,
				 pointer2, &i, data);
      }
    wvFree (grpprl);
}

void
wvApplysprmCChs (CHP * achp, U8 * pointer, U16 * pos)
{
    /*
       When this sprm is interpreted, the first byte of the operand is moved to
       chp.fChsDiff and the remaining word is moved to chp.chse.
     */
    achp->fChsDiff = dread_8ubit (NULL, &pointer);
    (*pos)++;
    /*achp->chse ???? */
    /* the doc says to set this, but it doesnt exist anywhere else in the docs */
    dread_16ubit (NULL, &pointer);
    (*pos) += 2;
}

void
wvApplysprmCSymbol (wvVersion ver, CHP * achp, U8 * pointer, U16 * pos)
{
    if (ver == WORD8)
      {
	  /*
	     Word 8
	     This sprm's operand is 4 bytes. The first 2 hold the font code; the last 2
	     hold a character specifier. When this sprm is interpreted, the font code is
	     moved to chp.ftcSym and the character specifier is moved to chp.xchSym and
	     chp.fSpec is set to 1.
	   */
	  achp->ftcSym = dread_16ubit (NULL, &pointer);
	  (*pos) += 2;
	  achp->xchSym = dread_16ubit (NULL, &pointer);
	  (*pos) += 2;
	  wvTrace (("%d %d\n", achp->ftcSym, achp->xchSym));
      }
    else
      {
	  /*
	     Word 6 and 7
	     The length byte recorded at offset 1 in this
	     sprm will always be 3. When this sprm is interpreted the two byte
	     font code recorded at offset 2 is moved to chp.ftcSym, the single
	     byte character specifier recorded at offset 4 is moved to chp.chSym
	     and chp.fSpec is set to 1.
	   */
	  dread_8ubit (NULL, &pointer);
	  (*pos)++;
	  achp->ftcSym = dread_16ubit (NULL, &pointer);
	  (*pos) += 2;
	  achp->xchSym = dread_8ubit (NULL, &pointer);
	  achp->xchSym += 61440;	/* promote this char into a unicode char to
					   be consistent with what word 8 does */
	  (*pos)++;
      }
    achp->fSpec = 1;
}

void
wvApplysprmCIstdPermute (CHP * achp, U8 * pointer, U16 * pos)
{
    U8 cch;
    U8 fLongg;
    U8 fSpare;
    U16 istdFirst;
    U16 istdLast;
    U16 *rgistd;
    U16 i;

    cch = dread_8ubit (NULL, &pointer);
    (*pos)++;
    fLongg = dread_8ubit (NULL, &pointer);
    (*pos)++;
    fSpare = dread_8ubit (NULL, &pointer);
    (*pos)++;
    istdFirst = dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    istdLast = dread_16ubit (NULL, &pointer);
    (*pos) += 2;

    if (cch > 6)
      {
	  rgistd = (U16 *) wvMalloc (sizeof (U16) * ((cch - 6) / 2));
	  for (i = 0; i < (cch - 6) / 2; i++)
	    {
		rgistd[i] = dread_16ubit (NULL, &pointer);
		(*pos) += 2;
	    }
      }
    else
	rgistd = NULL;
    /*
       first check if chp.istd is greater than the
       istdFirst recorded in the sprm and less than or equal to the istdLast
       recorded in the sprm If not, the sprm has no effect. If it is, chp.istd is
       set to rgstd[chp.istd - istdFirst] and any chpx stored in that rgstd entry
       is applied to the chp.

       Note that it is possible that an istd may be recorded in the rgistd that
       refers to a paragraph style. This will no harmful consequences since the
       istd for a paragraph style should never be recorded in chp.istd.
     */

    /* AbiWord: rgistd is NULL when cch <= 6, and istdLast is
       file-controlled -- bound the index to the entries present */
    if (rgistd && (achp->istd > istdFirst) && (achp->istd <= istdLast)
	&& ((U32) (achp->istd - istdFirst) < (U32) ((cch - 6) / 2)))
      {
	  achp->istd = rgistd[achp->istd - istdFirst];
	  /*
	     if really a chp style
	     wvAddCHPXFromUPEBucket(achp,&(stsh->std[achp->istd].grupe[0].chpx),stsh);
	     else
	     complain;
	   */
      }
    wvFree (rgistd);
}

void
wvApplysprmCDefault (CHP * achp, U8 * pointer, U16 * pos)
{
    /*
       sprmCDefault (opcode 0x2A32) clears the fBold, fItalic, fOutline, fStrike,
       fShadow, fSmallCaps, fCaps, fVanish, kul and ico fields of the chp to 0. It
       was first defined for Word 3.01 and had to be backward compatible with Word
       3.00 so it is a variable length sprm whose count of bytes is 0. It consists
       of the sprmCDefault opcode followed by a byte of 0.
     */
    dread_8ubit (NULL, &pointer);
    (*pos)++;
    achp->fBold = 0;
    achp->fItalic = 0;
    achp->fOutline = 0;
    achp->fStrike = 0;
    achp->fShadow = 0;
    achp->fSmallCaps = 0;
    achp->fCaps = 0;
    achp->fVanish = 0;
    achp->kul = 0;
    achp->ico = 0;
}

void
wvApplysprmCFtcDefault (CHP * achp, STSH * stsh, U8 * pointer, U16 * pos)
{
    CHP ctemp;

    /* sprmCFtcDefault (0x4A3D): the 2-byte operand is unused; the sprm
       restores the chp's font indices to the underlying style's values
       (the "Default Paragraph Font" behaviour) */
    bread_16ubit (pointer, pos);
    wvInitCHPFromIstd (&ctemp, achp->istd, stsh);
    achp->ftcAscii = ctemp.ftcAscii;
    achp->ftcFE = ctemp.ftcFE;
    achp->ftcOther = ctemp.ftcOther;
    achp->ftcBidi = ctemp.ftcBidi;
}

void
wvApplysprmCPlain (CHP * achp, STSH * stsh)
{
    U8 fSpec;
    /*
       the style sheet CHP is copied over the original CHP preserving the
       fSpec setting from the original CHP.
     */
    fSpec = achp->fSpec;
    wvInitCHPFromIstd (achp, achp->istd, stsh);
    achp->fSpec = fSpec;
}


U8
wvToggle (U8 in, U8 toggle)
{
    /*
       When the parameter of the sprm is set to 0 or 1, then
       the CHP property is set to the parameter value.
     */
    if ((toggle == 0) || (toggle == 1))
	return (toggle);
    /*
       When the parameter of the sprm is 128, then the CHP property is set to the
       value that is stored for the property in the style sheet. CHP When the
       parameter of the sprm is 129, the CHP property is set to the negation of the
       value that is stored for the property in the style sheet CHP.
     */

    /*
       an argument might be made that instead of in being returned or negated that
       it should be the looked up in the original chp through the istd that should
       be used, in which case this should be a macro that does the right thing.
       but im uncertain as to which is the correct one to do, ideas on a postcard
       to... etc etc
     */
    if (toggle == 128)
	return (in);
    else if (toggle == 129)
	return (!in);
    wvWarning ("Strangle sprm toggle value, ignoring\n");
    return (in);
}

void
wvApplysprmCSizePos (CHP * achp, U8 * pointer, U16 * pos)
{
    U8 prevhpsPos;
    U16 temp8;
    struct _temp {
	U32 hpsSize:8;
	U32 cInc:7;
	U32 fAdjust:1;
	U32 hpsPos:8;
    } temp;
    temp.hpsSize = dread_8ubit (NULL, &pointer);
    (*pos)++;
    temp8 = dread_8ubit (NULL, &pointer);
    (*pos)++;
    temp.cInc = (temp8 & 0x7f) >> 8;
    temp.fAdjust = (temp8 & 0x80) >> 7;
    temp.hpsPos = dread_8ubit (NULL, &pointer);
    (*pos)++;

    /*
       if hpsSize != 0 then chp.hps is set to hpsSize.

       If cInc is != 0, the cInc is interpreted as a 7 bit twos complement
       number and the procedure described below for interpreting sprmCHpsInc is
       followed to increase or decrease the chp.hps by the specified number of
       levels.

       If hpsPos is != 128, then chp.hpsPos is set equal to hpsPos.

       If fAdjust is on , hpsPos != 128 and hpsPos != 0 and the previous value of
       chp.hpsPos == 0, then chp.hps is reduced by one level following the method
       described for sprmCHpsInc.

       If fAdjust is on, hpsPos == 0 and the previous value of chp.hpsPos != 0,
       then the chp.hps value is increased by one level using the method described
       below for sprmCHpsInc.
     */

    if (temp.hpsSize != 0)
	achp->hps = temp.hpsSize;

    if (temp.cInc != 0)
      {
      }

    prevhpsPos = achp->hpsPos;

    if (temp.hpsPos != 128)
	achp->hpsPos = temp.hpsPos;
#if 0
    /*else ? who knows ? */
    if ((temp.fAdjust) && (temp.hpsPos != 128) && (temp.hpsPos != 0)
	&& (achp->hpsPos == 0))
	/*reduce level */ ;
    if ((temp.fAdjust) && (temp.hpsPos == 0) && (achp->hpsPos != 0))
	/*increase level */ ;
#endif

    /*
       This depends on an implementation of sprmCHpsInc, read wvApplysprmCHpsInc for
       some comments on the whole matter
     */

    wvError (
	     ("This document has an unsupported sprm (sprmCSizePos), please mail "));
    wvError (
	     ("Caolan.McNamara@ul.ie with this document, as i haven't been able to "));
    wvError (("get any examples of it so as to figure out how to handle it\n"));

}

void
wvApplysprmCHpsInc (CHP * achp, U8 * pointer, U16 * pos)
{
    U8 param;
    /*
       sprmCHpsInc(opcode 0x2A44) is a three-byte sprm consisting of the sprm
       opcode and a one-byte parameter.

       Word keeps an ordered array of the font sizes that are defined for the fonts
       recorded in the system file with each font size transformed into an hps.

       The parameter is a one-byte twos complement number. Word uses this number
       to calculate an index in the font size array to determine the new hps for a
       run. When Word interprets this sprm and the parameter is positive, it searches
       the array of font sizes to find the index of the smallest entry in the font
       size table that is greater than the current chp.hps.It then adds the
       parameter minus 1 to the index and maxes this with the index of the last array
       entry. It uses the result as an index into the font size array and assigns that
       entry of the array to chp.hps.

       When the parameter is negative, Word searches the array of font sizes to
       find the index of the entry that is less than or equal to the current
       chp.hps. It then adds the negative parameter to the index and does a min of
       the result with 0. The result of the min function is used as an index into
       the font size array and that entry of the array is assigned to chp.hps.
       sprmCHpsInc is stored only in grpprls linked to piece table entries.
     */

    wvError (
	     ("This document has an unsupported sprm (sprmCHpsInc), please mail"));
    wvError (
	     ("Caolan.McNamara@ul.ie with this document, as i haven't been able to "));
    wvError (("get any examples of it so as to figure out how to handle it\n"));

    param = dread_8ubit (NULL, &pointer);
    (*pos)++;

    /*
       Now for christ sake !!, how on earth would i have an "ordered array of the
       font sizes that are defined for the fonts recorded in the system file", that
       sounds to me that i would have to have access to the fonts on the actual
       machine that word was last run on !, it sounds to me that this sprm might only
       be used during the editing of a file, so im going to have to ignore it because
       it complete goobledegook to me
     */

}

void
wvApplysprmCHpsPosAdj (CHP * achp, U8 * pointer, U16 * pos)
{
    U8 param;
    /*
       sprmCHpsPosAdj (opcode 0x2A46) causes the hps of a run to be reduced the
       first time text is superscripted or subscripted and causes the hps of a run
       to be increased when superscripting/subscripting is removed from a run.

       The one byte parameter of this sprm is the new hpsPos value that is to be
       stored in chp.hpsPos.

       If the new hpsPos is not equal 0 (meaning that the text is to be super/
       subscripted), Word first examines the current value of chp.hpsPos
       to see if it is equal to 0.

       If so, Word uses the algorithm described for sprmCHpsInc to decrease chp.hps
       by one level.

       If the new hpsPos == 0 (meaning the text is not super/subscripted),
       Word examines the current chp.hpsPos to see if it is not equal to 0. If it is
       not (which means text is being restored to normal position), Word uses the
       sprmCHpsInc algorithm to increase chp.hps by one level.

       After chp.hps is adjusted, the parameter value is stored in chp.hpsPos.
     */

    wvError (
	     ("This document has an partially unsupported sprm (sprmCHpsPosAdj), please mail "));
    wvError (
	     ("Caolan.McNamara@ul.ie with this document, as i haven't been able to "));
    wvError (("get any examples of it so as to figure out how to handle it\n"));

    param = dread_8ubit (NULL, &pointer);
    (*pos)++;

    /*
       please see wvApplysprmCHpsInc for why this is unfinished
     */

#if 0
    if ((param != 0) && (achp->hpsPos == 0))
	/*decrease chp.hps */ ;
    else if ((param == 0) && (achp->hpsPos != 0))
	/*increase chp.hps */ ;
#endif

    achp->hpsPos = param;


}

void
wvApplysprmCMajority (CHP * achp, STSH * stsh, U8 * pointer, U16 * pos)
{
    U16 i;
    CHP base;
    CHP orig;
    UPXF upxf;
    /*
       Bytes 0 and 1 of
       sprmCMajority contains the opcode, byte 2 contains the length of the
       following list of character sprms. . Word begins interpretation of this sprm
       by applying the stored character sprm list to a standard chp. That chp has
       chp.istd = istdNormalChar. chp.hps=20, chp.lid=0x0400 and chp.ftc = 4. Word
       then compares fBold, fItalic, fStrike, fOutline, fShadow, fSmallCaps, fCaps,
       ftc, hps, hpsPos, kul, qpsSpace and ico in the original CHP with the values
       recorded for these fields in the generated CHP.. If a field in the original
       CHP has the same value as the field stored in the generated CHP, then that
       field is reset to the value stored in the style's CHP. If the two copies
       differ, then the original CHP value is left unchanged.
     */
    wvTrace (
	     ("This document has a sprm (sprmCMajority), that ive never seen in practice please mail "));
    wvTrace (
	     ("Caolan.McNamara@ul.ie with this document, as i haven't been able to "));
    wvTrace (
	     ("get any examples of it so as to figure out if its handled correctly\n"));

    wvInitCHP (&base);
    base.ftc = 4;

    /*generate a UPE and run wvAddCHPXFromBucket */

    upxf.cbUPX = dread_8ubit (NULL, &pointer);
    (*pos)++;
    upxf.upx.chpx.grpprl = (U8 *) wvMalloc (upxf.cbUPX);

    for (i = 0; i < upxf.cbUPX; i++)
      {
	  upxf.upx.chpx.grpprl[i] = dread_8ubit (NULL, &pointer);
	  (*pos)++;
      }

    wvTrace (("achp istd is %d\n", achp->istd));

    wvAddCHPXFromBucket (&base, &upxf, stsh);

    wvTrace (("achp istd is %d\n", achp->istd));

    wvTrace (("my underline started as %d\n", achp->kul));

    wvInitCHPFromIstd (&orig, achp->istd, stsh);

    /* this might be a little wrong, review after doing dedicated CHP's */
    if (achp->fBold == base.fBold)
	achp->fBold = orig.fBold;
    if (achp->fItalic == base.fItalic)
	achp->fItalic = orig.fItalic;
    if (achp->fStrike == base.fStrike)
	achp->fStrike = orig.fStrike;
    if (achp->fOutline == base.fOutline)
	achp->fOutline = orig.fOutline;
    if (achp->fShadow == base.fShadow)
	achp->fShadow = orig.fShadow;
    if (achp->fSmallCaps == base.fSmallCaps)
	achp->fSmallCaps = orig.fSmallCaps;
    if (achp->fCaps == base.fCaps)
	achp->fCaps = orig.fCaps;
    if (achp->ftc == base.ftc)
	achp->ftc = orig.ftc;
    if (achp->hps == base.hps)
	achp->hps = orig.hps;
    if (achp->hpsPos == base.hpsPos)
	achp->hpsPos = orig.hpsPos;
    if (achp->kul == base.kul)
	achp->kul = orig.kul;
    /* ????
       if (achp->qpsSpace == base.qpsSpace)
       achp->qpsSpace = orig.qpsSpace;
     */
    if (achp->ico == base.ico)
	achp->ico = orig.ico;

    /*
       these ones are mentioned in a different part of the spec, that
       doesnt have as much weight as the above, but i'm going to add them
       anyway
     */
    if (achp->fVanish == base.fVanish)
	achp->fVanish = orig.fVanish;
    wvTrace (("%d\n", base.dxaSpace));
    wvTrace (("%d\n", achp->dxaSpace));
    if (achp->dxaSpace == base.dxaSpace)
	achp->dxaSpace = orig.dxaSpace;
    if (achp->lidDefault == base.lidDefault)
	achp->lidDefault = orig.lidDefault;
    if (achp->lidFE == base.lidFE)
	achp->lidFE = orig.lidFE;
    wvFree (upxf.upx.chpx.grpprl);


    wvTrace (("my underline ended as %d\n", achp->kul));
}

void
wvApplysprmCHpsInc1 (CHP * achp, U8 * pointer, U16 * pos)
{
    /*
       This sprm is interpreted by adding the two byte increment
       stored as the opcode of the sprm to chp.hps. If this result is less than 8,
       the chp.hps is set to 8. If the result is greater than 32766, the chp.hps is
       set to 32766.
     */
    dread_8ubit (NULL, &pointer);
    (*pos)++;
    achp->hps += dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    if (achp->hps < 8)
	achp->hps = 8;
    else if (achp->hps > 32766)
	achp->hps = 32766;
}


void
wvApplysprmCMajority50 (CHP * achp, STSH * stsh, U8 * pointer, U16 * pos)
{
    U16 i;
    CHP base;
    CHP orig;
    UPXF upxf;
    /*
       Bytes 0 and 1 of
       sprmCMajority contains the opcode, byte 2 contains the length of the
       following list of character sprms. . Word begins interpretation of this sprm
       by applying the stored character sprm list to a standard chp. That chp has
       chp.istd = istdNormalChar. chp.hps=20, chp.lid=0x0400 and chp.ftc = 4. Word
       then compares fBold, fItalic, fStrike, fOutline, fShadow, fSmallCaps, fCaps,
       ftc, hps, hpsPos, kul, qpsSpace and ico in the original CHP with the values
       recorded for these fields in the generated CHP.. If a field in the original
       CHP has the same value as the field stored in the generated CHP, then that
       field is reset to the value stored in the style's CHP. If the two copies
       differ, then the original CHP value is left unchanged.
     */
    wvTrace (
	     ("This document has a sprm (sprmCMajority50), that ive never seen in practice please mail "));
    wvTrace (
	     ("Caolan.McNamara@ul.ie with this document, as i haven't been able to "));
    wvTrace (
	     ("get any examples of it so as to figure out if its handled correctly\n"));

    wvInitCHP (&base);
    base.ftc = 4;

    /*generate a UPE and run wvAddCHPXFromBucket */

    upxf.cbUPX = dread_8ubit (NULL, &pointer);
    (*pos)++;
    upxf.upx.chpx.grpprl = (U8 *) wvMalloc (upxf.cbUPX);

    for (i = 0; i < upxf.cbUPX; i++)
      {
	  upxf.upx.chpx.grpprl[i] = dread_8ubit (NULL, &pointer);
	  (*pos)++;
      }

    wvAddCHPXFromBucket (&base, &upxf, stsh);

    wvInitCHPFromIstd (&orig, achp->istd, stsh);

    /* this might be a little wrong, review after doing dedicated CHP's */
    wvTrace (("istd is %d\n", achp->istd));
    if (achp->fBold == base.fBold)
	achp->fBold = orig.fBold;
    if (achp->fItalic == base.fItalic)
	achp->fItalic = orig.fItalic;
    if (achp->fStrike == base.fStrike)
	achp->fStrike = orig.fStrike;
    if (achp->fSmallCaps == base.fSmallCaps)
	achp->fSmallCaps = orig.fSmallCaps;
    if (achp->fCaps == base.fCaps)
	achp->fCaps = orig.fCaps;
    if (achp->ftc == base.ftc)
	achp->ftc = orig.ftc;
    if (achp->hps == base.hps)
	achp->hps = orig.hps;
    if (achp->hpsPos == base.hpsPos)
	achp->hpsPos = orig.hpsPos;
    if (achp->kul == base.kul)
	achp->kul = orig.kul;
    if (achp->ico == base.ico)
	achp->ico = orig.ico;
    if (achp->fVanish == base.fVanish)
	achp->fVanish = orig.fVanish;
    if (achp->dxaSpace == base.dxaSpace)
	achp->dxaSpace = orig.dxaSpace;

    wvFree (upxf.upx.chpx.grpprl); /* this seemed to be missing... */
}

void
wvApplysprmCPropRMark (CHP * achp, U8 * pointer, U16 * pos)
{
    /* spra 6: cbGrpprl-style length byte followed by the operand.
       sprmCPropRMark (0xCA89) carries a PropRMark (7 bytes, incl DTTM);
       sprmCPropRMark90 (0xCA57) carries a PropRMark90 (3 bytes, no DTTM) */
    U16 len = dread_8ubit (NULL, &pointer);
    U16 end = *pos + 1 + len;
    (*pos)++;
    if (*pos < end)
      {
	  achp->fPropRMark = dread_8ubit (NULL, &pointer);
	  (*pos)++;
      }
    if (*pos + 2 <= end)
      {
	  achp->ibstPropRMark = (S16) dread_16ubit (NULL, &pointer);
	  (*pos) += 2;
      }
    if (*pos + 4 <= end)
      {
	  wvGetDTTMFromBucket (&achp->dttmPropRMark, pointer);
	  (*pos) += 4;
      }
    *pos = end;
}

void
wvApplysprmCDispFldRMark (CHP * achp, U8 * pointer, U16 * pos)
{
    /*
       is interpreted by moving the first
       parameter byte to chp.fDispFldRMark, the next two bytes to
       chp.ibstDispFldRMark, the next four bytes to chp.dttmDispFldRMark,
       and the remaining 32 bytes to chp.xstDispFldRMark.
     */

    int i;
    dread_8ubit (NULL, &pointer);	/*len */
    (*pos)++;
    achp->fDispFldRMark = dread_8ubit (NULL, &pointer);
    (*pos)++;
    achp->ibstDispFldRMark = (S16) dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    wvGetDTTMFromBucket (&achp->dttmDispFldRMark, pointer);
    (*pos) += 4;
    pointer += 4;		/* 4 unused bytes before the XCHAR array */
    (*pos) += 4;
    for (i = 0; i < 16; i++)
      {
	  achp->xstDispFldRMark[i] = dread_16ubit (NULL, &pointer);
	  (*pos) += 2;
      }
    /* AbiWord: the field is a fixed 16-XCHAR array; guarantee a NUL so
       later string conversion cannot scan off the end */
    achp->xstDispFldRMark[15] = 0;
}


void
wvApplysprmSOlstAnm (wvVersion ver, SEP * asep, U8 * pointer, U16 * pos)
{
    U8 len = dread_8ubit (NULL, &pointer);
    wvGetOLSTFromBucket (ver, &asep->olstAnm, pointer);
    if (len != cbOLST)
	wvError (("OLST len is different from expected\n"));
    (*pos) += len;
}

void
wvApplysprmSPropRMark (SEP * asep, U8 * pointer, U16 * pos)
{
    dread_8ubit (NULL, &pointer);
    (*pos)++;
    /*
       sprmPPropRMark is interpreted by moving the first parameter
       byte to pap.fPropRMark, the next two bytes to pap.ibstPropRMark, and the
       remaining four bytes to pap.dttmPropRMark.
     */
    asep->fPropRMark = dread_8ubit (NULL, &pointer);
    (*pos)++;
    asep->ibstPropRMark = dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    wvGetDTTMFromBucket (&asep->dttmPropRMark, pointer);
    (*pos) += 4;
}


/*
sprmTDxaLeft (opcode 0x9601) is called to adjust the x position within a
column which marks the left boundary of text within the first cell of a
table row. This sprm causes a whole table row to be shifted left or right
within its column leaving the horizontal width and vertical height of cells
in the row unchanged. Bytes 0-1 of the sprm contains the opcode, and the new
dxa position, call it dxaNew, is stored as an integer in bytes 2 and 3. Word
interprets this sprm by adding dxaNew - (rgdxaCenter[0] + tap.dxaGapHalf) to
every entry of tap.rgdxaCenter whose index is less than tap.itcMac.
sprmTDxaLeft is stored only in grpprls linked to piece table entries.
*/
void
wvApplysprmTDxaLeft (TAP * aTap, U8 * pointer, U16 * pos)
{
    S16 dxaNew = (S16) dread_16ubit (NULL, &pointer);
    int i;
    (*pos) += 2;
    dxaNew = dxaNew - (aTap->rgdxaCenter[0] + aTap->dxaGapHalf);
    for (i = 0; i < aTap->itcMac; i++)
	aTap->rgdxaCenter[i] += dxaNew;
}

/*
sprmTDxaGapHalf (opcode 0x9602) adjusts the white space that is maintained
between columns by changing tap.dxaGapHalf. Because we want the left
boundary of text within the leftmost cell to be at the same location after
the sprm is applied, Word also adjusts tap.rgdxCenter[0] by the amount that
tap.dxaGapHalf changes. Bytes 0-1 of the sprm contains the opcode, and the
new dxaGapHalf, call it dxaGapHalfNew, is stored in bytes 2 and 3. When the
sprm is interpreted, the change between the old and new dxaGapHalf values,
tap.dxaGapHalf - dxaGapHalfNew, is added to tap.rgdxaCenter[0] and then
dxaGapHalfNew is moved to tap.dxaGapHalf. sprmTDxaGapHalf is stored in PAPXs
and also in grpprls linked to piece table entries.

*/
void
wvApplysprmTDxaGapHalf (TAP * aTap, U8 * pointer, U16 * pos)
{
    S16 dxaGapHalfNew = (S16) dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    aTap->rgdxaCenter[0] += aTap->dxaGapHalf - dxaGapHalfNew;
    aTap->dxaGapHalf = dxaGapHalfNew;
}

/*
sprmTTableBorders (opcode 0xD605) sets the tap.rgbrcTable. The sprm is
interpreted by moving the 24 bytes of the sprm's operand to tap.rgbrcTable.
*/
void
wvApplysprmTTableBorders (wvVersion ver, TAP * aTap, U8 * pointer, U16 * pos)
{
    int i, d;
    if (ver == WORD8)
      {
	  dread_8ubit (NULL, &pointer);
	  (*pos)++;
      }
    for (i = 0; i < 6; i++)
      {
	  d = wvGetBRCFromBucket (ver, &(aTap->rgbrcTable[i]), pointer);
	  pointer += d;
	  (*pos) += d;
      }
}

/*
sprmTDefTable (opcode 0xD608) defines the boundaries of table cells
(tap.rgdxaCenter) and the properties of each cell in a table (tap.rgtc).
Bytes 0 and 1 of the sprm contain its opcode. Bytes 2 and 3 store a two-byte
length of the following parameter. Byte 4 contains the number of cells that
are to be defined by the sprm, call it itcMac. When the sprm is interpreted,
itcMac is moved to tap.itcMac. itcMac cannot be larger than 32. In bytes 5
through 5+2*(itcMac + 1) -1 , is stored an array of integer dxa values
sorted in ascending order which will be moved to tap.rgdxaCenter. In bytes
5+ 2*(itcMac + 1) through byte 5+2*(itcMac + 1) + 10*itcMac - 1 is stored an
array of TC entries corresponding to the stored tap.rgdxaCenter. This array
is moved to tap.rgtc. sprmTDefTable is only stored in PAPXs.
*/
void
wvApplysprmTDefTable (TAP * aTap, U8 * pointer, U16 * pos)
{
    U16 len;
    int i, t, oldpos;
    wvVersion type;
    len = dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    wvTrace (("wvApplysprmTDefTable\n"));
    aTap->itcMac = dread_8ubit (NULL, &pointer);
    (*pos)++;
    /* a corrupt document can claim more cells than the arrays hold;
       clamp rather than scribble over memory */
    if (aTap->itcMac > itcMax)
	aTap->itcMac = itcMax;
    oldpos = (*pos) - 2;
    wvTrace (("oldpos is %x\n", oldpos));
    wvTrace (("C: there are %d cells\n", aTap->itcMac));
    for (i = 0; i < aTap->itcMac + 1; i++)
      {
	  if (len - (*pos - oldpos) < 2)
	      break;
	  aTap->rgdxaCenter[i] = (S16) dread_16ubit (NULL, &pointer);
	  wvTrace (("C: cell boun is %d\n", aTap->rgdxaCenter[i]));
	  (*pos) += 2;
      }

    wvTrace (
	     ("HERE-->pos is now %d, the len was %d, there is %d left\n",
	      *pos, len, len - (*pos - oldpos)));

    if ((len - (*pos - oldpos)) < (cb6TC * aTap->itcMac))
      {
	  pointer += len - (*pos - oldpos);
	  (*pos) += len - (*pos - oldpos);
	  return;
      }

    if ((len - (*pos - oldpos)) < (cbTC * aTap->itcMac))
	type = WORD6;
    else
	type = WORD8;

    wvTrace (("type is %d\n", type));

    wvTrace (("left over is %d\n", len - (*pos - oldpos)));

    for (i = 0; i < aTap->itcMac; i++)
      {
	  t = wvGetTCFromBucket (type, &(aTap->rgtc[i]), pointer);
	  wvTrace (("DefTable merge is %d\n", aTap->rgtc[i].fVertMerge));
	  /* for christ sake !!, word 8 stores word 6 sized TC's in this sprm ! */
	  (*pos) += t;
	  pointer += t;
	  wvTrace (("t is %d, under is %x\n", t, *pointer));
      }

    wvTrace (("left over is %d\n", len - (*pos - oldpos)));

    /* if the declared TC block overshot the operand length, *pos -
       oldpos can exceed len -- an != 0 condition would loop forever
       reading past the buffer */
    while ((*pos - oldpos) < (int) len)
      {
	  wvTrace (("Eating byte %x\n", dread_8ubit (NULL, &pointer)));
	  (*pos)++;
      }
    wvTrace (
	     ("oldpos is %x, pos is %x, diff is %d\n", oldpos, *pos,
	      *pos - oldpos - 2));
}

/*
sprmTDefTable10 (opcode0xD606) is an obsolete version of sprmTDefTable
(opcode 0xD608) that was used in WinWord 1.x. Its contents are identical to
those in sprmTDefTable, except that the TC structures contain the obsolete
structures BRC10s.
*/

void
wvApplysprmTDefTable10 (TAP * aTap, U8 * pointer, U16 * pos)
{
    U16 len;
    int i, t;
    len = dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    aTap->itcMac = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (aTap->itcMac > itcMax)
	aTap->itcMac = itcMax;
    for (i = 0; i < aTap->itcMac + 1; i++)
      {
	  if (len - (1 + i * 2) < 2)
	      break;
	  aTap->rgdxaCenter[i] = (S16) dread_16ubit (NULL, &pointer);
	  (*pos) += 2;
      }
    for (i = 0; i < aTap->itcMac; i++)
      {
	  t = wvGetTCFromBucket (WORD6, &(aTap->rgtc[i]), pointer);
	  (*pos) += t;
	  pointer += t;
      }
}

void
wv2ApplysprmTDefTableShd (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 len;
    U16 itcMac;
    int i;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    itcMac = len / cbSHD;
    wvTrace (
	     ("len in 2sprmTDefTableShd is %d, no of cells is %d\n", len,
	      itcMac));

    if (itcMac > itcMax)
	itcMac = itcMax;
    for (i = 0; i < itcMac; i++)
      {
	  wvGetSHDFromBucket (&(aTap->rgshd[i]), pointer);
	  pointer += cbSHD;
	  (*pos) += cbSHD;
      }
}


/*
sprmTDefTableShd (opcode 0xD609) is similar to sprmTDefTable, and
compliments it by defining the shading of each cell in a table (tap.rgshd).
Bytes 0 and 1 of the sprm contain its opcode. Bytes 2 and 3 store a two-byte
length of the following parameter. Byte 4 contains the number of cells that
are to be defined by the sprm, call it itcMac. itcMac cannot be larger than
32. In bytes 5 through 5+2*(itcMac + 1) -1 , is stored an array of SHDs.
This array is moved to tap.rgshd. sprmTDefTable is only stored in PAPXs.
*/
void
wvApplysprmTDefTableShd (TAP * aTap, U8 * pointer, U16 * pos)
{
    U16 len;
    U16 itcMac;
    int i, oldpos;

    len = dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    if (len >= 0x4000)
      {
	  len = len & 0x00ff;
	  wvError (
		   ("bad len in sprmTDefTableShd, munging to %d instead\n",
		    len));
      }
    wvTrace (("wvApplysprmTDefTableShd, len %d\n", len));
    itcMac = dread_8ubit (NULL, &pointer);
    (*pos)++;
    oldpos = (*pos) - 2;
    wvTrace (("oldpos is %x\n", oldpos));
    wvTrace (("C: there are %d cells\n", itcMac));
    if (itcMac > 32)
	wvError (("Broken word doc, recovering from stupidity\n"));
    else
      {
	  if ((len - (*pos - oldpos)) < (cbSHD * aTap->itcMac))
	    {
		wvError (("Broken sprmDefTableShd, recovering from problem\n"));
		pointer += len - (*pos - oldpos);
		(*pos) += len - (*pos - oldpos);
		return;
	    }

	  for (i = 0; i < itcMac; i++)
	    {
		wvGetSHDFromBucket (&(aTap->rgshd[i]), pointer);
		pointer += cbSHD;
		(*pos) += cbSHD;
	    }
      }

    /* if the declared SHD block overshot the operand length, *pos -
       oldpos can exceed len -- an != 0 condition would loop forever
       reading past the buffer */
    while ((*pos - oldpos) < (int) len)
      {
	  wvTrace (("Eating byte %x\n", dread_8ubit (NULL, &pointer)));
	  (*pos)++;
      }
    wvTrace (
	     ("oldpos is %x, pos is %x, diff is %d\n", oldpos, *pos,
	      *pos - oldpos - 2));
}

/*
Word 8

sprmTSetBrc (opcode 0xD620) allows the border definitions(BRCs) within TCs
to be set to new values. It has the following format:

 b10 b16 field          type  size bitfield comments

 0   0   sprm           short               opcode 0xD620

 2   2   count          byte                number of bytes for operand

 3   3   itcFirst       byte                the index of the first cell
                                            that is to have its borders
                                            changed.

 4   4   itcLim         byte                index of the cell that follows
                                            the last cell to have its
                                            borders changed

 5   5                  short :4   F0       reserved

         fChangeRight   short :1   08       =1 when tap.rgtc[].brcRight is
                                            to be changed

         fChangeBottom  short :1   04       =1 when tap.rgtc[].brcBottom
                                            is to be changed

         fChangeLeft    short :1   02       =1 when tap.rgtc[].brcLeft is
                                            to be changed

         fChangeTop     short :1   01       =1 when tap.rgtc[].brcTop is
                                            to be changed

 6   6   brc            BRC                 new BRC value to be stored in
                                            TCs.

*/
/* Pre Word 8 *
0    0    	sprm byte opcode 193
1    1    	itcFirst  byte
2    2    	itcLim    byte
3    3         int  :4 F0   reserved
			fChangeRight int  :1   08
			fChangeBottom int  :1   04
			fChangeLeft int  :1   02
			fChangeTop int  :1   01
	4    4  brc  BRC
*/
void
wvApplysprmTSetBrc (wvVersion ver, TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst, itcLim, len, temp8;
    BRC abrc;
    int i;
    if (ver == WORD8)
      {
	  len = dread_8ubit (NULL, &pointer);
	  (*pos)++;
	  wvTrace (("the len is %d", len));
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    itcLim = dread_8ubit (NULL, &pointer);
    temp8 = dread_8ubit (NULL, &pointer);
    (*pos) += 3;
    (*pos) += wvGetBRCFromBucket (ver, &abrc, pointer);

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  if (temp8 & 0x08)
	      wvCopyBRC (&aTap->rgtc[i].brcRight, &abrc);
	  if (temp8 & 0x04)
	      wvCopyBRC (&aTap->rgtc[i].brcBottom, &abrc);
	  if (temp8 & 0x02)
	      wvCopyBRC (&aTap->rgtc[i].brcLeft, &abrc);
	  if (temp8 & 0x01)
	      wvCopyBRC (&aTap->rgtc[i].brcTop, &abrc);
      }
}

/*
sprmTInsert (opcode 0x7621) inserts new cell definitions in an existing
table's cell structure.

Bytes 0 and 1 of the sprm contain the opcode.

Byte 2 is the index within tap.rgdxaCenter and tap.rgtc at which the new dxaCenter
and tc values will be inserted. Call this index itcInsert.

Byte 3 contains a count of the cell definitions to be added to the tap, call it ctc.

Bytes 4 and 5 contain the width of the cells that will be added, call it dxaCol.

If there are already cells defined at the index where cells are to be inserted,
tap.rgdxaCenter entries at or above this index must be moved to the entry
ctc higher and must be adjusted by adding ctc*dxaCol to the value stored.

The contents of tap.rgtc at or above the index must be moved 10*ctc bytes
higher in tap.rgtc.

If itcInsert is greater than the original tap.itcMac, itcInsert - tap.ctc columns
beginning with index tap.itcMac must be added of width dxaCol
(loop from itcMac to itcMac+itcInsert-tap.ctc adding dxaCol to the rgdxaCenter
value of the previous entry and storing sum as dxaCenter of new entry),
whose TC entries are cleared to zeros.

Beginning with index itcInsert, ctc columns of width dxaCol must be added by
constructing new tap.rgdxaCenter and tap.rgtc entries with the newly defined
rgtc entries cleared to zeros.

Finally, the number of cells that were added to the tap is added to tap.itcMac.

sprmTInsert is stored only in grpprls linked to piece table entries.
*/

void
wvApplysprmTInsert (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcInsert = dread_8ubit (NULL, &pointer);
    U8 ctc = dread_8ubit (NULL, &pointer);
    S16 dxaCol = (S16) dread_16ubit (NULL, &pointer);
    int i;
    (*pos) += 4;

    if (aTap->itcMac + ctc >= itcMax)
	ctc = itcMax - 1 - aTap->itcMac;
    if (ctc <= 0)
	return;

    if (itcInsert <= aTap->itcMac + 1)
      {
	  for (i = aTap->itcMac + 1; i >= itcInsert; i--)
	    {
		aTap->rgdxaCenter[i + ctc] =
		    aTap->rgdxaCenter[i] + ctc * dxaCol;
		if (i + ctc < itcMax)
		    aTap->rgtc[i + ctc] = aTap->rgtc[i];
	    }
      }

    if (itcInsert > aTap->itcMac)
      {
	  for (i = aTap->itcMac + 1;
	       i <= itcInsert && i <= itcMax; i++)
	    {
		aTap->rgdxaCenter[i] = aTap->rgdxaCenter[i - 1] + dxaCol;
		if (i < itcMax)
		    wvInitTC (&(aTap->rgtc[i]));
	    }
      }

    for (i = itcInsert; i < ctc + itcInsert && i < itcMax; i++)
      {
	  aTap->rgdxaCenter[i] = aTap->rgdxaCenter[i - 1] + dxaCol;
	  wvInitTC (&(aTap->rgtc[i]));
      }

    aTap->itcMac += ctc;
}


/*
sprmTDelete (opcode 0x5622) deletes cell definitions from an existing
table's cell structure. Bytes 0 and 1of the sprm contain the opcode. Byte 2
contains the index of the first cell to delete, call it itcFirst. Byte 3
contains the index of the cell that follows the last cell to be deleted,
call it itcLim. sprmTDelete causes any rgdxaCenter and rgtc entries whose
index is greater than or equal to itcLim to be moved to the entry that is
itcLim - itcFirst lower, and causes tap.itcMac to be decreased by the number
of cells deleted. sprmTDelete is stored only in grpprls linked to piece
table entries.
*/
void
wvApplysprmTDelete (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst = dread_8ubit (NULL, &pointer);
    U8 itcLim = dread_8ubit (NULL, &pointer);
    int i;
    (*pos) += 2;

    if (itcLim > aTap->itcMac || itcFirst >= itcLim)
	return;

    for (i = itcLim; i < aTap->itcMac + 1; i++)
      {
	  aTap->rgdxaCenter[i - (itcLim - itcFirst)] = aTap->rgdxaCenter[i];
	  if (i - (itcLim - itcFirst) < itcMax)
	      wvCopyTC (&(aTap->rgtc[i - (itcLim - itcFirst)]),
			&(aTap->rgtc[i]));
      }
    aTap->itcMac -= itcLim - itcFirst;
}

/*
sprmTDxaCol (opcode 0x7623) changes the width of cells whose index is within
a certain range to be a certain value. Bytes 0 and 1of the sprm contain the
opcode. Byte 2 contains the index of the first cell whose width is to be
changed, call it itcFirst. Byte 3 contains the index of the cell that
follows the last cell whose width is to be changed, call it itcLim. Bytes 4
and 5 contain the new width of the cell, call it dxaCol.

This sprm causes the itcLim - itcFirst entries of tap.rgdxaCenter to be
adjusted so that tap.rgdxaCenter[i+1] = tap.rgdxaCenter[i] + dxaCol. Any
tap.rgdxaCenter entries that exist beyond itcLim are adjusted to take into
account the amount added to or removed from the previous columns.
*/
void
wvApplysprmTDxaCol (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst = dread_8ubit (NULL, &pointer);
    U8 itcLim = dread_8ubit (NULL, &pointer);
    S16 dxaCol = (S16) dread_16ubit (NULL, &pointer);
    S16 diff = 0;
    int i;
    (*pos) += 4;

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    for (i = itcFirst; i < itcLim; i++)
      {
	  diff += aTap->rgdxaCenter[i + 1] - (aTap->rgdxaCenter[i] + dxaCol);
	  aTap->rgdxaCenter[i + 1] = aTap->rgdxaCenter[i] + dxaCol;
      }
    /* the original code had a stray semicolon here which caused a
       single out-of-bounds write past rgdxaCenter instead of shifting
       all following column positions */
    for (i = itcLim + 1; i <= aTap->itcMac; i++)
	aTap->rgdxaCenter[i] -= diff;
}

/*
sprmTMerge (opcode 0x5624) merges the display areas of cells within a
specified range. Bytes 0 and 1 of the sprm contain the opcode. Byte 2
contains the index of the first cell that is to be merged, call it itcFirst.
Byte 3 contains the index of the cell that follows the last cell to be
merged, call it itcLim.

This sprm causes tap.rgtc[itcFirst].fFirstMerged to
be set to 1. Cells in the range whose index is greater than itcFirst and
less than itcLim have tap.rgtc[].fMerged set to 1. sprmTMerge is stored only
in grpprls linked to piece table entries.

*/
void
wvApplysprmTMerge (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst = dread_8ubit (NULL, &pointer);
    U8 itcLim = dread_8ubit (NULL, &pointer);
    int i;
    (*pos) += 2;

    if (itcFirst >= itcMax)
	return;
    aTap->rgtc[itcFirst].fFirstMerged = 1;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst + 1; i < itcLim; i++)
	aTap->rgtc[i].fMerged = 1;
}

/*
sprmTSplit (opcode 0x5625) splits the display areas of merged cells into
their originally assigned display areas. Bytes 0 and 1 of the sprm contain
the opcode. Byte 2 contains the index of the first cell that is to be split,
call it itcFirst. Byte 3 contains the index of the cell that follows the
last cell to be split, call it itcLim.

This sprm clears
tap.rgtc[].fFirstMerged and tap.rgtc[].fMerged for all rgtc entries >=
itcFirst and < itcLim. sprmTSplit is stored only in grpprls linked to piece
table entries.
*/
void
wvApplysprmTSplit (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst = dread_8ubit (NULL, &pointer);
    U8 itcLim = dread_8ubit (NULL, &pointer);
    int i;
    (*pos) += 2;

    if (itcLim > itcMax)
	itcLim = itcMax;
    if (itcFirst < itcMax)
	aTap->rgtc[itcFirst].fFirstMerged = 0;
    for (i = itcFirst; i < itcLim; i++)
	aTap->rgtc[i].fMerged = 0;
}

/*
This is guess based upon SetBrc
*/
void
wvApplysprmTSetBrc10 (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst, itcLim, len, temp8;
    BRC10 abrc;
    int i;
    len = dread_8ubit (NULL, &pointer);
    itcFirst = dread_8ubit (NULL, &pointer);
    itcLim = dread_8ubit (NULL, &pointer);
    temp8 = dread_8ubit (NULL, &pointer);
    (*pos) += 3;
    (*pos) += wvGetBRC10FromBucket (&abrc, pointer);

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  if (temp8 & 0x08)
	      wvConvertBRC10ToBRC (&aTap->rgtc[i].brcRight, &abrc);
	  if (temp8 & 0x04)
	      wvConvertBRC10ToBRC (&aTap->rgtc[i].brcBottom, &abrc);
	  if (temp8 & 0x02)
	      wvConvertBRC10ToBRC (&aTap->rgtc[i].brcLeft, &abrc);
	  if (temp8 & 0x01)
	      wvConvertBRC10ToBRC (&aTap->rgtc[i].brcTop, &abrc);
      }
}

/*
sprmTSetShd (opcode 0x7627) allows the shading definitions(SHDs) within a
tap to be set to new values. Bytes 0 and 1 of the sprm contain the opcode.
Byte 2 contains the index of the first cell whose shading is to be changed,
call it itcFirst. Byte 3 contains the index of the cell that follows the
last cell whose shading is to be changed, call it itcLim. Bytes 4 and 5
contain the SHD structure, call it shd. This sprm causes the itcLim -
itcFirst entries of tap.rgshd to be set to shd. sprmTSetShd is stored only
in grpprls linked to piece table entries.
*/
void
wvApplysprmTSetShd (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst = dread_8ubit (NULL, &pointer);
    U8 itcLim = dread_8ubit (NULL, &pointer);
    int i;
    SHD shd;
    (*pos) += 2;

    wvGetSHDFromBucket (&shd, pointer);
    (*pos) += cbSHD;

    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
	wvCopySHD (&aTap->rgshd[i], &shd);
}

/*
sprmTSetShdOdd (opcode 0x7628) is identical to sprmTSetShd, but it only
changes the rgshd for odd indices between itcFirst and. sprmTSetShdOdd is
stored only in grpprls linked to piece table entries.
*/
void
wvApplysprmTSetShdOdd (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst = dread_8ubit (NULL, &pointer);
    U8 itcLim = dread_8ubit (NULL, &pointer);
    int i;
    SHD shd;
    (*pos) += 2;

    wvGetSHDFromBucket (&shd, pointer);
    (*pos) += cbSHD;

    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  if ((i - itcFirst) % 2 == 0)
	      wvCopySHD (&aTap->rgshd[i], &shd);
      }
}

/* CellRangeTextFlow (MS-DOC 2.9.31): itcFirst, itcLim (1 byte each),
   then a 2-byte TextFlow enum. */
void
wvApplysprmTTextFlow (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 itcFirst = dread_8ubit (NULL, &pointer);
    U8 itcLim = dread_8ubit (NULL, &pointer);
    U16 tf = dread_16ubit (NULL, &pointer);
    int i;
    (*pos) += 4;

    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  aTap->rgtc[i].textFlow = tf;
	  /* MS-DOC TextFlow: 1 (tbrl), 3 (btlr) and 5 (tbrlv) are the
	     rotated line layouts; 0 and 4 run lines horizontally */
	  aTap->rgtc[i].fVertical = (tf == 1 || tf == 3 || tf == 5);
	  aTap->rgtc[i].fBackward = (tf == 3);
	  aTap->rgtc[i].fRotateFont = (tf == 4 || tf == 5);
      }
}

/*
sprmTVertMerge (opcode 0xD62B) changes the vertical cell merge properties
for a cell in the tap.rgtc[]. Bytes 0 and 1 of the sprm contain the opcode.
Byte 2 contains the index of the cell whose vertical cell merge properties
are to be changed. Byte 3 codes the new vertical cell merge properties for
the cell, a 0 clears both fVertMerge and fVertRestart, a 1 sets fVertMerge
and clears fVertRestart, and a 3 sets both flags. sprmTVertMerge is stored
only in grpprls linked to piece table entries.
*/
void
wvApplysprmTVertMerge (TAP * aTap, U8 * pointer, U16 * pos)
{
    /* spra 6 operand (MS-DOC VertMergeOperand): cb byte, then an
       ItcFirstLim (itcFirst, itcLim) and a 1-byte VerticalMergeFlag;
       the flag applies to the whole range, not a single cell */
    U8 len, itcFirst, itcLim, props;
    int i;
    wvTrace (("doing Vertical merge\n"));

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len < 3)
      {
	  (*pos) += len;
	  return;
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    itcLim = dread_8ubit (NULL, &pointer);
    props = dread_8ubit (NULL, &pointer);
    (*pos) += 3;
    if (len > 3)
	(*pos) += len - 3;

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  switch (props)
	    {
	    case 0:
		aTap->rgtc[i].fVertMerge = 0;
		aTap->rgtc[i].fVertRestart = 0;
		break;
	    case 1:
		aTap->rgtc[i].fVertMerge = 1;
		aTap->rgtc[i].fVertRestart = 0;
		break;
	    case 3:
		aTap->rgtc[i].fVertMerge = 1;
		aTap->rgtc[i].fVertRestart = 1;
		break;
	    }
      }
}

/*
sprmTVertAlign (opcode 0xD62C) changes the vertical alignment property in
the tap.rgtc[]. Bytes 0 and 1 of the sprm contain the opcode. Byte 2
contains the index of the first cell whose shading is to be changed, call it
itcFirst. Byte 3 contains the index of the cell that follows the last cell
whose shading is to be changed, call it itcLim. This sprm causes the
vertAlign properties of the itcLim - itcFirst entries of tap.rgtc[] to be
set to the new vertical alignment property contained in Byte 4.
sprmTVertAlign is stored only in grpprls linked to piece table entries.
*/
void
wvApplysprmTVertAlign (TAP * aTap, U8 * pointer, U16 * pos)
{
    /* spra 6 variable-length operand: cb byte, then ItcFirstLim
       (2 bytes), then a 1-byte vertAlign */
    U8 len = dread_8ubit (NULL, &pointer);
    U8 itcFirst, itcLim, props;
    int i;
    (*pos)++;

    if (len < 3)
      {
	  (*pos) += len;
	  return;
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    itcLim = dread_8ubit (NULL, &pointer);
    props = dread_8ubit (NULL, &pointer);
    (*pos) += 3;
    if (len > 3)
	(*pos) += len - 3;

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
	aTap->rgtc[i].vertAlign = props;
}

/* DefTableShdOperand (MS-DOC 2.9.94): 1-byte cb followed by cb/10
   10-byte Shd structures applied to cells firstCell..firstCell+n-1.
   sprmTDefTableShd/sprmTDefTableShd2nd/sprmTDefTableShd3rd (and the
   Raw variants) differ only in the first cell index (0, 22, 44). */
void
wvApplysprmTDefTableShdNew (TAP * aTap, U8 * pointer, U16 * pos,
			    int firstCell)
{
    U8 len;
    int i, nop;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    nop = len / 10;
    for (i = 0; i < nop; i++)
      {
	  if (firstCell + i >= itcMax)
	      break;
	  wvGetSHD10FromBucket (&aTap->rgshd[firstCell + i],
				pointer + i * 10);
      }
    (*pos) += len;
}

/* TableShadeOperand (MS-DOC 2.9.312): cb byte, ItcFirstLim (2 bytes),
   then a 10-byte Shd.  With odd set only every other cell of the range
   is shaded. */
void
wvApplysprmTSetShdNew (TAP * aTap, U8 * pointer, U16 * pos, int odd)
{
    U8 len;
    int i, itcFirst, itcLim, step;
    SHD shd;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len < 12)
      {
	  (*pos) += len;
	  return;
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    itcLim = dread_8ubit (NULL, &pointer);
    pointer += 2;
    (*pos) += 4;
    wvGetSHD10FromBucket (&shd, pointer);
    (*pos) += 10;
    if (len > 12)
	(*pos) += len - 12;
    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    step = odd ? 2 : 1;
    for (i = itcFirst; i < itcLim; i += step)
	wvCopySHD (&(aTap->rgshd[i]), &shd);
}

/* TableBordersOperand (MS-DOC): cb byte followed by six 8-byte Brc
   structures (top, left, bottom, right, insideH, insideV). */
void
wvApplysprmTTableBorders97 (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 len;
    int i;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    for (i = 0; i < 6 && len >= 8; i++, len -= 8)
      {
	  wvGetBRC8FromBucket (&aTap->rgbrcTable[i], pointer);
	  pointer += 8;
	  (*pos) += 8;
      }
    (*pos) += len;
}

/* TableBrcOperand (MS-DOC 2.9.308): cb byte, ItcFirstLim (2 bytes), a
   bitmask of borders to set (top 0x01, left 0x02, bottom 0x04, right
   0x08, diag tl2br 0x10, diag tr2bl 0x20), then an 8-byte
   BrcMayBeNil. */
void
wvApplysprmTSetBrcNew (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 len, temp8;
    int i, itcFirst, itcLim;
    BRC brc;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len < 11)
      {
	  (*pos) += len;
	  return;
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    (*pos)++;
    itcLim = dread_8ubit (NULL, &pointer);
    (*pos)++;
    temp8 = dread_8ubit (NULL, &pointer);
    (*pos)++;
    wvGetBRC8FromBucket (&brc, pointer);
    pointer += 8;
    (*pos) += 8;
    if (len > 11)
	(*pos) += len - 11;

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  if (temp8 & 0x08)
	      wvCopyBRC (&aTap->rgtc[i].brcRight, &brc);
	  if (temp8 & 0x04)
	      wvCopyBRC (&aTap->rgtc[i].brcBottom, &brc);
	  if (temp8 & 0x02)
	      wvCopyBRC (&aTap->rgtc[i].brcLeft, &brc);
	  if (temp8 & 0x01)
	      wvCopyBRC (&aTap->rgtc[i].brcTop, &brc);
	  /* 0x10/0x20 diagonal borders are not represented in TC */
      }
}

/* BrcCvOperand (MS-DOC): cb byte followed by itcMac*4 COLORREFs, one
   for each cell's border in the affected position. */
void
wvApplysprmTBrcCv (TAP * aTap, U8 * pointer, U16 * pos, int which)
{
    U8 len;
    int i;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    for (i = 0; i < len / 4 && i < aTap->itcMac && i < itcMax; i++)
      {
	  BRC *b = NULL;

	  switch (which)
	    {
	    case 0:
		b = &aTap->rgtc[i].brcTop;
		break;
	    case 1:
		b = &aTap->rgtc[i].brcLeft;
		break;
	    case 2:
		b = &aTap->rgtc[i].brcBottom;
		break;
	    case 3:
		b = &aTap->rgtc[i].brcRight;
		break;
	    }
	  if (b)
	    {
		b->cv = dread_32ubit (NULL, &pointer);
		b->fCv = 1;
	    }
      }
    (*pos) += len;
}

/* CSSAOperand (MS-DOC 2.9.38): cb byte, ItcFirstLim (2 bytes), mask
   (top 0x01, left 0x02, bottom 0x04, right 0x08), fts (1 byte),
   wWidth (2 bytes).  isDefault applies the padding to the TAP
   defaults (sprmTCellPaddingDefault / sprmTCellPaddingStyle),
   otherwise it lands in the per-cell TC pad fields (sprmTCellPadding). */
void
wvApplysprmTCellPadding (TAP * aTap, U8 * pointer, U16 * pos,
			 U8 isDefault)
{
    U8 len, mask, fts;
    S16 w;
    int i, itcFirst, itcLim;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len < 6)
      {
	  (*pos) += len;
	  return;
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    (*pos)++;
    itcLim = dread_8ubit (NULL, &pointer);
    (*pos)++;
    mask = dread_8ubit (NULL, &pointer);
    (*pos)++;
    fts = dread_8ubit (NULL, &pointer);
    (*pos)++;
    w = (S16) dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    if (len > 6)
	(*pos) += len - 6;

    if (fts != ftsDxa)
	w = 0;			/* only twips margins are representable */
    if (isDefault)
      {
	  if (mask & 0x01)
	      aTap->cellPadTop = w;
	  if (mask & 0x02)
	      aTap->cellPadLeft = w;
	  if (mask & 0x04)
	      aTap->cellPadBottom = w;
	  if (mask & 0x08)
	      aTap->cellPadRight = w;
	  aTap->fCellPadMask |= mask & 0x0f;
	  return;
      }
    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  if (mask & 0x01)
	    {
		aTap->rgtc[i].padTop = w;
		aTap->rgtc[i].fPadMask |= 0x01;
	    }
	  if (mask & 0x02)
	    {
		aTap->rgtc[i].padLeft = w;
		aTap->rgtc[i].fPadMask |= 0x02;
	    }
	  if (mask & 0x04)
	    {
		aTap->rgtc[i].padBottom = w;
		aTap->rgtc[i].fPadMask |= 0x04;
	    }
	  if (mask & 0x08)
	    {
		aTap->rgtc[i].padRight = w;
		aTap->rgtc[i].fPadMask |= 0x08;
	    }
      }
}

/* sprmTCellSpacingDefault: CSSAOperand, spacing stored in wWidth. */
void
wvApplysprmTCellSpacing (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 len, fts;
    S16 w;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len < 6)
      {
	  (*pos) += len;
	  return;
      }
    pointer += 3;		/* itc + mask */
    (*pos) += 3;
    fts = dread_8ubit (NULL, &pointer);
    (*pos)++;
    w = (S16) dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    if (len > 6)
	(*pos) += len - 6;

    if (fts == ftsDxa || fts == ftsDxaSys)
      {
	  aTap->cellSpacing = w;
	  aTap->fCellSpacing = 1;
      }
}

/* TableCellWidthOperand (MS-DOC 2.9.309): cb byte, ItcFirstLim
   (2 bytes), then a 3-byte FtsWWidth_Table preferred cell width. */
void
wvApplysprmTCellWidth (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 len, fts;
    S16 w;
    int i, itcFirst, itcLim;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len < 5)
      {
	  (*pos) += len;
	  return;
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    (*pos)++;
    itcLim = dread_8ubit (NULL, &pointer);
    (*pos)++;
    fts = dread_8ubit (NULL, &pointer);
    (*pos)++;
    w = (S16) dread_16ubit (NULL, &pointer);
    (*pos) += 2;
    if (len > 5)
	(*pos) += len - 5;

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  aTap->rgtc[i].ftsWidth = fts;
	  aTap->rgtc[i].wWidth = w;
      }
}

/* sprmTFitText (MS-DOC): FtsCellWidth operand — ItcFirstLim then a
   boolean flag, applied per cell. */
void
wvApplysprmTFitText (TAP * aTap, U8 * pointer, U16 * pos)
{
    int i, itcFirst, itcLim;
    U8 f;

    itcFirst = dread_8ubit (NULL, &pointer);
    (*pos)++;
    itcLim = dread_8ubit (NULL, &pointer);
    (*pos)++;
    f = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
	aTap->rgtc[i].fFitText = f;
}

/* Variable-length boolean cell-flag sprms (sprmTFCellNoWrap,
   sprmTCellFHideMark): cb byte, ItcFirstLim, 1-byte flag.  which
   selects the TC field. */
void
wvApplysprmTFlagRange (TAP * aTap, U8 * pointer, U16 * pos, int which)
{
    U8 len, f;
    int i, itcFirst, itcLim;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len < 3)
      {
	  (*pos) += len;
	  return;
      }
    itcFirst = dread_8ubit (NULL, &pointer);
    (*pos)++;
    itcLim = dread_8ubit (NULL, &pointer);
    (*pos)++;
    f = dread_8ubit (NULL, &pointer);
    (*pos)++;
    if (len > 3)
	(*pos) += len - 3;

    if (itcLim > aTap->itcMac)
	itcLim = aTap->itcMac;
    if (itcLim > itcMax)
	itcLim = itcMax;
    for (i = itcFirst; i < itcLim; i++)
      {
	  switch (which)
	    {
	    case 0:
		aTap->rgtc[i].fNoWrap = f;
		break;
	    case 1:
		aTap->rgtc[i].fHideMark = f;
		break;
	    }
      }
}

/* TCellBrcTypeOperand (MS-DOC 2.9.302): cb byte followed by 4 BrcType
   bytes per cell (top, left, bottom, right). */
void
wvApplysprmTCellBrcType (TAP * aTap, U8 * pointer, U16 * pos)
{
    U8 len;
    int i;

    len = dread_8ubit (NULL, &pointer);
    (*pos)++;
    for (i = 0; i < len / 4 && i < aTap->itcMac && i < itcMax; i++)
      {
	  aTap->rgtc[i].brcTop.brcType = pointer[i * 4];
	  aTap->rgtc[i].brcTop.fCv = 0;
	  aTap->rgtc[i].brcLeft.brcType = pointer[i * 4 + 1];
	  aTap->rgtc[i].brcLeft.fCv = 0;
	  aTap->rgtc[i].brcBottom.brcType = pointer[i * 4 + 2];
	  aTap->rgtc[i].brcBottom.fCv = 0;
	  aTap->rgtc[i].brcRight.brcType = pointer[i * 4 + 3];
	  aTap->rgtc[i].brcRight.fCv = 0;
      }
    (*pos) += len;
}

/* sprmTSetShdTable (MS-DOC): SHDOperand applied to the whole table. */
void
wvApplysprmTSetShdTable (wvVersion ver, TAP * aTap, U8 * pointer,
			 U16 * pos)
{
    (*pos) += wvGetSHDOperandFromBucket (&aTap->shdTable, pointer);
}

SprmName rgsprmPrm[0x80] =
    { sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmPIncLvl, sprmPJc80,
    sprmPFSideBySide, sprmPFKeep, sprmPFKeepFollow, sprmPFPageBreakBefore,
    sprmPBrcl, sprmPBrcp, sprmPIlvl, sprmNoop, sprmPFNoLineNumb, sprmNoop,
    sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop,
    sprmNoop, sprmPFInTable, sprmPFTtp, sprmNoop, sprmNoop, sprmNoop, sprmPPc,
    sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop,
    sprmPWr, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop,
    sprmPFNoAutoHyph, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop,
    sprmPFLocked, sprmPFWidowControl, sprmNoop, sprmPFKinsoku, sprmPFWordWrap,
    sprmPFOverflowPunct, sprmPFTopLinePunct, sprmPFAutoSpaceDE,
    sprmPFAutoSpaceDN, sprmNoop, sprmNoop, sprmPISnapBaseLine, sprmNoop,
    sprmNoop, sprmNoop, sprmCFStrikeRM, sprmCFRMarkIns, sprmCFFldVanish,
    sprmNoop,
    sprmNoop, sprmNoop, sprmCFData, sprmNoop, sprmNoop, sprmNoop, sprmCFOle2,
    sprmNoop, sprmCHighlight, sprmCFEmboss, sprmCSfxText, sprmNoop, sprmNoop,
    sprmNoop, sprmCPlain, sprmNoop, sprmCFBold, sprmCFItalic, sprmCFStrike,
    sprmCFOutline, sprmCFShadow, sprmCFSmallCaps, sprmCFCaps, sprmCFVanish,
    sprmNoop, sprmCKul, sprmNoop, sprmNoop, sprmNoop, sprmCIco, sprmNoop,
    sprmCHpsInc, sprmNoop, sprmCHpsPosAdj, sprmNoop, sprmCIss, sprmNoop,
    sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop, sprmNoop,
    sprmNoop, sprmNoop, sprmCFDStrike, sprmCFImprint, sprmCFSpec, sprmCFObj,
    sprmPicBrcl, sprmPOutLvl, sprmNoop, sprmNoop, sprmNoop, sprmNoop,
    sprmNoop,
    sprmPPnbrRMarkNot
};

SprmName
wvGetrgsprmPrm (U16 in)
{
    if (in > 0x80)
      {
	  wvError (("Impossible rgsprmPrm value\n"));
	  return (sprmNoop);
      }
    return (rgsprmPrm[in]);
}


SprmName rgsprmWord6[256] = {
    sprmNoop /*          0 */ ,
    sprmNoop /*                  1 */ ,
    sprmPIstd /*         2 */ ,
    sprmPIstdPermute /*  3 */ ,
    sprmPIncLvl /*       4 */ ,
    sprmPJc80 /*           5 */ ,
    sprmPFSideBySide /*  6 */ ,
    sprmPFKeep /*        7 */ ,
    sprmPFKeepFollow /*  8 */ ,
    sprmPFPageBreakBefore /*  9 */ ,	/* added F */
    sprmPBrcl /*         10 */ ,
    sprmPBrcp /*         11 */ ,
    sprmPAnld /*         12 */ ,
    sprmPNLvlAnm /*      13 */ ,
    sprmPFNoLineNumb /*  14 */ ,
    sprmPChgTabsPapx /*  15 */ ,
    sprmPDxaRight80 /*     16 */ ,
    sprmPDxaLeft80 /*      17 */ ,
    sprmPNest80 /*         18 */ ,
    sprmPDxaLeft180 /*     19 */ ,
    sprmPDyaLine /*      20 */ ,
    sprmPDyaBefore /*    21 */ ,
    sprmPDyaAfter /*     22 */ ,
    sprmPChgTabs /*      23 */ ,
    sprmPFInTable /*     24 */ ,
    sprmPFTtp /*         25 */ ,	/* added F */
    sprmPDxaAbs /*       26 */ ,
    sprmPDyaAbs /*       27 */ ,
    sprmPDxaWidth /*     28 */ ,
    sprmPPc /*           29 */ ,
    sprmPBrcTop10 /*     30 */ ,
    sprmPBrcLeft10 /*    31 */ ,
    sprmPBrcBottom10 /*  32 */ ,
    sprmPBrcRight10 /*   33 */ ,
    sprmPBrcBetween10 /* 34 */ ,
    sprmPBrcBar10 /*     35 */ ,
    sprmPDxaFromText10 /*   36 */ ,	/* new name */
    sprmPWr /*           37 */ ,
    sprmPBrcTop80 /*       38 */ ,
    sprmPBrcLeft80 /*      39 */ ,
    sprmPBrcBottom80 /*    40 */ ,
    sprmPBrcRight80 /*     41 */ ,
    sprmPBrcBetween80 /*   42 */ ,
    sprmPBrcBar80 /*       43 */ ,
    sprmPFNoAutoHyph /*  44 */ ,
    sprmPWHeightAbs /*   45 */ ,
    sprmPDcs /*          46 */ ,
    sprmPShd80 /*          47 */ ,
    sprmPDyaFromText /*  48 */ ,
    sprmPDxaFromText /*  49 */ ,
    sprmPFLocked /*      50 */ ,
    sprmPFWidowControl /*  51 */ ,
    sprmNoop /*          52 */ ,
    sprmNoop /*          53 */ ,
    sprmNoop /*          54 */ ,
    sprmNoop /*          55 */ ,
    sprmNoop /*          56 */ ,
    sprmPUNKNOWN2 /*     57 */ ,
    sprmPUNKNOWN3 /*     58 */ ,
    sprmPUNKNOWN4 /*     59 */ ,
    sprmNoop /*          60 */ ,
    sprmNoop /*          61 */ ,
    sprmNoop /*          62 */ ,
    sprmNoop /*          63 */ ,
    sprmNoop /*          64 */ ,
    sprmCFStrikeRM /*    65 */ ,
    sprmCFRMarkIns /*    66 */ ,
    sprmCFFldVanish /*   67 */ ,
    sprmCPicLocation /*  68 */ ,
    sprmCIbstRMark /*    69 */ ,
    sprmCDttmRMark /*    70 */ ,
    sprmCFData /*        71 */ ,
    sprmCIdslRMark /*     72 */ ,	/* new name */
    sprmCChs /*         73 */ ,	/* new name */
    sprmCSymbol /*       74 */ ,
    sprmCFOle2 /*        75 */ ,
    sprmNoop /*          76 */ ,
    sprmNoop /*          77 */ ,
    sprmNoop /*          78 */ ,
    sprmNoop /*          79 */ ,
    sprmCIstd /*         80 */ ,
    sprmCIstdPermute /*  81 */ ,
    sprmCDefault /*      82 */ ,
    sprmCPlain /*        83 */ ,
    sprmNoop /*          84 */ ,
    sprmCFBold /*        85 */ ,
    sprmCFItalic /*      86 */ ,
    sprmCFStrike /*      87 */ ,
    sprmCFOutline /*     88 */ ,
    sprmCFShadow /*      89 */ ,
    sprmCFSmallCaps /*   90 */ ,
    sprmCFCaps /*        91 */ ,
    sprmCFVanish /*      92 */ ,
    sprmCFtc /*          93 */ ,
    sprmCKul /*          94 */ ,
    sprmCSizePos /*      95 */ ,
    sprmCDxaSpace /*     96 */ ,
    sprmCLid /*          97 */ ,
    sprmCIco /*          98 */ ,
    sprmCHps /*          99 */ ,
    sprmCHpsInc /*       100 */ ,
    sprmCHpsPos /*       101 */ ,
    sprmCHpsPosAdj /*    102 */ ,
    sprmCMajority /*     103 */ ,
    sprmCIss /*          104 */ ,
    sprmCHpsNew50 /*     105 */ ,
    sprmCHpsInc1 /*      106 */ ,
    sprmCHpsKern /*      107 */ ,
    sprmCMajority50 /*   108 */ ,
    sprmCHpsMul /*       109 */ ,
    sprmCHresi /*         110 */ ,	/* spec name */
    sprmCUNKNOWN5 /*     111 */ ,
    sprmCUNKNOWN6 /*     112 */ ,
    sprmCUNKNOWN7 /*     113 */ ,
    sprmNoop /*          114 */ ,
    sprmNoop /*          115 */ ,
    sprmNoop /*          116 */ ,
    sprmCFSpec /*        117 */ ,
    sprmCFObj /*         118 */ ,
    sprmPicBrcl /*       119 */ ,
    sprmPicScale /*      120 */ ,
    sprmPicBrcTop /*     121 */ ,
    sprmPicBrcLeft /*    122 */ ,
    sprmPicBrcBottom /*  123 */ ,
    sprmPicBrcRight /*   124 */ ,
    sprmNoop /*          125 */ ,
    sprmNoop /*          126 */ ,
    sprmNoop /*          127 */ ,
    sprmNoop /*          128 */ ,
    sprmNoop /*          129 */ ,
    sprmNoop /*          130 */ ,
    sprmScnsPgn /*           131 */ ,	/* new name */
    sprmSiHeadingPgn /*  132 */ ,
    sprmSOlstAnm /*      133 */ ,
    sprmNoop /*          134 */ ,
    sprmNoop /*          135 */ ,
    sprmSDxaColWidth /*  136 */ ,
    sprmSDxaColWidth /*  137 */ ,	/* new name */
    sprmSFEvenlySpaced /*138 */ ,
    sprmSFProtected /*   139 */ ,
    sprmSDmBinFirst /*   140 */ ,
    sprmSDmBinOther /*   141 */ ,
    sprmSBkc /*          142 */ ,
    sprmSFTitlePage /*   143 */ ,
    sprmSCcolumns /*     144 */ ,
    sprmSDxaColumns /*   145 */ ,
    sprmSFAutoPgn /*     146 */ ,
    sprmSNfcPgn /*       147 */ ,
    sprmSDyaPgn /*       148 */ ,
    sprmSDxaPgn /*       149 */ ,
    sprmSFPgnRestart /*  150 */ ,
    sprmSFEndnote /*     151 */ ,
    sprmSLnc /*          152 */ ,
    sprmSGprfIhdt /*     153 */ ,
    sprmSNLnnMod /*      154 */ ,
    sprmSDxaLnn /*       155 */ ,
    sprmSDyaHdrTop /*    156 */ ,
    sprmSDyaHdrBottom /* 157 */ ,
    sprmNoop /*          158 */ ,
    sprmSVjc /*          159 */ ,
    sprmSLnnMin /*       160 */ ,
    sprmSPgnStart97 /*   161 */ ,
    sprmSBOrientation /* 162 */ ,
    sprmSBCustomize /*   163 */ ,
    sprmSXaPage /*       164 */ ,
    sprmSYaPage /*       165 */ ,
    sprmSDxaLeft /*      166 */ ,
    sprmSDxaRight /*     167 */ ,
    sprmSDyaTop /*       168 */ ,
    sprmSDyaBottom /*    169 */ ,
    sprmSDzaGutter /*    170 */ ,
    sprmSDmPaperReq /*   171 */ ,
    sprmNoop /*          172 */ ,
    sprmNoop /*          173 */ ,
    sprmNoop /*          174 */ ,
    sprmNoop /*          175 */ ,
    sprmNoop /*          176 */ ,
    sprmNoop /*          177 */ ,
    sprmNoop /*          178 */ ,
    sprmNoop /*          179 */ ,
    sprmNoop /*          180 */ ,
    sprmNoop /*          181 */ ,
    sprmTJc /*           182 */ ,
    sprmTDxaLeft /*      183 */ ,
    sprmTDxaGapHalf /*   184 */ ,
    sprmTFCantSplit /*   185 */ ,
    sprmTTableHeader /*  186 */ ,
    sprmTTableBorders /* 187 */ ,
    sprmTDefTable10 /*   188 */ ,
    sprmTDyaRowHeight /* 189 */ ,
    sprmTDefTable /*     190 */ ,
    sprmTDefTableShd /*  191 */ ,
    sprmTTlp /*          192 */ ,
    sprmTSetBrc /*       193 */ ,
    sprmTInsert /*       194 */ ,
    sprmTDelete /*       195 */ ,
    sprmTDxaCol /*       196 */ ,
    sprmTMerge /*        197 */ ,
    sprmTSplit /*        198 */ ,
    sprmTSetBrc10 /*     199 */ ,
    sprmTSetShd /*       200 */ ,
    sprmNoop /*          201 */ ,
    sprmNoop /*          202 */ ,
    sprmNoop /*          203 */ ,

    sprmTUNKNOWN1 /*    204 */ ,
    /*guess I know that this should be either
     * a) 3 bytes long,
     * b) complex with a len of 2,
     * its certainly a table related sprm, my guess is sprmTVertMerge
     * as that fits its profile, but it isn't working in practice.
     * */
#if 0
    sprmNoop /*          205 */ ,
    sprmNoop /*          206 */ ,
    sprmNoop /*          207 */ ,
    sprmMax			/*           208 */
#endif
};

SprmName
wvGetrgsprmWord6 (U8 in)
{
    return (rgsprmWord6[in]);
}
