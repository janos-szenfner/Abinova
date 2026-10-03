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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wv.h"
#include "wvinternal.h"

void
wvCopyBlip (Blip * dest, Blip * src)
{
    int i;
    wvCopyFBSE (&dest->fbse, &src->fbse);
    dest->type = src->type;

    if (src->name)
      {
	  /* fbse.cbName is a byte count; name holds cbName/2 U16s */
	  dest->name = (U16 *) wvMalloc (src->fbse.cbName);
	  for (i = 0; i + 1 < src->fbse.cbName; i += 2)
	      dest->name[i / 2] = src->name[i / 2];
      }
    else
	dest->name = NULL;
    switch (dest->type)
      {
      case msoblipWMF:
      case msoblipEMF:
      case msoblipPICT:
	  wvCopyMetafile (&dest->blip.metafile, &(src->blip.metafile));
	  break;
      case msoblipJPEG:
      case msoblipPNG:
      case msoblipDIB:
	  wvCopyBitmap (&dest->blip.bitmap, &(src->blip.bitmap));
	  break;
      }
}

void
wvReleaseBlip (Blip * blip)
{
    wvFree (blip->name);
    /* a Blip owns its m_pvBits stream: wvGetStoreBlip steals it
       into the caller's copy so ownership is never shared, and
       wvExtractBlipData clears it after consumption -- what is
       left here is genuinely owned.  ERROR/UNKNOWN slots never
       had their union written, so only touch it for the types
       that carry an m_pvBits.  wvStream_close tolerates NULL. */
    switch (blip->type)
      {
      case msoblipJPEG:
      case msoblipPNG:
      case msoblipDIB:
	  wvStream_close (blip->blip.bitmap.m_pvBits);
	  blip->blip.bitmap.m_pvBits = NULL;
	  break;
      case msoblipWMF:
      case msoblipEMF:
      case msoblipPICT:
	  wvStream_close (blip->blip.metafile.m_pvBits);
	  blip->blip.metafile.m_pvBits = NULL;
	  break;
      }
}

/*
  AbiWord: pull the image bytes out of a parsed Blip into a fresh
  wvMalloc'd buffer.  The blip's m_pvBits stream is consumed:
  closed and cleared, so a second call reports failure and
  wvReleaseBlip no longer touches it.  Returns 1 and sets *data
  (caller wvFree()s), *len and *type on success.  Metafile blips
  that were stored compressed keep their compressed bytes here --
  check MetaFileBlip.m_fCompression before extracting if the
  uncompressed form is needed.
*/
int
wvExtractBlipData (Blip * blip, U8 ** data, U32 * len, U16 * type)
{
    wvStream *bits = NULL;

    if (!blip || !data || !len || !type)
	return (0);
    *data = NULL;
    *len = 0;
    *type = blip->type;
    switch (blip->type)
      {
      case msoblipJPEG:
      case msoblipPNG:
      case msoblipDIB:
	  bits = blip->blip.bitmap.m_pvBits;
	  blip->blip.bitmap.m_pvBits = NULL;
	  break;
      case msoblipWMF:
      case msoblipEMF:
      case msoblipPICT:
	  bits = blip->blip.metafile.m_pvBits;
	  blip->blip.metafile.m_pvBits = NULL;
	  break;
      default:
	  return (0);
      }
    if (!bits)
	return (0);

    *len = wvStream_size (bits);
    *data = (U8 *) wvMalloc (*len ? *len : 1);
    if (!*data)
      {
	  *len = 0;
	  wvStream_close (bits);
	  return (0);
      }
    wvStream_rewind (bits);
    wvStream_read (*data, 1, *len, bits);
    wvStream_close (bits);
    return (1);
}

/*
  Read the payload of one OfficeArtBlip record whose header (amsofbh)
  has already been consumed from fd.  Returns payload bytes consumed.
  Unknown record types are skipped so the caller's stream stays
  aligned; blip->type is msoblipERROR then.
*/
static U32
wvGetBlipPayload (Blip * blip, MSOFBH * amsofbh, wvStream * fd)
{
    U32 count2 = 0;
    U16 type;

    wvTrace (
	     ("HERE is %x %x (%d)\n", wvStream_tell (fd), amsofbh->fbt,
	      amsofbh->fbt - msofbtBlipFirst));
    type = (U16) (amsofbh->fbt - msofbtBlipFirst);
    switch (type)
      {
      case msoblipWMF:
      case msoblipEMF:
      case msoblipPICT:
	  count2 += wvGetMetafile (&blip->blip.metafile, amsofbh, fd);
	  blip->type = type;
	  break;
      case msoblipJPEG:
      case msoblipPNG:
      case msoblipDIB:
	  count2 += wvGetBitmap (&blip->blip.bitmap, amsofbh, fd);
	  blip->type = type;
	  break;
      default:
	  /* not a blip we can use (TIFF, client blips, or a misparse):
	     eat the record so the caller's stream stays aligned */
	  blip->type = msoblipERROR;
	  wvStream_offset (fd, amsofbh->cbLength);
	  count2 += amsofbh->cbLength;
	  break;
      }
    return (count2);
}

/*
  Read one OfficeArtBlip record (header + payload) from fd into blip.
  Returns bytes consumed.
*/
static U32
wvGetBlipData (Blip * blip, wvStream * fd)
{
    MSOFBH amsofbh;
    U32 count2;

    count2 = wvGetMSOFBH (&amsofbh, fd);
    count2 += wvGetBlipPayload (blip, &amsofbh, fd);
    return (count2);
}

/*
  Read a bare OfficeArtBlip record (0xF018-0xF117) -- the kind stored
  directly in an OfficeArtBStoreContainerFileBlock or in the rgfb of
  an OfficeArtInlineSpContainer, i.e. without a wrapping FBSE.  The
  caller has already consumed the record header; it is passed in via
  amsofbh so only the payload is read here.
*/
U32
wvGetBlipRecord (Blip * blip, MSOFBH * amsofbh, wvStream * fd)
{
    memset (&blip->fbse, 0, sizeof (FBSE));
    blip->name = NULL;
    return wvGetBlipPayload (blip, amsofbh, fd);
}

U32
wvGetBlip (Blip * blip, wvStream * fd, wvStream * delay)
{
    U32 i, count, count2;
    MSOFBH amsofbh;
    long pos = 0;
    int delayed = 0;

    count = wvGetFBSE (&blip->fbse, fd);
    wvTrace (("count is %d\n", count));

    /* cbName is the length in BYTES of the UTF-16LE nameData field
       (even, <=0xfe); an odd byte is padding */
    if (blip->fbse.cbName == 0)
	blip->name = NULL;
    else
	blip->name = (U16 *) wvMalloc (blip->fbse.cbName);
    if (blip->name)
      {
	  for (i = 0; i + 1 < blip->fbse.cbName; i += 2)
	      blip->name[i / 2] = read_16ubit (fd);
	  if (blip->fbse.cbName & 1)
	      read_8ubit (fd);
      }
    else
	wvStream_offset (fd, blip->fbse.cbName);
    count += blip->fbse.cbName;
    wvTrace (("count is %d\n", count));
    wvTrace (("offset %x\n", blip->fbse.foDelay));

    /* foDelay == 0xffffffff means the blip is embedded in this record;
       anything else is an offset into the delay stream (the Data
       stream for Word) */
    if (blip->fbse.foDelay != 0xffffffffUL)
      {
	  if (delay && blip->fbse.foDelay < wvStream_size (delay))
	    {
		pos = wvStream_tell (delay);
		wvStream_goto (delay, blip->fbse.foDelay);
		wvTrace (("offset %x\n", blip->fbse.foDelay));
		fd = delay;
		delayed = 1;
	    }
	  else
	    {
		/* no delay stream available (or a bogus offset): peek
		   whether the blip was embedded anyway; if not, report
		   an empty slot but stay aligned for the next record */
		long save = wvStream_tell (fd);
		U16 fbt;
		read_16ubit (fd);
		fbt = read_16ubit (fd);
		wvStream_goto (fd, save);
		if (fbt < msofbtBlipFirst || fbt > 0xF117)
		  {
		      blip->type = msoblipERROR;
		      return (count);
		  }
	    }
      }

    count2 = wvGetBlipData (blip, fd);
    wvTrace (("count is %d\n", count2));

    if (delayed)
      {
	  wvStream_goto (delay, pos);
	  return (count);
      }

    return (count + count2);
}

U32
wvGetFBSE (FBSE * afbse, wvStream * fd)
{
    int i;
    afbse->btWin32 = read_8ubit (fd);
    afbse->btMacOS = read_8ubit (fd);
    for (i = 0; i < 16; i++)
	afbse->rgbUid[i] = read_8ubit (fd);
    afbse->tag = read_16ubit (fd);
    afbse->size = read_32ubit (fd);
    afbse->cRef = read_32ubit (fd);
    afbse->foDelay = read_32ubit (fd);
    wvTrace (("location is %x, size is %d\n", afbse->foDelay, afbse->size));
    afbse->usage = read_8ubit (fd);
    afbse->cbName = read_8ubit (fd);
    wvTrace (("name len is %d\n", afbse->cbName));
    afbse->unused2 = read_8ubit (fd);
    afbse->unused3 = read_8ubit (fd);
    return (36);
}

void
wvCopyFBSE (FBSE * dest, FBSE * src)
{
    memcpy (dest, src, sizeof (FBSE));
}


U32
wvGetBitmap (BitmapBlip * abm, MSOFBH * amsofbh, wvStream * fd)
{
    U32 i, count;
    char extra = 0;
    wvStream * stm = NULL;
    wvTrace (("starting bitmap at %x\n", wvStream_tell (fd)));
    for (i = 0; i < 16; i++)
	abm->m_rgbUid[i] = read_8ubit (fd);
    count = 16;

    abm->m_rgbUidPrimary[0] = 0;

    switch (amsofbh->fbt - msofbtBlipFirst)
      {
      case msoblipPNG:
	  wvTrace (("msoblipPNG\n"));
	  break;
      case msoblipJPEG:
	  wvTrace (("msoblipJPEG\n"));
	  break;
      case msoblipDIB:
	  wvTrace (("msoblipDIB\n"));
	  break;
      }

    /* per MS-ODRAW the two-UID variants are exactly the odd
       recInstance values (0x217/0x3D5/0x543/0x46B/0x6E1/0x6E3/0x7A9);
       testing inst^sig!=0 would wrongly eat a second UID for
       e.g. JPEG 0x6E2 */
    if (amsofbh->inst & 1)
      {
	  for (i = 0; i < 16; i++)
	      abm->m_rgbUidPrimary[i] = read_8ubit (fd);
	  count += 16;
      }

    abm->m_bTag = read_8ubit (fd);
    abm->m_pvBits = NULL;

    count++;
    if (amsofbh->cbLength <= count)
	return amsofbh->cbLength;

    U32 datalen = amsofbh->cbLength - count;
    long avail = (long) wvStream_size (fd) - (long) wvStream_tell (fd);
    if (avail < 0)
	avail = 0;
    if (datalen > (U32) avail)
	datalen = (U32) avail;

    /* the stream must hold exactly the image bytes, not cbLength:
       the uid/tag header is not part of the image data */
    stm = wvStream_TMP_create (datalen);
    if (!stm)
	return count;

    char *tmp = wvMalloc (datalen ? datalen : 1);
    if (!tmp)
	return count;
    wvStream_read (tmp, 1, datalen, fd);
    wvStream_write (tmp, 1, datalen, stm);
    wvFree (tmp);

    wvStream_rewind (stm);

    abm->m_pvBits = stm;

    return (count + datalen);
}

void
wvCopyBitmap (BitmapBlip * dest, BitmapBlip * src)
{
    U8 i;
    for (i = 0; i < 16; i++)
      {
	  dest->m_rgbUid[i] = src->m_rgbUid[i];
	  dest->m_rgbUidPrimary[i] = src->m_rgbUidPrimary[i];
      }

    dest->m_bTag = src->m_bTag;
    dest->m_pvBits = src->m_pvBits;
}


U32
wvGetMetafile (MetaFileBlip * amf, MSOFBH * amsofbh, wvStream * fd)
{
    char extra = 0;
    U32 i, count;
    wvStream * stm = 0;
    char *buf, *p;

    for (i = 0; i < 16; i++)
	amf->m_rgbUid[i] = read_8ubit (fd);
    count = 16;

    amf->m_rgbUidPrimary[0] = 0;

    switch (amsofbh->fbt - msofbtBlipFirst)
      {
      case msoblipEMF:
	  wvTrace (("msoblipEMF\n"));
	  break;
      case msoblipWMF:
	  wvTrace (("msoblipWMF\n"));
	  break;
      case msoblipPICT:
	  wvTrace (("msoblipPICT\n"));
	  break;
      }

    /* two-UID variants are the odd recInstance values (see
       wvGetBitmap) */
    if (amsofbh->inst & 1)
      {
	  for (i = 0; i < 16; i++)
	      amf->m_rgbUidPrimary[i] = read_8ubit (fd);
	  count += 16;
      }


    amf->m_cb = read_32ubit (fd);
    amf->m_rcBounds.bottom = read_32ubit (fd);
    amf->m_rcBounds.top = read_32ubit (fd);
    amf->m_rcBounds.right = read_32ubit (fd);
    amf->m_rcBounds.left = read_32ubit (fd);
    amf->m_ptSize.y = read_32ubit (fd);
    amf->m_ptSize.x = read_32ubit (fd);
    amf->m_cbSave = read_32ubit (fd);
    amf->m_fCompression = read_8ubit (fd);
    amf->m_fFilter = read_8ubit (fd);
    amf->m_pvBits = NULL;
    count += 34;

    if (amsofbh->cbLength <= count)
	return amsofbh->cbLength;

    U32 datalen = amsofbh->cbLength - count;
    long avail = (long) wvStream_size (fd) - (long) wvStream_tell (fd);
    if (avail < 0)
	avail = 0;
    if (datalen > (U32) avail)
	datalen = (U32) avail;

    buf = wvMalloc (datalen ? datalen : 1);
    if (!buf)
	return count;
    p = buf;

    for (i = 0; i < datalen; i++)
	*p++ = read_8ubit (fd);
    count += i;

    /* the stream must hold exactly the metafile bytes */
    wvStream_memory_create (&stm, buf, datalen);

    amf->m_pvBits = stm;

    return (count);
}


void wvCopyMetafile (MetaFileBlip * dest,
		     MetaFileBlip * src)
{
  U8 i; for (i = 0; i < 16; i++)
    {
      dest->m_rgbUid[i] = src->m_rgbUid[i];
      dest->m_rgbUidPrimary[i] = src->m_rgbUidPrimary[i];}
  dest->m_cb = src->m_cb;
  dest->m_rcBounds.bottom = src->m_rcBounds.bottom;
  dest->m_rcBounds.top = src->m_rcBounds.top;
  dest->m_rcBounds.right = src->m_rcBounds.right;
  dest->m_rcBounds.left = src->m_rcBounds.left;
  dest->m_ptSize.y = src->m_ptSize.y;
  dest->m_ptSize.x = src->m_ptSize.x;
  dest->m_cbSave = src->m_cbSave;
  dest->m_fCompression = src->m_fCompression;
  dest->m_fFilter = src->m_fFilter;
  dest->m_pvBits = src->m_pvBits;
}

/* TODO: code wvPutBlip(), wvPutMetafile() */

void
wvPutFBSE (FBSE * item, wvStream * fd)
{
    int i;

    write_8ubit (fd, item->btWin32);
    write_8ubit (fd, item->btMacOS);

    for (i = 0; i < 16; i++)
	write_8ubit (fd, item->rgbUid[i]);

    write_16ubit (fd, item->tag);
    write_32ubit (fd, item->size);
    write_32ubit (fd, item->cRef);
    write_32ubit (fd, item->foDelay);
    write_8ubit (fd, item->usage);
    write_8ubit (fd, item->cbName);
    write_8ubit (fd, item->unused2);
    write_8ubit (fd, item->unused3);
}
