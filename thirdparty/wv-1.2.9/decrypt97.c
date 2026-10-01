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

/*
 *  Password verification in Microsoft Word 8.0
 *  see D_CREDITS & D_README
 *
 *  Encryption semantics per MS-DOC 2.2.6 + MS-OFFCRYPTO 2.3.6:
 *  the EncryptionHeader sits unencrypted in the first FibBase.lKey
 *  bytes of the Table stream; its EncryptionVersionInfo.vMajor MUST
 *  be 0x0001 for the plain RC4 scheme implemented here (vMajor
 *  0x0002..0x0004 denote RC4 CryptoAPI encryption, which uses a
 *  different key derivation and is rejected).  The rest of the Table
 *  stream, the WordDocument stream beyond its initial 68 bytes and the
 *  whole Data stream are RC4-encrypted in 512-byte blocks (the block
 *  number restarts at 0 for each stream and a fresh RC4 key is derived
 *  per block).  The cipher logically runs over the plaintext prefix
 *  bytes too -- they are simply stored untransformed -- so on decrypt
 *  the keystream must advance over them while their stored bytes are
 *  left alone.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#ifndef WIN32
#include <sys/types.h>
#endif
#include <string.h>
#include "wv.h"
#undef S32
#include "rc4.h"
#include "md5.h"

void wvMD5StoreDigest (wvMD5_CTX * mdContext);

static void
makekey (U32 block, rc4_key * key, wvMD5_CTX * valContext)
{
    wvMD5_CTX mdContext;
    U8 pwarray[64];

    memset (pwarray, 0, 64);

    /* 40 bit of hashed password, set by verifypwd() */
    memcpy (pwarray, valContext->digest, 5);

    /* put block number in byte 6...9 */
    pwarray[5] = (U8) (block & 0xFF);
    pwarray[6] = (U8) ((block >> 8) & 0xFF);
    pwarray[7] = (U8) ((block >> 16) & 0xFF);
    pwarray[8] = (U8) ((block >> 24) & 0xFF);

    pwarray[9] = 0x80;
    pwarray[56] = 0x48;

    wvMD5Init (&mdContext);
    wvMD5Update (&mdContext, pwarray, 64);
    wvMD5StoreDigest (&mdContext);
    prepare_key (mdContext.digest, 16, key);
}

static int
verifypwd (U8 pwarray[64], U8 docid[16], U8 salt[64], U8 hashedsalt[16],
	   wvMD5_CTX * valContext)
{
    wvMD5_CTX mdContext1, mdContext2;
    rc4_key key;
    int offset, keyoffset;
    unsigned int tocopy;

    wvMD5Init (&mdContext1);
    wvMD5Update (&mdContext1, pwarray, 64);
    wvMD5StoreDigest (&mdContext1);

    offset = 0;
    keyoffset = 0;
    tocopy = 5;

    wvMD5Init (valContext);

    while (offset != 16)
      {
	  if ((64 - offset) < 5)
	      tocopy = 64 - offset;

	  memcpy (pwarray + offset, mdContext1.digest + keyoffset, tocopy);
	  offset += tocopy;

	  if (offset == 64)
	    {
		wvMD5Update (valContext, pwarray, 64);
		keyoffset = tocopy;
		tocopy = 5 - tocopy;
		offset = 0;
		continue;
	    }

	  keyoffset = 0;
	  tocopy = 5;
	  memcpy (pwarray + offset, docid, 16);
	  offset += 16;
      }

    /* Fix (zero) all but first 16 bytes */

    pwarray[16] = 0x80;
    memset (pwarray + 17, 0, 47);
    pwarray[56] = 0x80;
    pwarray[57] = 0x0A;

    wvMD5Update (valContext, pwarray, 64);
    wvMD5StoreDigest (valContext);

    /* Generate 40-bit RC4 key from 128-bit hashed password */

    makekey (0, &key, valContext);

    rc4 (salt, 16, &key);
    rc4 (hashedsalt, 16, &key);

    salt[16] = 0x80;
    memset (salt + 17, 0, 47);
    salt[56] = 0x80;

    wvMD5Init (&mdContext2);
    wvMD5Update (&mdContext2, salt, 64);
    wvMD5StoreDigest (&mdContext2);

    return (memcmp (mdContext2.digest, hashedsalt, 16));
}

static void
expandpw (U16 password[16], U8 pwarray[64])
{
    /* expandpw expects null terminated 16bit unicode input */
    int i;

    for (i = 0; i < 64; i++)
	pwarray[i] = 0;

    i = 0;
    /* AbiWord: bound the scan to the 16-entry password array -- a
       non-terminated input must not read/write past pwarray[64] */
    while (i < 16 && password[i])
      {
	  pwarray[2 * i] = (password[i] & 0xff);
	  pwarray[(2 * i) + 1] = ((password[i] >> 8) & 0xff);
	  i++;
      }

    pwarray[2 * i] = 0x80;
    pwarray[56] = (i << 4);
}

/*
 * RC4-decrypt a whole stream into a freshly allocated buffer.
 * "prefix" bytes at the start of the stream are stored untransformed
 * (MS-DOC 2.2.6); the cipher still runs over them, so their decrypted
 * output is discarded and the stored bytes kept.  Returns the buffer
 * or NULL; *len receives the stream size.
 */
static U8 *
decrypt_rc4_stream (wvStream * enc, wvMD5_CTX * valContext, U32 prefix,
		    size_t * len)
{
    U8 *buf;
    U8 test[0x10];
    U32 size, pos;
    unsigned int blk, i, n;
    rc4_key key;

    size = wvStream_size (enc);
    wvStream_goto (enc, 0);

    buf = (U8 *) malloc (size ? size : 1);
    if (!buf)
	return (NULL);

    blk = 0;
    makekey (blk, &key, valContext);

    for (pos = 0; pos < size;)
      {
	  n = (size - pos < 0x10) ? (unsigned int) (size - pos) : 0x10;
	  for (i = 0; i < n; i++)
	      {
		  test[i] = read_8ubit (enc);
		  buf[pos + i] = test[i];	/* keep the stored bytes */
	      }

	  /* the cipher also ran over the untransformed prefix, so the
	     keystream must advance over it even though its output is
	     discarded there */
	  rc4 (test, n, &key);

	  for (i = 0; i < n; i++)
	      if (pos + i >= prefix)
		  buf[pos + i] = test[i];

	  pos += n;
	  if ((pos % 0x200) == 0)
	    {
		/*
		   at this stage we need to rekey the rc4 algorithm
		   Dieter Spaar <spaar@mirider.augusta.de> figured out
		   this rekeying, big kudos to him
		 */
		blk++;
		makekey (blk, &key, valContext);
	    }
      }

    *len = size;
    return (buf);
}

int
wvDecrypt97 (wvParseStruct * ps)
{
    U8 pwarray[64];
    U8 docid[16], salt[64], hashedsalt[16];
    U16 vMajor, vMinor;
    int i;
    U8 *mainbuf, *tablebuf, *databuf;
    size_t mainlen, tablelen, datalen;
    U32 tableprefix;
    wvMD5_CTX valContext;

    if (!ps->tablefd)
	return (1);

    /* EncryptionVersionInfo: vMajor 0x0001 = RC4, 0x0002..0x0004 =
       RC4 CryptoAPI (MS-OFFCRYPTO 2.3.5), which we cannot decrypt */
    wvStream_rewind (ps->tablefd);
    vMajor = read_16ubit (ps->tablefd);
    vMinor = read_16ubit (ps->tablefd);
    if (vMajor != 0x0001 || vMinor != 0x0001)
      {
	  wvError (("Encrypted .doc: unsupported EncryptionVersionInfo "
		    "%u.%u (only RC4 1.1 is supported)\n",
		    vMajor, vMinor));
	  return (-2);
      }

    for (i = 0; i < 16; i++)
	docid[i] = read_8ubit (ps->tablefd);

    for (i = 0; i < 16; i++)
	salt[i] = read_8ubit (ps->tablefd);

    for (i = 0; i < 16; i++)
	hashedsalt[i] = read_8ubit (ps->tablefd);

    expandpw (ps->password, pwarray);

    if (verifypwd (pwarray, docid, salt, hashedsalt, &valContext))
	return (1);

    /* the first lKey bytes of the Table stream hold the plaintext
       EncryptionHeader; clamp defensively against a corrupt FIB */
    tableprefix = ps->fib.lKey;
    if (tableprefix > wvStream_size (ps->tablefd))
	tableprefix = wvStream_size (ps->tablefd);

    tablebuf = decrypt_rc4_stream (ps->tablefd, &valContext, tableprefix,
				   &tablelen);
    mainbuf = decrypt_rc4_stream (ps->mainfd, &valContext, 68, &mainlen);

    /* the Data stream, when present, is wholly encrypted with its own
       block numbering starting at 0 */
    databuf = NULL;
    datalen = 0;
    if (ps->data && ps->data != ps->mainfd)
	databuf = decrypt_rc4_stream (ps->data, &valContext, 0, &datalen);

    if (!tablebuf || !mainbuf)
      {
	  free (tablebuf);
	  free (mainbuf);
	  free (databuf);
	  return (-1);
      }

    /* the stored FibBase flags keep advertising fEncrypted/fObfuscated;
       clear them in the cleartext copy so the FIB re-read below parses
       the whole stream */
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
    ps->fib.fEncrypted = 0;
    if (wvGetFIB (&ps->fib, ps->mainfd))
	return (-1);
    wvClampFIBFcLcb (&ps->fib, wvStream_size (ps->tablefd0));
    ps->fib.fEncrypted = 0;
    return (0);
}


/*
this is just cut out of wvMD5Final to get the byte order correct
under MSB systems, the previous code was woefully tied to intel
x86

C.
*/
void
wvMD5StoreDigest (wvMD5_CTX * mdContext)
{
    unsigned int i, ii;
    /* store buffer in digest */
    for (i = 0, ii = 0; i < 4; i++, ii += 4)
      {
	  mdContext->digest[ii] = (unsigned char) (mdContext->buf[i] & 0xFF);
	  mdContext->digest[ii + 1] =
	      (unsigned char) ((mdContext->buf[i] >> 8) & 0xFF);
	  mdContext->digest[ii + 2] =
	      (unsigned char) ((mdContext->buf[i] >> 16) & 0xFF);
	  mdContext->digest[ii + 3] =
	      (unsigned char) ((mdContext->buf[i] >> 24) & 0xFF);
      }
}
