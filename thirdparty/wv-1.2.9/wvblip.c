/* wvblip -- list (and optionally dump) the images stored in a Word
   .doc file.  Debug/test hook for the AbiWord blip extraction API
   added to this wv: wvGetDocEscher / wvGetStoreBlipCount /
   wvGetStoreBlip / wvExtractBlipData, and the inline-picture path
   wvGetPICF + wv0x01.

   usage:
     wvblip FILE.doc [--dump DIR]    enumerate the OfficeArt blip
                                     store (FBSE + bare blip slots)
     wvblip FILE.doc --picf OFFSET   read the PICF at Data-stream
                                     byte OFFSET and extract the
                                     inline picture inside it

   Exit status: 0 when at least one image payload was extracted,
   1 when none was found, 2 on argument/parse failure.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "wv.h"

static const char *
blip_type_name (U16 type)
{
    switch (type)
      {
      case msoblipEMF:
	  return "EMF";
      case msoblipWMF:
	  return "WMF";
      case msoblipPICT:
	  return "PICT";
      case msoblipJPEG:
	  return "JPEG";
      case msoblipPNG:
	  return "PNG";
      case msoblipDIB:
	  return "DIB";
      case msoblipERROR:
	  return "error";
      case msoblipUNKNOWN:
	  return "unknown";
      default:
	  return "other";
      }
}

static const char *
blip_type_ext (U16 type)
{
    switch (type)
      {
      case msoblipEMF:
	  return "emf";
      case msoblipWMF:
	  return "wmf";
      case msoblipPICT:
	  return "pict";
      case msoblipJPEG:
	  return "jpg";
      case msoblipPNG:
	  return "png";
      case msoblipDIB:
	  return "dib";
      default:
	  return "bin";
      }
}

static int
dump_blip (Blip * blip, const char *label, const char *dumpdir)
{
    U8 *data;
    U32 len;
    U16 type;
    static int dumpidx = 0;

    if (!wvExtractBlipData (blip, &data, &len, &type))
      {
	  printf ("%s: type=%s -- no payload\n", label,
		  blip_type_name (blip->type));
	  return 0;
      }

    printf ("%s: type=%s len=%lu magic=%02x %02x %02x %02x\n", label,
	    blip_type_name (type), (unsigned long) len,
	    len > 0 ? data[0] : 0, len > 1 ? data[1] : 0,
	    len > 2 ? data[2] : 0, len > 3 ? data[3] : 0);

    if (dumpdir)
      {
	  char *fname = g_strdup_printf ("%s/blip-%02d.%s", dumpdir,
					 dumpidx, blip_type_ext (type));
	  FILE *out = fopen (fname, "wb");
	  if (!out)
	    {
		fprintf (stderr, "wvblip: cannot write %s\n", fname);
		g_free (fname);
		wvFree (data);
		return 0;
	    }
	  fwrite (data, 1, len, out);
	  fclose (out);
	  printf ("  wrote %s\n", fname);
	  g_free (fname);
      }
    dumpidx++;
    wvFree (data);
    return 1;
}

int
main (int argc, char **argv)
{
    const char *path = NULL;
    const char *dumpdir = NULL;
    long picf_offset = -1;
    int i, found = 0;
    wvParseStruct ps;

    for (i = 1; i < argc; i++)
      {
	  if (!strcmp (argv[i], "--dump") && i + 1 < argc)
	      dumpdir = argv[++i];
	  else if (!strcmp (argv[i], "--picf") && i + 1 < argc)
	      picf_offset = strtol (argv[++i], NULL, 0);
	  else if (argv[i][0] != '-')
	      path = argv[i];
	  else
	    {
		fprintf (stderr, "wvblip: unknown option %s\n", argv[i]);
		return 2;
	    }
      }
    if (!path)
      {
	  fprintf (stderr,
		   "usage: wvblip FILE.doc [--dump DIR] | --picf OFFSET\n");
	  return 2;
      }
    if (dumpdir)
	g_mkdir_with_parents (dumpdir, 0755);

    wvInit ();
    if (wvInitParser (&ps, (char *) path))
      {
	  fprintf (stderr, "wvblip: cannot parse %s\n", path);
	  wvShutdown ();
	  return 2;
      }

    if (picf_offset >= 0)
      {
	  /* inline picture: the sprmCPicLocation operand names a
	     PICF record in the Data stream whose payload region
	     holds an OfficeArtInlineSpContainer / blip */
	  Blip blip;
	  PICF picf;

	  memset (&blip, 0, sizeof (blip));
	  if (ps.data == NULL ||
	      picf_offset < 0 ||
	      picf_offset >= (long) wvStream_size (ps.data))
	    {
		fprintf (stderr, "wvblip: bad PICF offset %ld\n",
			 picf_offset);
		wvOLEFree (&ps);
		wvShutdown ();
		return 2;
	    }
	  wvStream_goto (ps.data, picf_offset);
	  if (wvGetPICF (wvQuerySupported (&ps.fib, NULL), &picf,
			 ps.data) == 1 && picf.rgb != NULL)
	    {
		if (wv0x01 (&blip, picf.rgb, wvStream_size (picf.rgb),
			    ps.data, NULL))
		    found += dump_blip (&blip, "inline", dumpdir);
		else
		    printf ("inline: no blip payload\n");
		wvStream_close (picf.rgb);
		wvReleaseBlip (&blip);
	    }
	  else
	      printf ("inline: no PICF at offset %ld\n", picf_offset);
      }
    else
      {
	  /* floating/store pictures: the OfficeArtDggInfo blip
	     store indexed by shape pib properties */
	  escherstruct item;
	  U32 n;

	  wvGetDocEscher (&ps, &item);
	  n = wvGetStoreBlipCount (&item);
	  printf ("blip store: %lu entr%s\n", (unsigned long) n,
		  n == 1 ? "y" : "ies");
	  for (i = 0; i < (int) n; i++)
	    {
		Blip blip;
		char label[64];

		memset (&blip, 0, sizeof (blip));
		if (!wvGetStoreBlip (&item, (U32) (i + 1), &blip))
		  {
		      printf ("blip %d: unreadable slot\n", i + 1);
		      continue;
		  }
		snprintf (label, sizeof (label), "blip %d", i + 1);
		found += dump_blip (&blip, label, dumpdir);
		wvReleaseBlip (&blip);
	    }
	  wvReleaseEscher (&item);
      }

    wvOLEFree (&ps);
    wvShutdown ();
    return (found > 0) ? 0 : 1;
}
