/* AbiSource
 *
 * Copyright (C) 2010 Ben Martin
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include "ut_std_string.h"
#include "pd_Document.h"
#include "pd_DocumentRDF.h"
#include "pd_RDFSupport.h"

// Class definition include
#include "ODe_RDFWriter.h"

// Internal includes
#include "ODe_Common.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// RDF support
#ifdef WITH_REDLAND
#include "pd_RDFSupportRed.h"
#define DEBUG_RDF_IO 1
#endif


/**
 * Convert the RDF contained in pDoc->getDocumentRDF() to RDF/XML and
 * store that in manifest.rdf updating the pDoc so that a manifest
 * entry is created in META_INF by the manifest writing code.
 */
bool ODe_RDFWriter::writeRDF( PD_Document* pDoc, GsfOutfile* pODT, PD_RDFModelHandle additionalRDF )
{
    UT_DEBUGMSG(("writeRDF() \n"));

    //
    // Convert the native RDF model into RDF/XML. When libredland is
    // unavailable, toRDFXML() uses the built-in serializer in
    // pd_RDFSupport.cpp.
    //
    PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();
    std::list< PD_RDFModelHandle > ml;
    ml.push_back( rdf );
    ml.push_back( additionalRDF );
    std::string rdfxml = toRDFXML( ml );
    if( rdfxml.empty() )
    {
        // no RDF triples to write
        return true;
    }

    GsfOutput* oss = gsf_outfile_new_child(GSF_OUTFILE(pODT), "manifest.rdf", FALSE);
    ODe_gsf_output_write (oss, rdfxml.size(), (const guint8*)rdfxml.data() );
    ODe_gsf_output_close(oss);

    //
    // add an entry that the manifest writing code will pick up
    //
    {
        UT_ByteBufPtr pByteBuf(new UT_ByteBuf);
        std::string mime_type = "application/rdf+xml";
        PD_DataItemHandle* ppHandle = nullptr;

        if(!pDoc->createDataItem("manifest.rdf", 0, pByteBuf,
                                   mime_type, ppHandle))
        {
            UT_DEBUGMSG(("writeRDF() setting up manifest entry failed!\n"));
        }
    }

    UT_DEBUGMSG(("writeRDF() complete\n"));
    return true;
}
