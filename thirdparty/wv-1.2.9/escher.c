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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "wv.h"


void
wvReleaseEscher (escherstruct * item)
{
    wvReleaseDggContainer (&item->dggcontainer);
    wvReleaseDgContainer (&item->dgcontainer);
}

void
wvInitEscher (escherstruct * item)
{
    wvInitDggContainer (&item->dggcontainer);
    wvInitDgContainer (&item->dgcontainer);
}

/* AbiWord: bytes still readable from the stream, clamped at 0.
   Corrupt record lengths must not drive allocation or loop counts
   past EOF -- reads at/after end of stream only ever yield zeros */
static U32
s_stream_remaining (wvStream * fd)
{
    long left = (long) wvStream_size (fd) - (long) wvStream_tell (fd);
    return (left > 0) ? (U32) left : 0;
}

void
wvGetEscher (escherstruct * item, U32 offset, U32 len, wvStream * fd,
	     wvStream * delay)
{
    U32 count = 0;
    MSOFBH amsofbh;
    long base;

    wvStream_goto (fd, offset);
    wvTrace (("offset %x, len %d\n", offset, len));
    wvInitEscher (item);
    base = wvStream_tell (fd);
    while (count < len && s_stream_remaining (fd) != 0)
      {
	  long recstart = wvStream_tell (fd);
	  long recend;
	  count += wvGetMSOFBH (&amsofbh, fd);
	  wvTrace (
		   ("count is %x,len is %x, next len is %x\n", count, len,
		    amsofbh.cbLength));
	  wvTrace (("type is %x\n	", amsofbh.fbt));
	  switch (amsofbh.fbt)
	    {
	    case msofbtDggContainer:
		count +=
		    wvGetDggContainer (&item->dggcontainer, &amsofbh, fd,
				       delay);
		break;
	    case msofbtDgContainer:
		count += wvGetDgContainer (&item->dgcontainer, &amsofbh, fd);
		break;
	    default:
		/* stray padding bytes appear between the top-level
		   records of some producers' OfficeArtDggInfo (the old
		   "extra byte" hack in wvGetDggContainer worked around
		   exactly this): skip a single byte and rescan */
		wvTrace (("skipping pad byte at %x\n", recstart));
		wvStream_goto (fd, recstart + 1);
		count = recstart + 1 - base;
		continue;
	    }
	  /* resync to the end of the record whatever the child read */
	  recend = recstart + 8 + amsofbh.cbLength;
	  if ((long) wvStream_tell (fd) != recend)
	      wvStream_goto (fd, recend);
	  count = recend - base;
      }
    wvTrace (("offset %x, len %d (pos %x)\n", offset, len, wvStream_tell (fd)));
}

void
wvReleaseDggContainer (DggContainer * item)
{
    wvReleaseSplitMenuColors (&item->splitmenucolors);
    wvReleaseDgg (&item->dgg);
    wvReleaseBstoreContainer (&item->bstorecontainer);
}

void
wvInitDggContainer (DggContainer * item)
{
    wvInitSplitMenuColors (&item->splitmenucolors);
    wvInitDgg (&item->dgg);
    wvInitBstoreContainer (&item->bstorecontainer);
}

U32
wvGetDggContainer (DggContainer * item, MSOFBH * msofbh, wvStream * fd,
		   wvStream * delay)
{
    MSOFBH amsofbh;
    U32 count = 0;
    long entry = wvStream_tell (fd);

    while (count < msofbh->cbLength && s_stream_remaining (fd) != 0)
      {
	  long recstart = wvStream_tell (fd);
	  long recend;
	  count += wvGetMSOFBH (&amsofbh, fd);
	  wvTrace (
		   ("len is %x, type is %x, count %x,fullen %x\n",
		    amsofbh.cbLength, amsofbh.fbt, count, msofbh->cbLength));
	  wvTrace (("type is %x\n	", amsofbh.fbt));
	  switch (amsofbh.fbt)
	    {
	    case msofbtDgg:
		count += wvGetDgg (&item->dgg, &amsofbh, fd);
		break;
	    case msofbtSplitMenuColors:
		count +=
		    wvGetSplitMenuColors (&item->splitmenucolors, &amsofbh, fd);
		break;
	    case msofbtBstoreContainer:
		count +=
		    wvGetBstoreContainer (&item->bstorecontainer, &amsofbh,
					  fd, delay);
		if (item->bstorecontainer.no_fbse)
		    wvTrace (
			 ("type is %d (number is %d\n",
			  item->bstorecontainer.blip[item->bstorecontainer.
						     no_fbse - 1].type,
			  item->bstorecontainer.no_fbse));
		break;
	    default:
		count += wvEatmsofbt (&amsofbh, fd);
		wvError (("Eating type 0x%x\n", amsofbh.fbt));
		break;
	    }
	  /* resync the stream to the end of the record whatever the
	     child actually consumed */
	  recend = recstart + 8 + amsofbh.cbLength;
	  if ((long) wvStream_tell (fd) != recend)
	      wvStream_goto (fd, recend);
	  count = recend - entry;
      }

    /* note: the stray pad byte that used to be eaten here lives
       between the top-level records and is now skipped in
       wvGetEscher */
    return (count);
}

void
wvReleaseDgContainer (DgContainer * item)
{
    U32 i;
    for (i = 0; i < item->no_spgrcontainer; i++)
	wvReleaseSpgrContainer (&(item->spgrcontainer[i]));
    wvFree (item->spgrcontainer);

    for (i = 0; i < item->no_spcontainer; i++)
	wvReleaseFSPContainer (&(item->spcontainer[i]));
    wvFree (item->spcontainer);
}

void
wvInitDgContainer (DgContainer * item)
{
    item->fdg.csp = 0;
    item->fdg.spidCur = 0;
    item->no_spgrcontainer = 0;
    item->spgrcontainer = NULL;
    item->no_spcontainer = 0;
    item->spcontainer = NULL;
}

void
wvReleaseBstoreContainer (BstoreContainer * item)
{
    U32 i;
    for (i = 0; i < item->no_fbse; i++)
	wvReleaseBlip (&item->blip[i]);
    wvFree (item->blip);
}

void
wvInitBstoreContainer (BstoreContainer * item)
{
    item->no_fbse = 0;
    item->blip = NULL;
}

U32
wvGetBstoreContainer (BstoreContainer * item, MSOFBH * msofbh, wvStream * fd,
		      wvStream * delay)
{
    MSOFBH amsofbh;
    U32 count = 0;
    long entry = wvStream_tell (fd);
    while (count < msofbh->cbLength && s_stream_remaining (fd) != 0)
      {
	  long recstart = wvStream_tell (fd);
	  long recend;
	  count += wvGetMSOFBH (&amsofbh, fd);
	  wvTrace (("type is %x\n	", amsofbh.fbt));
	  switch (amsofbh.fbt)
	    {
	    case msofbtBSE:
		wvTrace (("Blip at %x\n", wvStream_tell (fd)));
		item->no_fbse++;
		item->blip =
		    (Blip *) realloc (item->blip,
				      sizeof (Blip) * item->no_fbse);
		count +=
		    wvGetBlip ((&item->blip[item->no_fbse - 1]), fd, delay);
		wvTrace (
			 ("type is %d (number is %d\n",
			  item->blip[item->no_fbse - 1].type, item->no_fbse));
		break;
	    default:
		/* an OfficeArtBStoreContainerFileBlock can also be a
		   bare OfficeArtBlip record (0xF018-0xF117): it still
		   occupies one rgfb slot, which the pib index counts */
		if (amsofbh.fbt >= msofbtBlipFirst && amsofbh.fbt <= 0xF117)
		  {
		      wvTrace (("Bare blip at %x\n", wvStream_tell (fd)));
		      item->no_fbse++;
		      item->blip =
			  (Blip *) realloc (item->blip,
					    sizeof (Blip) * item->no_fbse);
		      item->blip[item->no_fbse - 1].fbse.cbName = 0;
		      wvGetBlipRecord (&(item->blip[item->no_fbse - 1]),
				       &amsofbh, fd);
		  }
		else
		  {
		      count += wvEatmsofbt (&amsofbh, fd);
		      wvError (("Eating type 0x%x\n", amsofbh.fbt));
		  }
		break;
	    }
	  recend = recstart + 8 + amsofbh.cbLength;
	  if ((long) wvStream_tell (fd) != recend)
	      wvStream_goto (fd, recend);
	  count = recend - entry;
      }
    return (count);
}

U32
wvGetDgContainer (DgContainer * item, MSOFBH * msofbh, wvStream * fd)
{
    MSOFBH amsofbh;
    U32 count = 0;
    long entry = wvStream_tell (fd);

    item->spcontainer = NULL;
    item->no_spcontainer = 0;

    while (count < msofbh->cbLength && s_stream_remaining (fd) != 0)
      {
	  long recstart = wvStream_tell (fd);
	  long recend;
	  count += wvGetMSOFBH (&amsofbh, fd);
	  wvTrace (
		   ("len is %x, type is %x, count %x,fullen %x\n",
		    amsofbh.cbLength, amsofbh.fbt, count, msofbh->cbLength));
	  wvTrace (("type is %x\n	", amsofbh.fbt));
	  switch (amsofbh.fbt)
	    {
	    case msofbtDg:
		count += wvGetFDG (&item->fdg, fd);
		break;
	    case msofbtSpgrContainer:
		item->no_spgrcontainer++;
		item->spgrcontainer =
		    (SpgrContainer *) realloc (item->spgrcontainer,
					       sizeof (SpgrContainer) *
					       item->no_spgrcontainer);
		count +=
		    wvGetSpgrContainer (&
					(item->spgrcontainer
					 [item->no_spgrcontainer - 1]), &amsofbh, fd);
		break;
		case msofbtSpContainer:
	      	item->no_spcontainer++;
		item->spcontainer =
		    (FSPContainer *) realloc (item->spcontainer,
					       sizeof (FSPContainer) *
					       item->no_spcontainer);
		count +=
		    wvGetFSPContainer (&
	        			(item->spcontainer
					 [item->no_spcontainer - 1]), &amsofbh, fd);
		break;
	    default:
		count += wvEatmsofbt (&amsofbh, fd);
		wvError (("Eating type 0x%x\n", amsofbh.fbt));
		break;
	    }
	  recend = recstart + 8 + amsofbh.cbLength;
	  if ((long) wvStream_tell (fd) != recend)
	      wvStream_goto (fd, recend);
	  count = recend - entry;
      }
    return (count);
}

FSPContainer *
wvFindSPID (SpgrContainer * item, S32 spid)
{
    U32 i;
    FSPContainer *t;
    for (i = 0; i < item->no_spcontainer; i++)
      {
	  /* FIXME: Cast below is to avoid compiler warnings, but having
	     to have it could be a sign of something wrong. */
	  if (item->spcontainer[i].fsp.spid == (U32) spid)
	    {
		wvTrace (("FOUND IT\n"));
		return (&(item->spcontainer[i]));
	    }
      }
    for (i = 0; i < item->no_spgrcontainer; i++)
      {
	  t = wvFindSPID (&(item->spgrcontainer[i]), spid);
	  if (t)
	      return (t);
      }
    return (NULL);
}


void
wvReleaseSpgrContainer (SpgrContainer * item)
{
    U32 i;
    for (i = 0; i < item->no_spcontainer; i++)
	wvReleaseFSPContainer (&(item->spcontainer[i]));
    wvFree (item->spcontainer);
    for (i = 0; i < item->no_spgrcontainer; i++)
	wvReleaseSpgrContainer (&(item->spgrcontainer[i]));
    wvFree (item->spgrcontainer);
}


U32
wvGetSpgrContainer (SpgrContainer * item, MSOFBH * msofbh, wvStream * fd)
{
    MSOFBH amsofbh;
    U32 count = 0;
    long entry = wvStream_tell (fd);

    item->spgrcontainer = NULL;
    item->no_spgrcontainer = 0;
    item->spcontainer = NULL;
    item->no_spcontainer = 0;

    while (count < msofbh->cbLength && s_stream_remaining (fd) != 0)
      {
	  long recstart = wvStream_tell (fd);
	  long recend;
	  count += wvGetMSOFBH (&amsofbh, fd);
	  wvTrace (
		   ("len is %x, type is %x, count %x,fullen %x\n",
		    amsofbh.cbLength, amsofbh.fbt, count, msofbh->cbLength));
	  wvTrace (("type is %x\n	", amsofbh.fbt));
	  switch (amsofbh.fbt)
	    {
	    case msofbtSpContainer:
		item->no_spcontainer++;
		item->spcontainer =
		    realloc (item->spcontainer,
			     sizeof (FSPContainer) * item->no_spcontainer);
		count +=
		    wvGetFSPContainer (&
				       (item->spcontainer[item->no_spcontainer -
							  1]), &amsofbh, fd);
		break;
	    case msofbtSpgrContainer:
		item->no_spgrcontainer++;
		item->spgrcontainer =
		    realloc (item->spgrcontainer,
			     sizeof (SpgrContainer) * item->no_spgrcontainer);
		count +=
		    wvGetSpgrContainer (&
					(item->spgrcontainer
					 [item->no_spgrcontainer - 1]), &amsofbh, fd);
		break;
	    default:
		count += wvEatmsofbt (&amsofbh, fd);
		wvError (("Eating type 0x%x\n", amsofbh.fbt));
		break;
	    }
	  recend = recstart + 8 + amsofbh.cbLength;
	  if ((long) wvStream_tell (fd) != recend)
	      wvStream_goto (fd, recend);
	  count = recend - entry;
      }
    return (count);
}


U32
wvGetFDG (FDG * afdg, wvStream * fd)
{
    afdg->csp = read_32ubit (fd);
    afdg->spidCur = read_32ubit (fd);
    wvTrace (
	     ("there are %d shapes here, the last is %x\n", afdg->csp,
	      afdg->spidCur));
    return (8);
}


void
wvInitSplitMenuColors (SplitMenuColors * splitmenucolors)
{
    splitmenucolors->noofcolors = 0;
    splitmenucolors->colors = NULL;
}

void
wvReleaseSplitMenuColors (SplitMenuColors * splitmenucolors)
{
    wvFree (splitmenucolors->colors);
}

U32
wvGetSplitMenuColors (SplitMenuColors * splitmenucolors, MSOFBH * amsofbh,
		      wvStream * fd)
{
    U32 i = 0;
    splitmenucolors->noofcolors = amsofbh->cbLength / 4;
    /* cbLength is file-controlled: never claim more colors than the
       stream still holds */
    if (splitmenucolors->noofcolors > s_stream_remaining (fd) / 4)
	splitmenucolors->noofcolors = s_stream_remaining (fd) / 4;
    if (splitmenucolors->noofcolors)
      {
	  splitmenucolors->colors =
	      (U32 *) wvMalloc (sizeof (U32) * splitmenucolors->noofcolors);
	  for (i = 0; i < splitmenucolors->noofcolors; i++)
	      splitmenucolors->colors[i] = read_32ubit (fd);
      }
    return (i * 4);
}

void
wvReleaseDgg (Dgg * dgg)
{
    wvFree (dgg->fidcl);
}

void
wvInitDgg (Dgg * dgg)
{
    memset (&dgg->fdgg, 0, sizeof (FDGG));
    dgg->fidcl = NULL;
}

U32
wvGetDgg (Dgg * dgg, MSOFBH * amsofbh, wvStream * fd)
{
    U32 count = 0;
    U32 no;
    U32 i;
    count += wvGetFDGG (&dgg->fdgg, fd);
    if (dgg->fdgg.cidcl != 0)
      {
	  /* cbLength is file-controlled: a record shorter than the
	     FDGG it claims must not wrap this subtraction into a ~4GB
	     malloc, and only as many FIDCLs as the stream still holds
	     can exist */
	  if (amsofbh->cbLength <= count)
	      no = 0;
	  else
	      no = (amsofbh->cbLength - count) / 8;
	  if (no > s_stream_remaining (fd) / 8)
	      no = s_stream_remaining (fd) / 8;
	  wvTrace (("There are %d bytes left\n", no * 8));
	  if (no != dgg->fdgg.cidcl)
	    {
		wvWarning
		    ("Must be %d, not %d as specs, test algor gives %d\n", no,
		     dgg->fdgg.cidcl, dgg->fdgg.cspSaved - dgg->fdgg.cidcl);
	    }
	  if (no)
	    {
		dgg->fidcl = (FIDCL *) wvMalloc (sizeof (FIDCL) * no);
		for (i = 0; i < no; i++)
		    count += wvGetFIDCL (&(dgg->fidcl[i]), fd);
	    }
      }
    return (count);
}

U32
wvGetFIDCL (FIDCL * afidcl, wvStream * fd)
{
    afidcl->dgid = read_32ubit (fd);
    afidcl->cspidCur = read_32ubit (fd);
    wvTrace (("dgid %d cspidCur %d\n", afidcl->dgid, afidcl->cspidCur));
    return (8);
}


U32
wvGetFDGG (FDGG * afdgg, wvStream * fd)
{
    afdgg->spidMax = read_32ubit (fd);
    afdgg->cidcl = read_32ubit (fd);
    afdgg->cspSaved = read_32ubit (fd);
    afdgg->cdgSaved = read_32ubit (fd);
    wvTrace (
	     ("spidMax %d cidcl %d cspSaved %d cdgSaved %d\n", afdgg->spidMax,
	      afdgg->cidcl, afdgg->cspSaved, afdgg->cdgSaved));
    return (16);
}


void
wvGetDocEscher (wvParseStruct * ps, escherstruct * item)
{
    /* the OfficeArt delay stream for Word documents is the Data
       stream, not the main stream */
    wvGetEscher (item, ps->fib.fcDggInfo, ps->fib.lcbDggInfo, ps->tablefd,
		 ps->data);
}

U32
wvGetStoreBlipCount (const escherstruct * item)
{
    if (!item)
	return (0);
    return (item->dggcontainer.bstorecontainer.no_fbse);
}

int
wvGetStoreBlip (escherstruct * item, U32 pib, Blip * blip)
{
    Blip *src;
    BstoreContainer *store;

    if (!item || !blip || pib < 1)
	return (0);
    store = &item->dggcontainer.bstorecontainer;
    if (pib > store->no_fbse)
	return (0);
    src = &store->blip[pib - 1];
    wvCopyBlip (blip, src);
    /* wvCopyBlip only shares the payload stream; MOVE it into the
       copy instead so the returned Blip is its sole owner and the
       store slot can be released independently */
    switch (blip->type)
      {
      case msoblipJPEG:
      case msoblipPNG:
      case msoblipDIB:
	  src->blip.bitmap.m_pvBits = NULL;
	  break;
      case msoblipWMF:
      case msoblipEMF:
      case msoblipPICT:
	  src->blip.metafile.m_pvBits = NULL;
	  break;
      }
    return (1);
}

int
wvFindBlipBySPID (escherstruct * item, S32 spid, Blip * blip)
{
    U32 i;
    FSPContainer *answer = NULL;

    if (!item || !blip)
	return (0);
    wvTrace (("spid is %x\n", spid));

    for (i = 0; i < item->dgcontainer.no_spgrcontainer; i++)
      {
	  answer = wvFindSPID (&(item->dgcontainer.spgrcontainer[i]), spid);
	  if (answer)
	      break;
      }

    i = 0;
    if (answer == NULL)
	wvError (("Damn found nothing\n"));
    else if (answer->fopte)
      {
	  while (answer->fopte[i].pid != 0)
	    {
		/* 260 == 0x104 == the pib property: a 1-based index into
		   the blip store */
		if (answer->fopte[i].pid == 260)
		  {
		      wvTrace (
			       ("has a blip reference of %d\n",
				answer->fopte[i].op));
		      wvTrace (
			       ("no blips is %d\n",
				item->dggcontainer.bstorecontainer.no_fbse));
		      if (wvGetStoreBlip (item, answer->fopte[i].op, blip))
			{
			    wvTrace (("Copied Blip\n"));
			    wvTrace (("type is %d\n", blip->type));
			    return (1);
			}
		  }
		i++;
	    }
      }
    wvTrace (("spid is %x\n", spid));
    return (0);
}

int
wv0x08 (Blip * blip, S32 spid, wvParseStruct * ps)
{
    int ret;
    escherstruct item;

    wvGetDocEscher (ps, &item);
    ret = wvFindBlipBySPID (&item, spid, blip);
    wvReleaseEscher (&item);
    return (ret);
}

/*
  Walk the records of a picture data region (a real
  OfficeArtInlineSpContainer from a Word8 PICF payload, or the
  synthesized escher wrapper that wvGetPICF builds for pre-Word8
  data) looking for blip payloads: msofbtBSE records and bare
  OfficeArtBlip records (0xF018-0xF117).  Shape containers are
  recursed into, everything else is skipped in place.
*/
static int
wvFindBlipInRegion (Blip * blip, wvStream * fd, U32 len, wvStream * delay,
		    FOPTE ** shapeprops)
{
    MSOFBH amsofbh;
    U32 count = 0;
    int ret = 0;
    long base = wvStream_tell (fd);

    while (count < len && s_stream_remaining (fd) != 0)
      {
	  long recstart = wvStream_tell (fd);
	  long recend;
	  count += wvGetMSOFBH (&amsofbh, fd);
	  wvTrace (("type is %x\n	", amsofbh.fbt));
	  if (amsofbh.fbt == msofbtBSE)
	    {
		wvTrace (("Blip at %x\n", wvStream_tell (fd)));
		count += wvGetBlip (blip, fd, delay);
		ret = 1;
	    }
	  else if (amsofbh.fbt >= msofbtBlipFirst && amsofbh.fbt <= 0xF117)
	    {
		wvTrace (("Bare blip at %x\n", wvStream_tell (fd)));
		count += wvGetBlipRecord (blip, &amsofbh, fd);
		ret = 1;
	    }
	  else if (amsofbh.fbt == msofbtOPT && shapeprops && !*shapeprops)
	    {
		/* the first OPT record in a picture region is the
		   picture shape's property table: hang on to it so the
		   caller can read size/crop/geometry props */
		count += wvGetFOPTEArray (shapeprops, &amsofbh, fd);
	    }
	  else if (amsofbh.ver == 0xF)
	    {
		/* container: recurse into it */
		wvTrace (("Container at %x\n", wvStream_tell (fd)));
		if (wvFindBlipInRegion (blip, fd, amsofbh.cbLength, delay,
					shapeprops))
		    ret = 1;
	    }
	  else
	    wvStream_offset (fd, amsofbh.cbLength);

	  /* resync to the declared end of the record */
	  recend = recstart + 8 + amsofbh.cbLength;
	  if ((long) wvStream_tell (fd) != recend)
	      wvStream_goto (fd, recend);
	  count = recend - base;
      }
    return (ret);
}

int
wv0x01 (Blip * blip, wvStream * fd, U32 len, wvStream * delay,
	FOPTE ** shapeprops)
{
    if (shapeprops)
	*shapeprops = NULL;
    if (fd == NULL)
	return (0);

    return wvFindBlipInRegion (blip, fd, len, delay, shapeprops);
}

U32
wvGetFSP (FSP * fsp, wvStream * fd)
{
    fsp->spid = read_32ubit (fd);
    wvTrace (("SPID is %x\n", fsp->spid));
    fsp->grfPersistent = read_32ubit (fd);
    return (8);
}


U32
wvGetFSPGR (FSPGR * item, wvStream * fd)
{
    /* It is supposed to be a RECT, but its only 4 long so... */
    item->rcgBounds.left = read_32ubit (fd);
    item->rcgBounds.right = read_32ubit (fd);
    item->rcgBounds.top = read_32ubit (fd);
    item->rcgBounds.bottom = read_32ubit (fd);
    return (16);
}

void
wvReleaseFSPContainer (FSPContainer * item)
{
    wvReleaseClientTextbox (&item->clienttextbox);
    wvReleaseClientData (&item->clientdata);
    wvReleaseFOPTEArray (&item->fopte);
}

void
wvInitFSPContainer (FSPContainer * item)
{
    memset (&item->fspgr, 0, sizeof (FSPGR));
    item->fsp.spid = 0;
    item->fsp.grfPersistent = 0;
    memset (&item->fanchor, 0, sizeof (FAnchor));
    wvInitFOPTEArray (&item->fopte);
    wvInitClientData (&item->clientdata);
    wvInitClientTextbox (&item->clienttextbox);
}

U32
wvGetFSPContainer (FSPContainer * item, MSOFBH * msofbh, wvStream * fd)
{
    MSOFBH amsofbh;
    U32 count = 0;
    long entry = wvStream_tell (fd);
    wvInitFSPContainer (item);
    while (count < msofbh->cbLength && s_stream_remaining (fd) != 0)
      {
	  long recstart = wvStream_tell (fd);
	  long recend;
	  count += wvGetMSOFBH (&amsofbh, fd);
	  wvTrace (
		   ("len is %x, type is %x, count %x,fullen %x\n",
		    amsofbh.cbLength, amsofbh.fbt, count, msofbh->cbLength));
	  wvTrace (("type is %x\n	", amsofbh.fbt));
	  switch (amsofbh.fbt)
	    {
	    case msofbtSpgr:
		count += wvGetFSPGR (&item->fspgr, fd);
		break;

	    case msofbtSp:
		wvTrace (("Getting an fsp\n"));
		count += wvGetFSP (&item->fsp, fd);
		break;

	    case msofbtOPT:
		count += wvGetFOPTEArray (&item->fopte, &amsofbh, fd);
		break;

	    case msofbtAnchor:
	    case msofbtChildAnchor:
	    case msofbtClientAnchor:
		count += wvGetFAnchor (&item->fanchor, fd);
		break;

	    case msofbtClientData:
		count += wvGetClientData (&item->clientdata, &amsofbh, fd);
		break;
	    case msofbtClientTextbox:
		count +=
		    wvGetClientTextbox (&item->clienttextbox, &amsofbh, fd);
		break;

	    case msofbtTextbox:
	    case msofbtOleObject:
	    case msofbtDeletedPspl:
		/* unimplemented: eat the record so we keep advancing */
		count += wvEatmsofbt (&amsofbh, fd);
		break;

	    default:
		count += wvEatmsofbt (&amsofbh, fd);
		wvError (("Eating type 0x%x\n", amsofbh.fbt));
		break;
	    }
	  recend = recstart + 8 + amsofbh.cbLength;
	  if ((long) wvStream_tell (fd) != recend)
	      wvStream_goto (fd, recend);
	  count = recend - entry;
      }
    return (count);
}

void
wvInitClientData (ClientData * item)
{
    item->data = NULL;
}

void
wvReleaseClientData (ClientData * item)
{
    wvFree (item->data);
}

U32
wvGetClientData (ClientData * item, MSOFBH * msofbh, wvStream * fd)
{
    U32 i, n = msofbh->cbLength;
    /* cbLength is file-controlled: never allocate or read more than
       the stream actually holds */
    if (n > s_stream_remaining (fd))
	n = s_stream_remaining (fd);
    if (n)
      {
	  item->data = (U8 *) wvMalloc (n);
	  for (i = 0; i < n; i++)
	      item->data[i] = read_8ubit (fd);
      }
    else
	item->data = NULL;
    return (msofbh->cbLength);
}

U32
wvGetMSOFBH (MSOFBH * amsofbh, wvStream * fd)
{
    U16 dtemp = 0;
    dtemp = read_16ubit (fd);

#ifdef PURIFY
    amsofbh->ver = 0;
    amsofbh->inst = 0;
#endif

    amsofbh->ver = dtemp & 0x000F;
    amsofbh->inst = dtemp >> 4;
    amsofbh->fbt = read_16ubit (fd);
    amsofbh->cbLength = read_32ubit (fd);
    return (8);
}


U32
wvEatmsofbt (MSOFBH * amsofbh, wvStream * fd)
{
    wvStream_offset(fd, amsofbh->cbLength);
    return amsofbh->cbLength;
}

void
wvInitClientTextbox (ClientTextbox * item)
{
    item->textid = NULL;
}

void
wvReleaseClientTextbox (ClientTextbox * item)
{
    wvFree (item->textid);
}

U32
wvGetClientTextbox (ClientTextbox * item, MSOFBH * amsofbh, wvStream * fd)
{
    /* AbiWord: cbLength is file-controlled; a record shorter than a
       U32 still allocates it, so always reserve 4 bytes and check.
       Only the first U32 is ever read -- cap the reservation by what
       the stream can actually hold */
    U32 n = amsofbh->cbLength;
    if (n > s_stream_remaining (fd))
	n = s_stream_remaining (fd);
    item->textid = (U32 *) wvMalloc (n < 4 ? 4 : n);
    if (item->textid)
	*item->textid = read_32ubit (fd);
    return (amsofbh->cbLength);
}
