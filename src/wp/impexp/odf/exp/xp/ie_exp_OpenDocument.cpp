/* AbiSource
 * 
 * Copyright (C) 2002 Dom Lachowicz <cinamod@hotmail.com>
 * Copyright (C) 2004 Robert Staudinger <robsta@stereolyzer.net>
 * Copyright (C) 2005 Daniel d'Andrada T. de Carvalho
 * Copyright (C) 2025-2026 Abinova contributors
 * <daniel.carvalho@indt.org.br>
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


// Class definition include
#include "ie_exp_OpenDocument.h"

// Internal includes
#include "ODe_AbiDocListener.h"
#include "ODe_AuxiliaryData.h"
#include "ODe_Common.h"
#include "ODe_DocumentData.h"
#include "ODe_HeadingSearcher_Listener.h"
#include "ODe_TOC_Listener.h"
#include "ODe_ManifestWriter.h"
#include "ODe_RDFWriter.h"
#include "ODe_Main_Listener.h"
#include "ODe_MetaDataWriter.h"
#include "ODe_ThumbnailsWriter.h"
#include "ODe_PicturesWriter.h"
#include "ODe_SettingsWriter.h"
#include "../../common/xp/ODc_Crypto.h"

// Abiword includes
#include "ut_assert.h"
#include "ut_locale.h"
#include "ut_xml.h"
#include "pd_Document.h"
#include "pd_DocumentRDF.h"
#include "ie_exp_DocRangeListener.h"
#include "pl_ListenerCoupleCloser.h"

#ifdef _WIN32
#include <io.h>
#endif
#include <map>
#include <set>
#include <vector>
#include <string>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * Constructor
 */
IE_Exp_OpenDocument::IE_Exp_OpenDocument (PD_Document * pDoc)
  : IE_Exp (pDoc), m_odt(nullptr)
{
}


/**
 * Destructor
 */
IE_Exp_OpenDocument::~IE_Exp_OpenDocument()
{
}

GsfOutput* IE_Exp_OpenDocument::_openFile(const char *szFilename)
{
  GsfOutput *output = nullptr;

  const std::string & prop = getProperty ("uncompressed");

  if (!prop.empty() && UT_parseBool (prop.c_str (), false))
    {
      char *filename = UT_go_filename_from_uri (szFilename);
      if (filename) 
	{
	  output = (GsfOutput*)gsf_outfile_stdio_new (filename, nullptr);
	  g_free (filename);
	}
    }
  else
    {
      output = IE_Exp::_openFile (szFilename);
    }

  return output;
}

void IE_Exp_OpenDocument::setGSFOutput(GsfOutput * pBuf)
{
    m_odt = gsf_output_container(pBuf);
}

/*!
 * This method copies the selection defined by pDocRange to ODT format
 * placed in the ByteBuf bufODT
 */
UT_Error IE_Exp_OpenDocument::copyToBuffer(PD_DocumentRange * pDocRange, const UT_ByteBufPtr & bufODT)
{
    //
    // First export selected range to a tempory document
    //
    PD_Document * outDoc = new PD_Document();
    outDoc->createRawDocument();
    IE_Exp_DocRangeListener * pRangeListener = new IE_Exp_DocRangeListener(pDocRange,outDoc);
    UT_DEBUGMSG(("DocumentRange low %d High %d \n",pDocRange->m_pos1,pDocRange->m_pos2));
    PL_ListenerCoupleCloser* pCloser = new PL_ListenerCoupleCloser();
    pDocRange->m_pDoc->tellListenerSubset(pRangeListener,pDocRange,pCloser);
    if( pCloser)
        delete pCloser;
    
    //
    // Grab the RDF triples while we are copying...
    //
    if( PD_DocumentRDFHandle outrdf = outDoc->getDocumentRDF() )
    {

        std::set< std::string > xmlids;
        PD_DocumentRDFHandle inrdf = pDocRange->m_pDoc->getDocumentRDF();
        inrdf->addRelevantIDsForRange( xmlids, pDocRange );

        if( !xmlids.empty() )
        {
            UT_DEBUGMSG(("MIQ: ODF export creating restricted RDF model xmlids.sz:%ld \n",(long)xmlids.size()));
            PD_RDFModelHandle subm = inrdf->createRestrictedModelForXMLIDs( xmlids );
            PD_DocumentRDFMutationHandle m = outrdf->createMutation();
            m->add( subm );
            m->commit();
            subm->dumpModel("copied rdf triples subm");
            outrdf->dumpModel("copied rdf triples result");
        }
        
        // PD_DocumentRDFMutationHandle m = outrdf->createMutation();
        // m->add( PD_URI("http://www.example.com/foo"),
        //         PD_URI("http://www.example.com/bar"),
        //         PD_Literal("copyToBuffer path") );
        // m->commit();
    }
    outDoc->finishRawCreation();
    //
    // OK now we have a complete and valid document containing our selected 
    // content. We export this to an in memory GSF buffer
    //
    IE_Exp * pNewExp = nullptr; 
    char *szTempFileName = nullptr;
    GError *err = nullptr;
    g_file_open_tmp ("XXXXXX", &szTempFileName, &err);
    GsfOutput * outBuf =  gsf_output_stdio_new (szTempFileName,&err);
    IEFileType ftODT = IE_Exp::fileTypeForMimetype("application/vnd.oasis.opendocument.text");
    UT_Error aerr = IE_Exp::constructExporter(outDoc,outBuf,
					     ftODT,&pNewExp);
    if(pNewExp == nullptr)
    {
         return aerr;
    }
    aerr = pNewExp->writeFile(szTempFileName);
    if(aerr != UT_OK)
    {
	delete pNewExp;
	delete pRangeListener;
	UNREFP( outDoc);
	g_remove(szTempFileName);
	g_free (szTempFileName);
	return aerr;
    }
    //
    // File is closed at the end of the export. Open it again.
    //

    GsfInput *  fData = gsf_input_stdio_new(szTempFileName,&err);
    UT_DebugOnly<UT_sint32> siz = gsf_input_size(fData);
    const UT_Byte * pData = gsf_input_read(fData,gsf_input_size(fData),nullptr);
    UT_DEBUGMSG(("Writing %d bytes to clipboard \n", (UT_sint32)siz));
    bufODT->append( pData, gsf_input_size(fData));
    
    delete pNewExp;
    delete pRangeListener;
    UNREFP( outDoc);
    g_remove(szTempFileName);
    g_free (szTempFileName);
    return aerr;
}

/**
 * This writes out our Abinova file as an OpenOffice
 * compound document
 */
UT_Error IE_Exp_OpenDocument::_writeDocument(void)
{
	ODe_DocumentData docData(getDoc());
	ODe_AuxiliaryData auxData;
	ODe_AbiDocListener* pAbiDocListener = nullptr;
	ODe_AbiDocListenerImpl* pAbiDocListenerImpl = nullptr;
    
	UT_return_val_if_fail (getFp(), UT_ERROR);

    PD_DocumentRDFHandle rdf = getDoc()->getDocumentRDF();
    auxData.m_additionalRDF = rdf->createScratchModel();
    
	const std::string & prop = getProperty ("uncompressed");
	std::string password = getDoc()->getSavePassword();
	if (password.empty())
	  {
	    // headless (e.g. --to= conversions) cannot show the save-dialog
	    // password field; allow it to be supplied via the environment
	    const char * envpw = getenv ("ABINOVA_PASSWORD");
	    if (envpw)
	      password = envpw;
	  }
	GsfOutput* pPlainPackage = nullptr;

	if (!password.empty())
	  {
	    // Encryption requested: write a complete plaintext package to
	    // memory first, then encrypt each stream into the real output
	    // in a second pass below.
	    pPlainPackage = gsf_output_memory_new();
	    m_odt = GSF_OUTFILE (gsf_outfile_zip_new (pPlainPackage, nullptr));
	  }
	else if (!prop.empty() && UT_parseBool (prop.c_str (), false))
	  {
	    m_odt = GSF_OUTFILE(g_object_ref(G_OBJECT(getFp())));
	  }
	else
	  {
	    GError* error = nullptr;
	    m_odt = GSF_OUTFILE (gsf_outfile_zip_new (getFp(), &error));

	    if (error)
	      {
		UT_DEBUGMSG(("Error writing odt file: %s\n", error->message));
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	      }
	  }

	UT_return_val_if_fail(m_odt, UT_ERROR);

	// Needed to ensure that all *printf writes numbers correctly,
	// like "45.56mm" instead of "45,56mm".
	UT_LocaleTransactor numericLocale (LC_NUMERIC, "C");
	{
		GsfOutput * mimetype = gsf_outfile_new_child_full (m_odt, "mimetype", FALSE, "compression-level", 0, (void*)0);
		if (!mimetype)
		{
			ODe_gsf_output_close(GSF_OUTPUT(m_odt));
			return UT_ERROR;
		}

		ODe_gsf_output_write(mimetype,
				39 /*39 == strlen("application/vnd.oasis.opendocument.text")*/,
				(const guint8 *)"application/vnd.oasis.opendocument.text");

		ODe_gsf_output_close(mimetype);
    }

	if (!ODe_MetaDataWriter::writeMetaData(getDoc(), m_odt))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}

	if (!ODe_ThumbnailsWriter::writeThumbnails(getDoc(), m_odt))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}

    if (!ODe_SettingsWriter::writeSettings(getDoc(), m_odt))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
	return UT_ERROR;
	}

	if (!ODe_PicturesWriter::writePictures(getDoc(), m_odt))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}


    // Gather all paragraph style names used by heading paragraphs
    // (ie. all paragraph styles that are used to build up TOCs).

    pAbiDocListenerImpl = new ODe_HeadingSearcher_Listener(docData.m_styles, auxData);
    pAbiDocListener = new ODe_AbiDocListener(getDoc(),
                                             pAbiDocListenerImpl, false);

	if (!getDoc()->tellListener(static_cast<PL_Listener *>(pAbiDocListener)))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}
    pAbiDocListener->finished();
    
    DELETEP(pAbiDocListener);
    DELETEP(pAbiDocListenerImpl);

    // Now that we have all paragraph styles that build up the TOCs in the 
    // document (if any), we can build up the TOC bodies. We do this because
    // OpenOffice.org requires the TOC bodies to be present and filled
    // when initially opening the document. Without it, it will show 
    // an empty TOC until the user regenerates it, which is not that pretty.
    // Annoyingly we have to build up the TOC ourselves during export, as
    // it doesn't exist within Abinova's PieceTable. Until that changes, this
    // is the best we can do.

    if (auxData.m_pTOCContents) {
        pAbiDocListenerImpl = new ODe_TOC_Listener(auxData);
        pAbiDocListener = new ODe_AbiDocListener(getDoc(),
                                                 pAbiDocListenerImpl, false);

	    if (!getDoc()->tellListener(static_cast<PL_Listener *>(pAbiDocListener)))
	    {
		    ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		    return UT_ERROR;
	    }
        pAbiDocListener->finished();
        
        DELETEP(pAbiDocListener);
        DELETEP(pAbiDocListenerImpl);
    }

    
    // Gather document content and styles

    if (!docData.doPreListeningWork()) {
      ODe_gsf_output_close(GSF_OUTPUT(m_odt));
      return UT_ERROR;
    }

    pAbiDocListenerImpl = new ODe_Main_Listener(docData, auxData);
    pAbiDocListener = new ODe_AbiDocListener(getDoc(),
                                             pAbiDocListenerImpl, false);

	if (!getDoc()->tellListener(static_cast<PL_Listener *>(pAbiDocListener)))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}
	pAbiDocListener->finished();
    
	DELETEP(pAbiDocListener);
	DELETEP(pAbiDocListenerImpl);
    
	if (!docData.doPostListeningWork())
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}

    // Write RDF.
    if (!ODe_RDFWriter::writeRDF(getDoc(), m_odt, auxData.m_additionalRDF ))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}

	// Write the manifest last (apart from content/styles, which are
	// enumerated statically) so that data items registered during
	// export — e.g. manifest.rdf — get a manifest:file-entry.
	if (!ODe_ManifestWriter::writeManifest(getDoc(), m_odt))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}

    // Write content and styles
        
	if (!docData.writeStylesXML(m_odt))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}
	if (!docData.writeContentXML(m_odt))
	{
		ODe_gsf_output_close(GSF_OUTPUT(m_odt));
		return UT_ERROR;
	}

	ODe_gsf_output_close(GSF_OUTPUT(m_odt));

	if (!password.empty())
	{
		UT_Error err = _encryptPackage(pPlainPackage, password, getFp());
		g_object_unref(G_OBJECT(pPlainPackage));
		return err;
	}
	return UT_OK;
}


/**
 * Collects (full-path, media-type) pairs from a plaintext
 * META-INF/manifest.xml so the encrypted package can emit the same
 * entries annotated with manifest:encryption-data.
 */
class ODe_ManifestEntryCollector : public UT_XML::Listener
{
public:
    std::vector<std::pair<std::string, std::string>> entries;

    virtual void startElement(const gchar* name, const gchar** atts) override
    {
        if (strcmp(name, "manifest:file-entry") != 0)
            return;
        const gchar* path = nullptr;
        const gchar* mime = nullptr;
        for (const gchar** a = atts; a && a[0]; a += 2)
        {
            if (!strcmp(a[0], "manifest:full-path"))
                path = a[1];
            else if (!strcmp(a[0], "manifest:media-type"))
                mime = a[1];
        }
        if (path)
            entries.emplace_back(path, mime ? mime : "");
    }
    virtual void endElement(const gchar* /*name*/) override {}
    virtual void charData(const gchar* /*buffer*/, int /*length*/) override {}
};


/**
 * Recursively collect the stream names inside a package directory.
 * A child is a directory when it is a GsfInfile that reports a
 * non-negative child count; zip file members report -1.
 */
static void collectPackageEntries(GsfInfile* dir, const std::string& prefix,
                                  std::vector<std::string>& paths)
{
    int count = gsf_infile_num_children(dir);
    for (int i = 0; i < count; i++)
    {
        const char* name = gsf_infile_name_by_index(dir, i);
        if (!name)
            continue;
        GsfInput* child = gsf_infile_child_by_index(dir, i);
        if (!child)
            continue;
        if (GSF_IS_INFILE(child) &&
            gsf_infile_num_children(GSF_INFILE(child)) >= 0)
            collectPackageEntries(GSF_INFILE(child), prefix + name + "/", paths);
        else
            paths.push_back(prefix + name);
        g_object_unref(G_OBJECT(child));
    }
}

/**
 * Open the stream at a '/'-separated package path (gsf_infile
 * children are addressed by single path components).
 */
static GsfInput* openPackageStream(GsfInfile* root, const std::string& path)
{
    GsfInput* cur = GSF_INPUT(root);
    g_object_ref(cur);
    std::string::size_type start = 0;
    while (cur)
    {
        std::string::size_type slash = path.find('/', start);
        std::string comp = path.substr(start, slash == std::string::npos
                                       ? std::string::npos : slash - start);
        GsfInput* next = gsf_infile_child_by_name(GSF_INFILE(cur), comp.c_str());
        g_object_unref(cur);
        if (!next)
            return nullptr;
        if (slash == std::string::npos)
            return next;
        cur = next;
        start = slash + 1;
    }
    return nullptr;
}


/**
 * Write an encrypted copy of the plaintext package in @pPlainPackage
 * to @pDest. Every stream except "mimetype" and
 * "META-INF/manifest.xml" is encrypted with Blowfish CFB per the ODF
 * 1.2 encryption model; the manifest is regenerated with
 * manifest:encryption-data entries.
 */
UT_Error IE_Exp_OpenDocument::_encryptPackage(GsfOutput* pPlainPackage,
                                              const std::string& password,
                                              GsfOutput* pDest)
{
    gsf_off_t sz = gsf_output_size(pPlainPackage);
    const guint8* bytes =
        gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(pPlainPackage));
    UT_return_val_if_fail(bytes && sz > 0, UT_ERROR);

    GsfInput* inMem = gsf_input_memory_new(bytes, sz, FALSE);
    GsfInfile* inZip = gsf_infile_zip_new(inMem, nullptr);
    if (!inZip)
    {
        g_object_unref(G_OBJECT(inMem));
        return UT_ERROR;
    }

    // pull the plaintext manifest's (path, media-type) entries before
    // anything else, they are needed when the manifest is regenerated
    std::vector<std::pair<std::string, std::string>> manifestEntries;
    {
        GsfInput* pManifest = openPackageStream(inZip, "META-INF/manifest.xml");
        if (pManifest)
        {
            ODe_ManifestEntryCollector coll;
            UT_XML reader;
            reader.setListener(&coll);
            gsf_off_t msz = gsf_input_size(pManifest);
            const guint8* mdata = gsf_input_read(pManifest, msz, nullptr);
            if (mdata)
                reader.parse(reinterpret_cast<const char*>(mdata), msz);
            manifestEntries = coll.entries;
            g_object_unref(G_OBJECT(pManifest));
        }
    }

    // walk the plaintext package
    std::vector<std::string> paths;
    collectPackageEntries(inZip, "", paths);

    GError* zerr = nullptr;
    GsfOutfile* outZip = GSF_OUTFILE(gsf_outfile_zip_new(pDest, &zerr));
    if (!outZip || zerr)
    {
        if (zerr)
            g_error_free(zerr);
        g_object_unref(G_OBJECT(inZip));
        g_object_unref(G_OBJECT(inMem));
        return UT_ERROR;
    }

    UT_Error result = UT_OK;
    std::map<std::string, ODc_CryptoInfo> cryptoInfo;

    // mimetype first, uncompressed and unencrypted
    {
        GsfInput* src = gsf_infile_child_by_name(inZip, "mimetype");
        if (!src)
        {
            result = UT_ERROR;
        }
        else
        {
            GsfOutput* dst = gsf_outfile_new_child_full(
                outZip, "mimetype", FALSE, "compression-level", 0, (void*)0);
            gsf_off_t s = gsf_input_size(src);
            const guint8* d = gsf_input_read(src, s, nullptr);
            if (dst && d)
                ODe_gsf_output_write(dst, s, d);
            if (dst)
                ODe_gsf_output_close(dst);
            else
                result = UT_ERROR;
            g_object_unref(G_OBJECT(src));
        }
    }

    // encrypt every remaining stream
    for (const std::string& path : paths)
    {
        if (result != UT_OK)
            break;
        if (path == "mimetype" || path == "META-INF/manifest.xml")
            continue;

        GsfInput* src = openPackageStream(inZip, path);
        if (!src)
            continue;
        gsf_off_t s = gsf_input_size(src);
        const guint8* d = (s > 0)
            ? gsf_input_read(src, s, nullptr)
            : reinterpret_cast<const guint8*>("");
        if (!d)
        {
            g_object_unref(G_OBJECT(src));
            continue;
        }

        ODc_CryptoInfo info;
        guint8* enc = nullptr;
        gsize encSize = 0;
        UT_Error cerr = ODc_Crypto::encrypt(d, s, password, info, &enc, &encSize);
        g_object_unref(G_OBJECT(src));
        if (cerr != UT_OK)
        {
            result = cerr;
            break;
        }
        cryptoInfo[path] = info;

        GsfOutput* dst = gsf_outfile_new_child_full(
            outZip, path.c_str(), FALSE, "compression-level", 0, (void*)0);
        if (!dst)
        {
            g_free(enc);
            result = UT_ERROR;
            break;
        }
        ODe_gsf_output_write(dst, encSize, enc);
        ODe_gsf_output_close(dst);
        g_free(enc);
    }

    // META-INF/manifest.xml, annotated with encryption-data
    if (result == UT_OK)
    {
        GsfOutput* man =
            gsf_outfile_new_child(outZip, "META-INF/manifest.xml", FALSE);
        if (!man)
        {
            result = UT_ERROR;
        }
        else
        {
            std::string out =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.4\">\n";

            std::set<std::string> written;
            for (const auto& e : manifestEntries)
            {
                const std::string& path = e.first;
                const std::string& mime = e.second;
                if (!written.insert(path).second)
                    continue;

                auto ci = cryptoInfo.find(path);
                if (ci == cryptoInfo.end())
                {
                    out += " <manifest:file-entry manifest:media-type=\"" +
                        mime + "\" manifest:full-path=\"" + path + "\"/>\n";
                }
                else
                {
                    const ODc_CryptoInfo& info = ci->second;
                    out += " <manifest:file-entry manifest:media-type=\"" +
                        mime + "\" manifest:full-path=\"" + path +
                        "\" manifest:size=\"" +
                        std::to_string(info.m_decryptedSize) + "\">\n"
                        "  <manifest:encryption-data manifest:checksum-type=\"SHA1/1K\" manifest:checksum=\"" +
                        info.m_checksum + "\">\n"
                        "   <manifest:algorithm manifest:algorithm-name=\"" +
                        info.m_algorithm + "\" manifest:initialisation-vector=\"" +
                        info.m_initVector + "\"/>\n"
                        "   <manifest:key-derivation manifest:key-derivation-name=\"" +
                        info.m_keyType + "\" manifest:key-size=\"16\" manifest:iteration-count=\"" +
                        std::to_string(info.m_iterCount) + "\" manifest:salt=\"" +
                        info.m_salt + "\"/>\n"
                        "   <manifest:start-key-generation manifest:start-key-generation-name=\"http://www.w3.org/2000/09/xmldsig#sha1\" manifest:key-size=\"20\"/>\n"
                        "  </manifest:encryption-data>\n"
                        " </manifest:file-entry>\n";
                }
            }

            // entries present in the package but missing from the
            // plaintext manifest (defensive; shouldn't happen)
            for (const auto& ci : cryptoInfo)
            {
                if (written.count(ci.first))
                    continue;
                const ODc_CryptoInfo& info = ci.second;
                out += " <manifest:file-entry manifest:media-type=\"\" manifest:full-path=\"" +
                    ci.first + "\" manifest:size=\"" +
                    std::to_string(info.m_decryptedSize) + "\">\n"
                    "  <manifest:encryption-data manifest:checksum-type=\"SHA1/1K\" manifest:checksum=\"" +
                    info.m_checksum + "\">\n"
                    "   <manifest:algorithm manifest:algorithm-name=\"" +
                    info.m_algorithm + "\" manifest:initialisation-vector=\"" +
                    info.m_initVector + "\"/>\n"
                    "   <manifest:key-derivation manifest:key-derivation-name=\"" +
                    info.m_keyType + "\" manifest:key-size=\"16\" manifest:iteration-count=\"" +
                    std::to_string(info.m_iterCount) + "\" manifest:salt=\"" +
                    info.m_salt + "\"/>\n"
                    "   <manifest:start-key-generation manifest:start-key-generation-name=\"http://www.w3.org/2000/09/xmldsig#sha1\" manifest:key-size=\"20\"/>\n"
                    "  </manifest:encryption-data>\n"
                    " </manifest:file-entry>\n";
            }

            out += "</manifest:manifest>\n";
            ODe_gsf_output_write(man, out.size(),
                                 reinterpret_cast<const guint8*>(out.c_str()));
            ODe_gsf_output_close(man);
        }
    }

    ODe_gsf_output_close(GSF_OUTPUT(outZip));
    g_object_unref(G_OBJECT(inZip));
    g_object_unref(G_OBJECT(inMem));
    return result;
}
