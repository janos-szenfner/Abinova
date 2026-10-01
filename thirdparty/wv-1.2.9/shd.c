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

void
wvGetSHD_internal (SHD * item, wvStream * fd, U8 * pointer)
{
    U16 temp16;
#ifdef PURIFY
    wvInitSHD (item);
#endif
    temp16 = dread_16ubit (fd, &pointer);
    item->icoFore = temp16 & 0x001F;
    item->icoBack = (temp16 & 0x03E0) >> 5;
    item->ipat = (temp16 & 0xFC00) >> 10;
    item->cvFore = 0;
    item->cvBack = 0;
    item->ipatFull = 0;
    item->fCv = 0;
}

void
wvGetSHD (SHD * item, wvStream * fd)
{
    wvGetSHD_internal (item, fd, NULL);
}

void
wvGetSHDFromBucket (SHD * item, U8 * pointer)
{
    wvGetSHD_internal (item, NULL, pointer);
}

/*
  SHDOperand (MS-DOC 2.9.249): cb (1 byte) then a 10-byte Shd:
  cvFore (4-byte COLORREF), cvBack (4-byte COLORREF), ipat (2-byte Ipat).
  cb MUST be 10; anything else is consumed but not applied.
  Returns the operand length in bytes (1 + cb).
*/
int
wvGetSHDOperandFromBucket (SHD * item, U8 * pointer)
{
    U8 cb;
    U8 *p = pointer;

    cb = dread_8ubit (NULL, &p);
    if (cb >= 10)
      {
	  item->cvFore = dread_32ubit (NULL, &p);
	  item->cvBack = dread_32ubit (NULL, &p);
	  item->ipatFull = dread_16ubit (NULL, &p);
	  item->icoFore = 0;
	  item->icoBack = 0;
	  item->ipat = 0;
	  item->fCv = 1;
      }
    return (cb + 1);
}

void
wvInitSHD (SHD * item)
{
    item->icoFore = 0;
    item->icoBack = 0;
    item->ipat = 0;
    item->cvFore = 0;
    item->cvBack = 0;
    item->ipatFull = 0;
    item->fCv = 0;
}

void
wvCopySHD (SHD * dest, SHD * src)
{
    memcpy (dest, src, sizeof (SHD));
}
