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

#include <gsf/gsf-output-memory.h>

/* ROTATE_LEFT rotates x left n bits , with a bitlen of b*/
#define ROTATE_LEFT(x, n,b) (((x) << (n)) | ((x) >> (b-(n))))

/* MS-OFFCRYPTO 2.3.7.2 helper: rotate a byte right one bit */
#define WV_ROR8(x)	((((x) >> 1) | ((x) << 7)) & 0xFF)

/* MS-OFFCRYPTO 2.3.7.2 PadArray */
static const U8 wv_xor_pad[15] = {
    0xBB, 0xFF, 0xFF, 0xBA, 0xFF, 0xFF, 0xB9, 0x80,
    0x00, 0xBE, 0x0F, 0x00, 0xBF, 0x0F, 0x00
};

/* MS-OFFCRYPTO 2.3.7.2 InitialCode */
static const U16 wv_xor_initial_code[15] = {
    0xE1F0, 0x1D0F, 0xCC9C, 0x84C0, 0x110C,
    0x0E10, 0xF1CE, 0x313E, 0x1872, 0xE139,
    0xD40F, 0x84F9, 0x280C, 0xA96A, 0x4EC3
};

/* MS-OFFCRYPTO 2.3.7.2 XorMatrix */
static const U16 wv_xor_matrix[105] = {
    0xAEFC, 0x4DD9, 0x9BB2, 0x2745, 0x4E8A, 0x9D14, 0x2A09,
    0x7B61, 0xF6C2, 0xFDA5, 0xEB6B, 0xC6F7, 0x9DCF, 0x2BBF,
    0x4563, 0x8AC6, 0x05AD, 0x0B5A, 0x16B4, 0x2D68, 0x5AD0,
    0x0375, 0x06EA, 0x0DD4, 0x1BA8, 0x3750, 0x6EA0, 0xDD40,
    0xD849, 0xA0B3, 0x5147, 0xA28E, 0x553D, 0xAA7A, 0x44D5,
    0x6F45, 0xDE8A, 0xAD35, 0x4A4B, 0x9496, 0x390D, 0x721A,
    0xEB23, 0xC667, 0x9CEF, 0x29FF, 0x53FE, 0xA7FC, 0x5FD9,
    0x47D3, 0x8FA6, 0x0F6D, 0x1EDA, 0x3DB4, 0x7B68, 0xF6D0,
    0xB861, 0x60E3, 0xC1C6, 0x93AD, 0x377B, 0x6EF6, 0xDDEC,
    0x45A0, 0x8B40, 0x06A1, 0x0D42, 0x1A84, 0x3508, 0x6A10,
    0xAA51, 0x4483, 0x8906, 0x022D, 0x045A, 0x08B4, 0x1168,
    0x76B4, 0xED68, 0xCAF1, 0x85C3, 0x1BA7, 0x374E, 0x6E9C,
    0x3730, 0x6E60, 0xDCC0, 0xA9A1, 0x4363, 0x86C6, 0x1DAD,
    0x3331, 0x6662, 0xCCC4, 0x89A9, 0x0373, 0x06E6, 0x0DCC,
    0x1021, 0x2042, 0x4084, 0x8108, 0x1231, 0x2462, 0x48C4
};

/* MS-OFFCRYPTO 2.3.7.2 CreateXorKey_Method1: password is a single-byte
   string of <= 15 characters */
static U16
wvCreateXorKey_Method1 (const U8 * pw, int len)
{
    U16 xorkey;
    int ce, ch, bit;

    if (len < 1 || len > 15)
	return (0);

    xorkey = wv_xor_initial_code[len - 1];
    ce = 0x68;

    for (ch = len - 1; ch >= 0; ch--)
      {
	  U8 c = pw[ch];
	  for (bit = 0; bit < 7; bit++)
	    {
		if (c & 0x40)
		    xorkey ^= wv_xor_matrix[ce];
		c <<= 1;
		ce--;
	    }
      }
    return (xorkey);
}

/* MS-OFFCRYPTO 2.3.7.1 CreatePasswordVerifier_Method1 */
static U16
wvCreatePasswordVerifier_Method1 (const U8 * pw, int len)
{
    U16 verifier = 0;
    int i;

    /* PasswordArray = [length byte] + password, consumed in reverse */
    for (i = len; i >= 0; i--)
      {
	  U8 byte = (i == 0) ? (U8) len : pw[i - 1];
	  U16 inter1 = (verifier & 0x4000) ? 1 : 0;
	  U16 inter2 = (U16) ((verifier * 2) & 0x7FFF);
	  verifier = (U16) ((inter1 | inter2) ^ byte);
      }
    return (verifier ^ 0xCE4B);
}

/* MS-OFFCRYPTO 2.3.7.4/2.3.7.5: build the 16-byte XOR obfuscation
   array; *verifier receives the 32-bit Method-2 password verifier */
static void
wvCreateXorArray_Method2 (const U8 * pw, int len, U8 array[16],
			  U32 * verifier)
{
    U16 keyhigh16 = wvCreateXorKey_Method1 (pw, len);
    U16 keylow16 = wvCreatePasswordVerifier_Method1 (pw, len);
    U8 keyhigh = (U8) (keyhigh16 >> 8);
    U8 keylow = (U8) (keyhigh16 & 0xFF);
    int i;

    *verifier = ((U32) keyhigh16 << 16) | keylow16;

    memset (array, 0, 16);
    for (i = 0; i < 16; i++)
	array[i] = (i < len) ? pw[i] : wv_xor_pad[i - len];

    for (i = 0; i < 16; i += 2)
      {
	  array[i] = WV_ROR8 (array[i] ^ keylow);
	  array[i + 1] = WV_ROR8 (array[i + 1] ^ keyhigh);
      }
}

/* MS-OFFCRYPTO 2.3.7.6 XOR Data Transformation Method 2: transform a
   whole stream into a fresh buffer; the first `skip` bytes keep their
   stored (untransformed) values, the obfuscation index tracks the
   absolute stream offset. */
static U8 *
wvXorTransformStream (wvStream * in, const U8 array[16], U32 skip,
		      size_t * len)
{
    U8 *buf;
    U32 size, pos;

    size = wvStream_size (in);
    wvStream_goto (in, 0);

    buf = (U8 *) malloc (size ? size : 1);
    if (!buf)
	return (NULL);

    for (pos = 0; pos < size; pos++)
      {
	  U8 b = read_8ubit (in);
	  if (pos >= skip)
	    {
		U8 a = array[pos & 0xF];
		if (b != 0 && (b ^ a) != 0)
		    b ^= a;
	    }
	  buf[pos] = b;
      }

    *len = size;
    return (buf);
}

/*
 * MS-DOC 2.2.6.1 XOR obfuscation (Word 97+): FibBase.fEncrypted and
 * fObfuscated are both 1 and FibBase.lKey is the password verifier.
 * The WordDocument stream is obfuscated from offset 68 on, the Table
 * and Data streams in full.
 */
int
wvDecryptObfuscated (wvParseStruct * ps)
{
    U8 pw[15];
    U8 array[16];
    U32 verifier;
    int i, len;
    U8 *mainbuf, *tablebuf, *databuf;
    size_t mainlen, tablelen, datalen;

    if (!ps->tablefd)
	return (1);

    /* Unicode -> single-byte password (MS-OFFCRYPTO 2.3.7.4): take the
       low byte of each character unless it is 0x00, in which case take
       the high byte; truncate to 15 characters */
    len = 0;
    for (i = 0; i < 16 && ps->password[i]; i++)
      {
	  U16 c = ps->password[i];
	  pw[len++] = (c & 0xFF) ? (U8) c : (U8) (c >> 8);
	  if (len == 15)
	      break;
      }

    if (len < 1)
	return (1);

    wvCreateXorArray_Method2 (pw, len, array, &verifier);
    if (verifier != ps->fib.lKey)
      {
	  wvTrace (("obfuscated .doc: password verifier mismatch "
		    "(%08x vs %08x)\n", verifier, ps->fib.lKey));
	  return (1);
      }

    tablebuf = wvXorTransformStream (ps->tablefd, array, 0, &tablelen);
    mainbuf = wvXorTransformStream (ps->mainfd, array, 68, &mainlen);

    databuf = NULL;
    datalen = 0;
    if (ps->data && ps->data != ps->mainfd)
	databuf = wvXorTransformStream (ps->data, array, 0, &datalen);

    if (!tablebuf || !mainbuf)
      {
	  free (tablebuf);
	  free (mainbuf);
	  free (databuf);
	  return (-1);
      }

    /* clear fEncrypted/fObfuscated in the cleartext copy's FibBase so
       the FIB re-read below parses the whole stream */
    if (mainlen > 0x0B)
	mainbuf[0x0B] &= 0x7E;

    if (ps->tablefd0)
	wvStream_close (ps->tablefd0);
    if (ps->tablefd1)
	wvStream_close (ps->tablefd1);
    if (ps->summary)
	wvStream_close (ps->summary);
    if (ps->data && ps->data != ps->mainfd)
	wvStream_close (ps->data);

    wvStream_close (ps->mainfd);

    wvStream_memory_create (&ps->tablefd0, (char *) tablebuf, tablelen);
    wvStream_memory_create (&ps->mainfd, (char *) mainbuf, mainlen);
    if (databuf)
	wvStream_memory_create (&ps->data, (char *) databuf, datalen);

    ps->tablefd = ps->tablefd0;
    ps->tablefd1 = ps->tablefd0;

    wvStream_rewind (ps->tablefd0);
    wvStream_rewind (ps->mainfd);
    if (wvGetFIB (&ps->fib, ps->mainfd))
	return (-1);
    wvClampFIBFcLcb (&ps->fib, wvStream_size (ps->tablefd0));
    ps->fib.fEncrypted = 0;
    return (0);
}

int
wvDecrypt95 (wvParseStruct * ps)
{
    GsfOutput *mainfd;
    unsigned char pw[16], z, g;
    unsigned char key[16];
    U8 pwkey[2];
    int i, c, len, ret = 1;
    U32 j = 0;
    unsigned long end;
    unsigned char test[0x10];
    U16 hash, h;

    if (ps->password[0] == 0)
	return (ret);

    hash = (U16) ps->fib.lKey & 0xffff;

    pwkey[0] = (U8) ((ps->fib.lKey >> 16) & 0xFF);
    pwkey[1] = (U8) ((ps->fib.lKey >> 24) & 0xFF);

    /* 
       currently I do not know what the story is with the actual
       input of non ascii password for word 95
     */
    for (i = 0; i < 16; i++)
	pw[i] = (U8) ps->password[i];

    len = strlen ((char *) pw);
    i = len;
    z = 0xbb;
    for (; i < 16; i++)
      {
	  switch (j)
	    {
	    case 0:
		pw[i] = 0xbb;
		break;
	    case 1:
		pw[i] = 0xff;
		break;
	    case 2:
		pw[i] = 0xff;
		break;
	    case 3:
		pw[i] = 0xba;
		break;
	    case 4:
		pw[i] = 0xff;
		break;
	    case 5:
		pw[i] = 0xff;
		break;
	    case 6:
		pw[i] = 0xb9;
		break;
	    case 7:
		pw[i] = 0x80;
		break;
	    case 8:
		pw[i] = 0x0;
		break;
	    case 9:
		pw[i] = 0xbe;
		break;
	    case 10:
		pw[i] = 0xf;
		break;
	    case 11:
		pw[i] = 0x0;
		break;
	    case 12:
		pw[i] = 0xbf;
		break;
	    case 13:
		pw[i] = 0xf;
		break;
	    case 14:
		pw[i] = 0;
		break;
		j++;
	    }
	  j++;
      }

    h = 0xce4b;
    wvTrace (("hash is now %x\n", hash));
    for (i = 0; i < 16; i++)
      {
	  g = (pw[i] ^ pwkey[i & 1]);
	  g = ROTATE_LEFT (g, 7, 8);
	  h ^= (ROTATE_LEFT (pw[i], i + 1, 15) ^ (i + 1) ^ i);
	  wvTrace (("h is now %x\n", h));
	  if (i == len - 1)
	      if (h == hash)
		  ret = 0;
	  key[i] = g;
      }

    if (ret)
	return (ret);

    wvStream_offset_from_end (ps->mainfd, 0);
    end = wvStream_tell (ps->mainfd);

    j = 0;
    wvStream_goto (ps->mainfd, j);

    mainfd = gsf_output_memory_new ();

    while (j < 0x30)
      {
	  c = read_8ubit (ps->mainfd);
	  gsf_output_write (mainfd, 1, (guint8 *)&c);
	  j++;
      }

    while (j < end)
      {
	  for (i = 0; i < 16; i++)
	      test[i] = read_8ubit (ps->mainfd);
	  for (i = 0; i < 16; i++)
	    {
		if (test[i] != 0)
		    c = key[i] ^ test[i];
		else
		    c = 0;
		gsf_output_write (mainfd, 1, (guint8 *)&c);
	    }
	  j += 16;
      }


    if (ps->tablefd0)
	wvStream_close (ps->tablefd0);
    if (ps->tablefd1)
	wvStream_close (ps->tablefd1);
    wvStream_close (ps->mainfd);

    gsf_output_close (mainfd);

    wvStream_memory_create(&ps->mainfd, 
			   g_memdup2 (gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mainfd)), gsf_output_size (mainfd)),
			   gsf_output_size (mainfd));

    g_object_unref (G_OBJECT (mainfd));

    ps->tablefd = ps->mainfd;
    ps->tablefd0 = ps->mainfd;
    ps->tablefd1 = ps->mainfd;

    wvStream_rewind (ps->mainfd);
    ps->fib.fEncrypted = 0;
    if (wvGetFIB (&ps->fib, ps->mainfd))
	return (-1);
    wvClampFIBFcLcb (&ps->fib, wvStream_size (ps->mainfd));
    ps->fib.fEncrypted = 0;
    return (ret);
}

#if 0
int
wvCrack95 (wvParseStruct * ps)
{
    /* 
       is is quite possible to crack password 95 files fairly easily,
       there is standard frequency analysis on the cipher text whiich
       is simply stamped with a 16byte xor key from 0x28 onwards,
       but there are quite a few known bytes in the fib header with
       is also encrypted, for instance unencrypted is the beginning
       and end of the text stream, encrypted is the len of the
       text stream, so already you have a few bytes of the xor key,
       and so on, the fcStshOrig is nearly always the same as the fcStsh
       so theres another byte or two, and so forth. If a struct has
       an lcb of 0, it remains zero despite what should happen in a
       xor and thus you know that the fc before and after the 0 lcb
       are the same, and so forth, so you could derive a lot of 
       information about the xor key from the header, I might
       complete this some day, there have been a load of other programs
       that do similiar things, so i don't know that my adding to that
       pile will help
     */


    int i;
    U8 block1[4];
    U8 block2[4];
    U32 reallen;
    wvError (("begin is %x\n", ps->fib.fcMin));
    wvError (("end is %x\n", ps->fib.fcMac));
    wvError (("len is %x\n", ps->fib.fcMac - ps->fib.fcMin));

    reallen = ps->fib.fcMac - ps->fib.fcMin;

    wvError (("encrypted len is %x\n", ps->fib.ccpText));
    block1[0] = (U8) (ps->fib.ccpText & 0xFF);
    block1[1] = (U8) ((ps->fib.ccpText >> 8) & 0xFF);
    block1[2] = (U8) ((ps->fib.ccpText >> 16) & 0xFF);
    block1[3] = (U8) ((ps->fib.ccpText >> 24) & 0xFF);

    block2[0] = (U8) (reallen & 0xFF);
    block2[1] = (U8) ((reallen >> 8) & 0xFF);
    block2[2] = (U8) ((reallen >> 16) & 0xFF);
    block2[3] = (U8) ((reallen >> 24) & 0xFF);

    key[4] = block1[0] ^ block2[0];
    key[5] = block1[1] ^ block2[1];
    key[6] = block1[2] ^ block2[2];
    key[7] = block1[3] ^ block2[3];


    block1[0] = (U8) (ps->fib.lcbStshf & 0xFF);
    block1[1] = (U8) ((ps->fib.lcbStshf >> 8) & 0xFF);
    block1[2] = (U8) ((ps->fib.lcbStshf >> 16) & 0xFF);
    block1[3] = (U8) ((ps->fib.lcbStshf >> 24) & 0xFF);

    block2[0] = block1[0] ^ key[4];
    block2[1] = block1[1] ^ key[5];
    block2[2] = block1[2] ^ key[6];
    block2[3] = block1[3] ^ key[7];

    reallen = sread_32ubit (block2);
    fprintf (stderr, "reallen is %x\n", reallen);

    block2[0] = (U8) (reallen & 0xFF);
    block2[1] = (U8) ((reallen >> 8) & 0xFF);
    block2[2] = (U8) ((reallen >> 16) & 0xFF);
    block2[3] = (U8) ((reallen >> 24) & 0xFF);

    block1[0] = (U8) (ps->fib.lcbStshfOrig & 0xFF);
    block1[1] = (U8) ((ps->fib.lcbStshfOrig >> 8) & 0xFF);
    block1[2] = (U8) ((ps->fib.lcbStshfOrig >> 16) & 0xFF);
    block1[3] = (U8) ((ps->fib.lcbStshfOrig >> 24) & 0xFF);

    key[12] = block1[0] ^ block2[0];
    key[13] = block1[1] ^ block2[1];
    key[14] = block1[2] ^ block2[2];
    key[15] = block1[3] ^ block2[3];


    block1[0] = (U8) (ps->fib.fcStshf & 0xFF);
    block1[1] = (U8) ((ps->fib.fcStshf >> 8) & 0xFF);
    block1[2] = (U8) ((ps->fib.fcStshf >> 16) & 0xFF);
    block1[3] = (U8) ((ps->fib.fcStshf >> 24) & 0xFF);

    block2[0] = (U8) (ps->fib.fcPlcffndRef & 0xFF);
    block2[1] = (U8) ((ps->fib.fcPlcffndRef >> 8) & 0xFF);
    block2[2] = (U8) ((ps->fib.fcPlcffndRef >> 16) & 0xFF);
    block2[3] = (U8) ((ps->fib.fcPlcffndRef >> 24) & 0xFF);

    if ((block1[0] == 0) && (block2[0]) && (reallen < 0xff))
      {
	  fprintf (stderr, "good\n");
	  key[8] = reallen ^ block2[0];
      }
    else
	fprintf (stderr, "crap\n");

    block2[0] = (U8) (ps->fib.lcbPlcffndRef & 0xFF);
    block2[1] = (U8) ((ps->fib.lcbPlcffndRef >> 8) & 0xFF);
    block2[2] = (U8) ((ps->fib.lcbPlcffndRef >> 16) & 0xFF);
    block2[3] = (U8) ((ps->fib.lcbPlcffndRef >> 24) & 0xFF);

    fprintf (stderr, "%x %x %x %x\n", block2[0], block2[1], block2[1],
	     block2[0]);

    if ((block2[0] == 0) && (block2[1] == 0) && (block2[2] == 0)
	&& (block2[3] == 0))
      {
	  fprintf (stderr, "good\n");
	  block1[0] = (U8) (ps->fib.fcPlcffndRef & 0xFF);
	  block2[0] = (U8) (ps->fib.fcPlcffndTxt & 0xFF);
	  reallen = key[8] ^ block2[0];
	  key[0] = reallen ^ block1[0];
      }

    for (i = 0; i < 16; i++)
	fprintf (stderr, "key %d is %x\n", i, key[i]);
    return (0);
}
#endif
