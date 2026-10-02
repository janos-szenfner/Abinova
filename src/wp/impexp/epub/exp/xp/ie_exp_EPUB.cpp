/* AbiSource
 * 
 * Copyright (C) 2011 Volodymyr Rudyj <vladimir.rudoy@gmail.com>
 * Copyright (C) 2025-2026 Abinova contributors
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

#include "ut_std_string.h"
#include "ut_raii.h"
#include "ie_exp_EPUB.h"

/*****************************************************************************/
/*****************************************************************************/
IE_Exp_EPUB::IE_Exp_EPUB(PD_Document * pDocument) :
    IE_Exp(pDocument),
    m_root(NULL),
    m_oebps(NULL),
    m_pHmtlExporter(NULL)
{
    registerDialogs();
//FIXME:FIDENCIO: Remove this clause when Cocoa's dialog is implemented
    AP_Dialog_EpubExportOptions::getEpubExportDefaults(
    &m_exp_opt, XAP_App::getApp());
}
IE_Exp_EPUB::~IE_Exp_EPUB()
{
    /* release any package children left open by a failed export */
    if (m_oebps)
    {
        if (!gsf_output_is_closed(m_oebps))
            gsf_output_close(m_oebps);
        g_object_unref(G_OBJECT(m_oebps));
    }
    if (m_root)
    {
        if (!gsf_output_is_closed(GSF_OUTPUT(m_root)))
            gsf_output_close(GSF_OUTPUT(m_root));
        g_object_unref(G_OBJECT(m_root));
    }
    DELETEP(m_pHmtlExporter);
}

UT_Error IE_Exp_EPUB::_writeDocument()
{
    UT_Error errOptions = doOptions();

    if (errOptions == UT_SAVE_CANCELLED) //see Bug 10840
    {
        return UT_SAVE_CANCELLED;
    }
    else if (errOptions != UT_OK) {
        return UT_ERROR;
    }

    /* headless/scripted exports can override the chapter split depth
     * via --exp-props (e.g. -e "split-level:2") */
    {
        std::string prop = getProperty("split-level");
        if (!prop.empty())
        {
            m_exp_opt.iSplitLevel = atoi(prop.c_str());
            if (m_exp_opt.iSplitLevel < 0 || m_exp_opt.iSplitLevel > 9)
                m_exp_opt.iSplitLevel = 0;
        }
    }

    m_root = gsf_outfile_zip_new(getFp(), NULL);

    if (m_root == NULL)
    {
        UT_DEBUGMSG(("ZIP output is null\n"));
        return UT_ERROR;
    }

    m_oebps = gsf_outfile_new_child(m_root, "OEBPS", TRUE);
    if (m_oebps == NULL)
    {
        UT_DEBUGMSG(("Can`t create oebps output object\n"));
        return UT_ERROR;
    }

    // mimetype must a first file in archive
    UT_GsfOutputPtr mimetype(gsf_outfile_new_child_full(m_root,
        "mimetype", FALSE, "compression-level", 0, NULL));
    if (mimetype)
    {
        gsf_output_write(mimetype.get(), strlen(EPUB_MIMETYPE),
                reinterpret_cast<const guint8*>( EPUB_MIMETYPE));
        gsf_output_close(mimetype.get());
    }

    // We need to create temporary directory to which
    // HTML plugin will export our document
    m_baseTempDir = UT_go_filename_to_uri(g_get_tmp_dir());
    m_baseTempDir += G_DIR_SEPARATOR_S;

    // To generate unique directory name we`ll use document UUID
    m_baseTempDir += getDoc()->getDocUUIDString();
    // We should delete any previous temporary data for this document to prevent
    // odd files appearing in the container
    UT_go_file_remove(m_baseTempDir.c_str(), NULL);
    UT_go_directory_create(m_baseTempDir.c_str(), NULL);

    if (writeContainer() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to write container\n"));
        UT_go_file_remove(m_baseTempDir.c_str(), NULL);
        return UT_ERROR;
    }
    if (writeStructure() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to write document structure\n"));
        UT_go_file_remove(m_baseTempDir.c_str(), NULL);
        return UT_ERROR;
    }
    if (writeNavigation() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to write navigation\n"));
        UT_go_file_remove(m_baseTempDir.c_str(), NULL);
        return UT_ERROR;
    }
    if (package() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to package document\n"));
        UT_go_file_remove(m_baseTempDir.c_str(), NULL);
        return UT_ERROR;
    }

    gsf_output_close(m_oebps);
    gsf_output_close(GSF_OUTPUT(m_root));
    g_object_unref(G_OBJECT(m_oebps)); m_oebps = NULL;
    g_object_unref(G_OBJECT(m_root)); m_root = NULL;

    // After doing all job we should delete temporary files
    UT_go_file_remove(m_baseTempDir.c_str(), NULL);
    return UT_OK;
}


UT_Error IE_Exp_EPUB::writeContainer()
{
    UT_GsfOutputPtr metaInf(gsf_outfile_new_child(m_root, "META-INF", TRUE));

    if (!metaInf)
    {
        UT_DEBUGMSG(("Can`t create META-INF dir\n"));
        return UT_ERROR;
    }
    UT_GsfOutputPtr container(gsf_outfile_new_child(
            GSF_OUTFILE(metaInf.get()), "container.xml", FALSE));
    if (!container)
    {
        UT_DEBUGMSG(("Can`t create container.xml\n"));
        return UT_ERROR;
    }
    GsfXMLOut * containerXml = gsf_xml_out_new(container.get());

    // <container>
    gsf_xml_out_start_element(containerXml, "container");
    gsf_xml_out_add_cstr(containerXml, "version", "1.0");
    gsf_xml_out_add_cstr(containerXml, "xmlns", OCF201_NAMESPACE);
    // <rootfiles>
    gsf_xml_out_start_element(containerXml, "rootfiles");
    // <rootfile>
    gsf_xml_out_start_element(containerXml, "rootfile");
    gsf_xml_out_add_cstr(containerXml, "full-path", "OEBPS/book.opf");
    gsf_xml_out_add_cstr(containerXml, "media-type", OPF_MIMETYPE);
    // </rootfile>
    gsf_xml_out_end_element(containerXml);
    // </rootfiles>
    gsf_xml_out_end_element(containerXml);
    // </container>
    gsf_xml_out_end_element(containerXml);

    g_object_unref(containerXml);
    return UT_OK;
}

UT_Error IE_Exp_EPUB::writeNavigation()
{
    if (m_exp_opt.bEpub2)
    {
        return EPUB2_writeNavigation();
    } else
    {
        if (EPUB2_writeNavigation() == UT_ERROR)
            return UT_ERROR;
        if ( EPUB3_writeNavigation() == UT_ERROR)
            return UT_ERROR;
    }
    
    return UT_OK;
}

UT_Error IE_Exp_EPUB::writeStructure()
{
    if (m_exp_opt.bEpub2)
    {
        return EPUB2_writeStructure();
    } else
    {
        return EPUB3_writeStructure();
    }
}

UT_Error IE_Exp_EPUB::EPUB2_writeStructure()
{
    m_oebpsDir = m_baseTempDir + G_DIR_SEPARATOR_S;
    m_oebpsDir += "OEBPS";

    UT_go_directory_create(m_oebpsDir.c_str(), NULL);

    std::string indexPath = m_oebpsDir + G_DIR_SEPARATOR_S;
    indexPath += "index.xhtml";

    // Exporting document to XHTML using HTML export plugin
	// We need to setup options for HTML exporter according to current settings of EPUB exporter
	std::string htmlProps =
	UT_std_string_sprintf("embed-css:no;html4:no;use-awml:no;declare-xml:yes;mathml-render-png:%s;split-document:%s;split-level:%d;add-identifiers:yes;",
		m_exp_opt.bRenderMathMLToPNG ? "yes" : "no",
		m_exp_opt.bSplitDocument ? "yes" : "no",
		m_exp_opt.iSplitLevel);

    m_pHmtlExporter = new IE_Exp_HTML(getDoc());
    m_pHmtlExporter->suppressDialog(true);
    m_pHmtlExporter->setProps(htmlProps.c_str());
    m_pHmtlExporter->writeFile(indexPath.c_str());

    m_coverURI = m_pHmtlExporter->getFirstImageURI().utf8_str();

    return UT_OK;
}

UT_Error IE_Exp_EPUB::EPUB2_writeNavigation()
{
    UT_GsfOutputPtr ncx(gsf_outfile_new_child(GSF_OUTFILE(m_oebps),
            "toc.ncx", FALSE));
    if (!ncx)
    {
        UT_DEBUGMSG(("Can`t create toc.ncx file\n"));
        return UT_ERROR;
    }
    GsfXMLOut* ncxXml = gsf_xml_out_new(ncx.get());

    // <ncx>
    gsf_xml_out_start_element(ncxXml, "ncx");
    gsf_xml_out_add_cstr(ncxXml, "xmlns", NCX_NAMESPACE);
    gsf_xml_out_add_cstr(ncxXml, "version", "2005-1");
    gsf_xml_out_add_cstr(ncxXml, "xml:lang", getLanguage().c_str());
    // <head>
    gsf_xml_out_start_element(ncxXml, "head");
    // <meta name="dtb:uid" content=... > — must match the OPF
    // dc:identifier verbatim (including the urn:uuid: scheme prefix)
    gsf_xml_out_start_element(ncxXml, "meta");
    gsf_xml_out_add_cstr(ncxXml, "name", "dtb:uid");
    gsf_xml_out_add_cstr(ncxXml, "content",
            ("urn:uuid:" + std::string(getDoc()->getDocUUIDString())).c_str());
    // </meta>
    gsf_xml_out_end_element(ncxXml);
    // <meta name="epub-creator" content=... >
    gsf_xml_out_start_element(ncxXml, "meta");
    gsf_xml_out_add_cstr(ncxXml, "name", "epub-creator");
    gsf_xml_out_add_cstr(ncxXml, "content",
            "Abinova (https://github.com/janos-szenfner/Abinova)");
    // </meta>
    gsf_xml_out_end_element(ncxXml);
    // <meta name="dtb:depth" content=... > — deepest navPoint level
    int iTocDepth = 1;
    for (int i = 0;
        i < m_pHmtlExporter->getNavigationHelper()->getNumTOCEntries(); i++)
    {
        int lvl = 0;
        m_pHmtlExporter->getNavigationHelper()->getNthTOCEntry(i, &lvl);
        if (lvl > iTocDepth)
            iTocDepth = lvl;
    }
    gsf_xml_out_start_element(ncxXml, "meta");
    gsf_xml_out_add_cstr(ncxXml, "name", "dtb:depth");
    gsf_xml_out_add_cstr(ncxXml, "content",
            UT_std_string_sprintf("%d", iTocDepth).c_str());
    // </meta>
    gsf_xml_out_end_element(ncxXml);
    // <meta name="dtb:totalPageCount" content=... >
    gsf_xml_out_start_element(ncxXml, "meta");
    gsf_xml_out_add_cstr(ncxXml, "name", "dtb:totalPageCount");
    gsf_xml_out_add_cstr(ncxXml, "content", "0");
    // </meta>
    gsf_xml_out_end_element(ncxXml);
    // <meta name="dtb:totalPageCount" content=... >
    gsf_xml_out_start_element(ncxXml, "meta");
    gsf_xml_out_add_cstr(ncxXml, "name", "dtb:maxPageCount");
    gsf_xml_out_add_cstr(ncxXml, "content", "0");
    // </meta>
    gsf_xml_out_end_element(ncxXml);
    // </head>
    gsf_xml_out_end_element(ncxXml);

    // <docTitle>
    gsf_xml_out_start_element(ncxXml, "docTitle");
    gsf_xml_out_start_element(ncxXml, "text");
    gsf_xml_out_add_cstr(ncxXml, NULL, getTitle().c_str());
    gsf_xml_out_end_element(ncxXml);
    // </docTitle>
    gsf_xml_out_end_element(ncxXml);

    // <docAuthor>
    gsf_xml_out_start_element(ncxXml, "docAuthor");
    gsf_xml_out_start_element(ncxXml, "text");
    gsf_xml_out_add_cstr(ncxXml, NULL, getAuthor().c_str());
    gsf_xml_out_end_element(ncxXml);
    // </docAuthor>
    gsf_xml_out_end_element(ncxXml);


    // <navMap>
    gsf_xml_out_start_element(ncxXml, "navMap");
    if (m_pHmtlExporter->getNavigationHelper()->hasTOC())
    {
        int lastItemLevel;
        int curItemLevel = 0;
        std::vector<int> tagLevels;
        int tocNum = 0;
        std::string prevFile;
        for (int currentItem = 0; 
            currentItem < m_pHmtlExporter->getNavigationHelper()->getNumTOCEntries(); 
            currentItem++)
        {
            lastItemLevel = curItemLevel;
	    std::string itemStr = m_pHmtlExporter->getNavigationHelper()
			->getNthTOCEntry(currentItem, &curItemLevel).utf8_str();
            PT_DocPosition itemPos;
            m_pHmtlExporter->getNavigationHelper()->getNthTOCEntryPos(currentItem, itemPos);
            
            std::string itemFilename;
            if (m_exp_opt.bSplitDocument)
            {
                itemFilename = m_pHmtlExporter->getNavigationHelper()
					->getFilenameByPosition(itemPos).utf8_str();

                if (itemFilename.length() == 0 || (itemFilename[0] ==  '.'))
                {
                    itemFilename = "index.xhtml";
                }
            } else
            {
                itemFilename = "index.xhtml";
            }

            if (std::find(m_opsId.begin(), m_opsId.end(), 
                          escapeForId(itemFilename)) == m_opsId.end())
            {
                m_opsId.push_back(escapeForId(itemFilename));
            }

            /* content anchors number each file's own headings
             * (AbiTOC0..N); TOC entries for one file are contiguous */
            if (itemFilename != prevFile)
            {
                tocNum = 0;
                prevFile = itemFilename;
            }

            UT_DEBUGMSG(("Item filename %s at pos %d\n", 
                itemFilename.c_str(),itemPos));

            if ((lastItemLevel >= curItemLevel) && (currentItem != 0))
            {
                while ((tagLevels.size() > 0) 
                        && (tagLevels.back() >= curItemLevel))
                {
                    gsf_xml_out_end_element(ncxXml);
                    tagLevels.pop_back();
                }

            }

	    std::string navClass = UT_std_string_sprintf("h%d", curItemLevel);
	    /* the fragment id inside the content file numbers the file's
	     * own headings (AbiTOC0..N); the navPoint id must instead be
	     * unique across the whole NCX document */
	    std::string navId = UT_std_string_sprintf("navPoint-%d", currentItem + 1);
	    std::string navSrc = std::string(itemFilename.c_str()) +
		UT_std_string_sprintf("#AbiTOC%d", tocNum);
            gsf_xml_out_start_element(ncxXml, "navPoint");
            gsf_xml_out_add_cstr(ncxXml, "playOrder",
                    UT_std_string_sprintf("%d", currentItem + 1).c_str());
            gsf_xml_out_add_cstr(ncxXml, "class", navClass.c_str());
            gsf_xml_out_add_cstr(ncxXml, "id", navId.c_str());
            gsf_xml_out_start_element(ncxXml, "navLabel");
            gsf_xml_out_start_element(ncxXml, "text");
            gsf_xml_out_add_cstr(ncxXml, NULL, itemStr.c_str());
            gsf_xml_out_end_element(ncxXml);
            gsf_xml_out_end_element(ncxXml);
            gsf_xml_out_start_element(ncxXml, "content");
            gsf_xml_out_add_cstr(ncxXml, "src", navSrc.c_str());
            gsf_xml_out_end_element(ncxXml);

            tagLevels.push_back(curItemLevel);
            tocNum++;

        }

        closeNTags(ncxXml, tagLevels.size());
    }
    else
    {
        m_opsId.push_back(escapeForId("index.xhtml"));
        gsf_xml_out_start_element(ncxXml, "navPoint");
        gsf_xml_out_add_cstr(ncxXml, "playOrder", "1");
        gsf_xml_out_add_cstr(ncxXml, "class", "h1");
        gsf_xml_out_add_cstr(ncxXml, "id", "index");

        gsf_xml_out_start_element(ncxXml, "navLabel");
        gsf_xml_out_start_element(ncxXml, "text");
        gsf_xml_out_add_cstr(ncxXml, NULL, getTitle().c_str());
        gsf_xml_out_end_element(ncxXml);
        gsf_xml_out_end_element(ncxXml);

        gsf_xml_out_start_element(ncxXml, "content");
        gsf_xml_out_add_cstr(ncxXml, "src", "index.xhtml");
        gsf_xml_out_end_element(ncxXml);
        gsf_xml_out_end_element(ncxXml);
    }
    // </navMap>
    gsf_xml_out_end_element(ncxXml);

    // </ncx>
    gsf_xml_out_end_element(ncxXml);
    g_object_unref(ncxXml);

    return UT_OK;
}

UT_Error IE_Exp_EPUB::EPUB3_writeNavigation()
{
    UT_GsfOutputPtr nav(gsf_outfile_new_child(GSF_OUTFILE(m_oebps),
            "nav.xhtml", FALSE));
    if (!nav)
    {
        UT_DEBUGMSG(("Can`t create nav.xhtml file\n"));
        return UT_ERROR;
    }
    GsfXMLOut* navXHTML = gsf_xml_out_new(nav.get());

     gsf_xml_out_start_element(navXHTML, "html");
    gsf_xml_out_add_cstr(navXHTML, "xmlns", XHTML_NS);
    gsf_xml_out_add_cstr(navXHTML, "xmlns:epub", OPS201_NAMESPACE);
    gsf_xml_out_add_cstr(navXHTML, "xml:lang", getLanguage().c_str());
    
    
    gsf_xml_out_start_element(navXHTML, "head");
    gsf_xml_out_start_element(navXHTML, "title");
    gsf_xml_out_add_cstr(navXHTML, NULL, "Table of Contents");
    gsf_xml_out_end_element(navXHTML);
    gsf_xml_out_start_element(navXHTML, "meta");
    gsf_xml_out_add_cstr(navXHTML, "charset", "utf-8");
    gsf_xml_out_end_element(navXHTML);
    gsf_xml_out_end_element(navXHTML);
    
    gsf_xml_out_start_element(navXHTML, "body");
    gsf_xml_out_start_element(navXHTML, "section");
    gsf_xml_out_add_cstr(navXHTML, "class", "frontmatter TableOfContents");
    gsf_xml_out_start_element(navXHTML, "header");
    gsf_xml_out_start_element(navXHTML, "h1");
    gsf_xml_out_add_cstr(navXHTML, NULL, "Contents");
    gsf_xml_out_end_element(navXHTML);
    gsf_xml_out_end_element(navXHTML);
    gsf_xml_out_start_element(navXHTML, "nav");
    gsf_xml_out_add_cstr(navXHTML, "epub:type", "toc");
    gsf_xml_out_add_cstr(navXHTML, "id", "toc");
    if (m_pHmtlExporter->getNavigationHelper()->hasTOC())
    {
        int lastItemLevel;
        int curItemLevel = 0;
        std::vector<int> tagLevels;
        int tocNum = 0;
        std::string prevFile;
        bool newList = true;
        for (int currentItem = 0; 
            currentItem < m_pHmtlExporter->getNavigationHelper()->getNumTOCEntries(); 
            currentItem++)
        {
            lastItemLevel = curItemLevel;
	    UT_UTF8String itemStr = m_pHmtlExporter->getNavigationHelper()
                ->getNthTOCEntry(currentItem, &curItemLevel);
            PT_DocPosition itemPos;
            m_pHmtlExporter->getNavigationHelper()->getNthTOCEntryPos(currentItem, itemPos);
	    
            std::string itemFilename;
            
            if (m_exp_opt.bSplitDocument)
            {
                itemFilename = m_pHmtlExporter->getNavigationHelper()
					->getFilenameByPosition(itemPos).utf8_str();

                if (itemFilename.length() == 0 || (itemFilename[0] == '.'))
                {
                    itemFilename = "index.xhtml";
                }
            } else
            {
                itemFilename = "index.xhtml";
            }

            if (std::find(m_opsId.begin(), m_opsId.end(), 
                          escapeForId(itemFilename)) == m_opsId.end())
            {
                m_opsId.push_back(escapeForId(itemFilename));
            }

            /* content anchors number each file's own headings
             * (AbiTOC0..N); TOC entries for one file are contiguous */
            if (itemFilename != prevFile)
            {
                tocNum = 0;
                prevFile = itemFilename;
            }

            UT_DEBUGMSG(("Item filename %s at pos %d\n", 
                itemFilename.c_str(),itemPos));

            if ((lastItemLevel >= curItemLevel) && (currentItem != 0))
            {
                while ((tagLevels.size() > 0) 
                        && (tagLevels.back() >= curItemLevel))
                {
                    if (tagLevels.back() == curItemLevel)
                    {
                        gsf_xml_out_end_element(navXHTML);
                    } else
                    {
                        closeNTags(navXHTML, 2);
                    }
                    tagLevels.pop_back();
                }

            } else
            if ((lastItemLevel < curItemLevel) || newList) 
            {
                gsf_xml_out_start_element(navXHTML, "ol");
                newList = false;

            }

	    std::string navClass = UT_std_string_sprintf("h%d", curItemLevel);
	    /* <li> ids must be unique in the nav document; the href keeps
	     * the per-file AbiTOC anchor generated inside the content file */
	    std::string navId = UT_std_string_sprintf("nav-item-%d",
                    currentItem + 1);
	    std::string navSrc = std::string(itemFilename.c_str()) +
		UT_std_string_sprintf("#AbiTOC%d", tocNum);
            gsf_xml_out_start_element(navXHTML, "li");
            gsf_xml_out_add_cstr(navXHTML, "class", navClass.c_str());
            gsf_xml_out_add_cstr(navXHTML, "id", navId.c_str());
            gsf_xml_out_start_element(navXHTML, "a");
            gsf_xml_out_add_cstr(navXHTML, "href", navSrc.c_str());
            gsf_xml_out_add_cstr(navXHTML, NULL, itemStr.utf8_str());
            gsf_xml_out_end_element(navXHTML);
            // gsf_xml_out_end_element(navXHTML);

            tagLevels.push_back(curItemLevel);
            tocNum++;

        }

        closeNTags(navXHTML, tagLevels.size()*2);
    }
    else
    {
        gsf_xml_out_start_element(navXHTML, "ol");
        gsf_xml_out_start_element(navXHTML, "li");
        gsf_xml_out_add_cstr(navXHTML, "class", "h1");
        gsf_xml_out_add_cstr(navXHTML, "id", "index");
        gsf_xml_out_start_element(navXHTML, "a");
        gsf_xml_out_add_cstr(navXHTML, "href", "index.xhtml");
        gsf_xml_out_add_cstr(navXHTML, NULL, getTitle().c_str());
        gsf_xml_out_end_element(navXHTML);
        gsf_xml_out_end_element(navXHTML); 
        gsf_xml_out_end_element(navXHTML); 
    }
   
    gsf_xml_out_end_element(navXHTML);
    // </section>
    gsf_xml_out_end_element(navXHTML);
    gsf_xml_out_end_element(navXHTML);
    
    
    gsf_xml_out_end_element(navXHTML);
    g_object_unref(navXHTML);
    return UT_OK;
}

UT_Error IE_Exp_EPUB::EPUB3_writeStructure()
{
    m_oebpsDir = m_baseTempDir + G_DIR_SEPARATOR_S;
    m_oebpsDir += "OEBPS";

    UT_go_directory_create(m_oebpsDir.c_str(), NULL);

    std::string indexPath = m_oebpsDir + G_DIR_SEPARATOR_S;
    indexPath += "index.xhtml";

    // Exporting document to XHTML using HTML export plugin 
    char *szIndexPath = static_cast<char*>( g_malloc(strlen(indexPath.c_str()) + 1));
    strcpy(szIndexPath, indexPath.c_str());
    IE_Exp_HTML_WriterFactory *pWriterFactory =
		new IE_Exp_EPUB_EPUB3WriterFactory(getLanguage());
    m_pHmtlExporter = new IE_Exp_HTML(getDoc());
    m_pHmtlExporter->setWriterFactory(pWriterFactory);
    m_pHmtlExporter->suppressDialog(true);
    m_pHmtlExporter->setProps(
        UT_std_string_sprintf(
            "embed-css:no;html4:no;use-awml:no;declare-xml:yes;add-identifiers:yes;split-level:%d;",
            m_exp_opt.iSplitLevel).c_str());

    m_pHmtlExporter->set_SplitDocument(m_exp_opt.bSplitDocument);
    m_pHmtlExporter->set_MathMLRenderPNG(m_exp_opt.bRenderMathMLToPNG);
    m_pHmtlExporter->writeFile(szIndexPath);
    m_coverURI = m_pHmtlExporter->getFirstImageURI().utf8_str();
    g_free(szIndexPath);
    DELETEP(pWriterFactory);
    return UT_OK;
}

UT_Error IE_Exp_EPUB::package()
{
    UT_GsfOutputPtr opf(gsf_outfile_new_child(GSF_OUTFILE(m_oebps),
            "book.opf", FALSE));
    if (!opf)
    {
        UT_DEBUGMSG(("Can`t create book.opf\n"));
        return UT_ERROR;
    }
    GsfXMLOut* opfXml = gsf_xml_out_new(opf.get());
    // <package>
    gsf_xml_out_start_element(opfXml, "package");
    if (m_exp_opt.bEpub2)
    {
    gsf_xml_out_add_cstr(opfXml, "version", "2.0");
    } else
    {
       gsf_xml_out_add_cstr(opfXml, "version", "3.0"); 
    }
    gsf_xml_out_add_cstr(opfXml, "xmlns", OPF201_NAMESPACE);
    gsf_xml_out_add_cstr(opfXml, "unique-identifier", "BookId");
    
    if (!m_exp_opt.bEpub2)
    {
       // EPUB 3.3: the draft-era "profile" attribute on <package> was
       // removed in the final specification; xml:lang remains allowed.
       gsf_xml_out_add_cstr(opfXml, "xml:lang", getLanguage().c_str());
    }

    // <metadata>
    gsf_xml_out_start_element(opfXml, "metadata");
    gsf_xml_out_add_cstr(opfXml, "xmlns:dc", DC_NAMESPACE);
    gsf_xml_out_add_cstr(opfXml, "xmlns:opf", OPF201_NAMESPACE);
    // Generation of required Dublin Core metadata
    gsf_xml_out_start_element(opfXml, "dc:title");
    gsf_xml_out_add_cstr(opfXml, NULL, getTitle().c_str());
    gsf_xml_out_end_element(opfXml);
    gsf_xml_out_start_element(opfXml, "dc:identifier");
    gsf_xml_out_add_cstr(opfXml, "id", "BookId");
    {
        // EPUB 3.3: the identifier should be a full URI. The doc UUID is
        // expressed with the urn:uuid scheme.
        std::string sUid = "urn:uuid:";
        sUid += getDoc()->getDocUUIDString();
        gsf_xml_out_add_cstr(opfXml, NULL, sUid.c_str());
    }
    gsf_xml_out_end_element(opfXml);
    gsf_xml_out_start_element(opfXml, "dc:language");
    gsf_xml_out_add_cstr(opfXml, NULL, getLanguage().c_str());
    gsf_xml_out_end_element(opfXml);
    gsf_xml_out_start_element(opfXml, "dc:creator");
    gsf_xml_out_add_cstr(opfXml, "id", "creator");
    if (m_exp_opt.bEpub2)
    {
        // EPUB 2 mechanism for contributor roles; in EPUB 3 this is
        // expressed by the refines/meta element below instead
        gsf_xml_out_add_cstr(opfXml, "opf:role", "aut");
    }
    gsf_xml_out_add_cstr(opfXml, NULL, getAuthor().c_str());
    gsf_xml_out_end_element(opfXml);
    if (!m_exp_opt.bEpub2)
    {
        // EPUB 3 refinement equivalent of opf:role="aut"
        gsf_xml_out_start_element(opfXml, "meta");
        gsf_xml_out_add_cstr(opfXml, "refines", "#creator");
        gsf_xml_out_add_cstr(opfXml, "property", "role");
        gsf_xml_out_add_cstr(opfXml, "scheme", "marc:relators");
        gsf_xml_out_add_cstr(opfXml, NULL, "aut");
        gsf_xml_out_end_element(opfXml);

        // EPUB 3.x requires the last-modification timestamp.
        {
            GDateTime * dt = g_date_time_new_now_utc();
            if (dt)
            {
                gchar * iso = g_date_time_format(dt, "%Y-%m-%dT%H:%M:%SZ");
                gsf_xml_out_start_element(opfXml, "meta");
                gsf_xml_out_add_cstr(opfXml, "property", "dcterms:modified");
                gsf_xml_out_add_cstr(opfXml, NULL, iso);
                gsf_xml_out_end_element(opfXml);
                g_free(iso);
                g_date_time_unref(dt);
            }
        }
    }
    if (m_exp_opt.bEpub2 && !m_coverURI.empty())
    {
        // EPUB 2.0.1 cover convention: a meta entry naming the
        // manifest id of the cover image (EPUB 3 uses the item
        // properties="cover-image" attribute below instead)
        gsf_xml_out_start_element(opfXml, "meta");
        gsf_xml_out_add_cstr(opfXml, "name", "cover");
        gsf_xml_out_add_cstr(opfXml, "content",
                             escapeForId(m_coverURI).c_str());
        gsf_xml_out_end_element(opfXml);
    }
    // </metadata>
    gsf_xml_out_end_element(opfXml);

    // <manifest>
    gsf_xml_out_start_element(opfXml, "manifest");
	gchar *basedir = g_filename_from_uri(m_oebpsDir.c_str(),NULL,NULL);
	UT_ASSERT(basedir);
	std::string _baseDir = basedir;
	std::vector<std::string> listing = getFileList(_baseDir);
	FREEP(basedir);

	for (std::vector<std::string>::iterator i = listing.begin(); i
            != listing.end(); i++)
    {
      std::string idStr = escapeForId(*i);
      std::string fullItemPath = m_oebpsDir + G_DIR_SEPARATOR_S + *i;
        gsf_xml_out_start_element(opfXml, "item");
        gsf_xml_out_add_cstr(opfXml, "id", idStr.c_str());
        gsf_xml_out_add_cstr(opfXml, "href", (*i).c_str());
        gsf_xml_out_add_cstr(opfXml, "media-type",
                getMimeType(fullItemPath).c_str());
        std::string itemProps;
        if (!m_exp_opt.bEpub2 && m_pHmtlExporter->hasMathML((*i)))
        {
            // EPUB 3: a content document using MathML must declare it in
            // the item properties (the old mathml="true" attribute was
            // draft syntax and fails EPUBCheck).
            itemProps = "mathml";
        }
        if (!m_exp_opt.bEpub2 && !m_coverURI.empty() && *i == m_coverURI)
        {
            // EPUB 3: the cover image is declared via the manifest
            // "cover-image" property
            itemProps += itemProps.empty() ? "cover-image" : " cover-image";
        }
        if (!itemProps.empty())
        {
            gsf_xml_out_add_cstr(opfXml, "properties", itemProps.c_str());
        }
        gsf_xml_out_end_element(opfXml);
    }

    // We`ll add navigation files manually
    gsf_xml_out_start_element(opfXml, "item");
    gsf_xml_out_add_cstr(opfXml, "id", "ncx");
    gsf_xml_out_add_cstr(opfXml, "href", "toc.ncx");
    gsf_xml_out_add_cstr(opfXml, "media-type", "application/x-dtbncx+xml");
    gsf_xml_out_end_element(opfXml);
    if (!m_exp_opt.bEpub2)
    {
        gsf_xml_out_start_element(opfXml, "item");
        gsf_xml_out_add_cstr(opfXml, "id", "toc");
        gsf_xml_out_add_cstr(opfXml, "href", "nav.xhtml");
        gsf_xml_out_add_cstr(opfXml, "media-type", "application/xhtml+xml");
        // EPUB 3 requires the Navigation Document to be declared with
        // the "nav" property.
        gsf_xml_out_add_cstr(opfXml, "properties", "nav");
        gsf_xml_out_end_element(opfXml);  
    }
    // </manifest>
    gsf_xml_out_end_element(opfXml);

    // <spine> — the Navigation Document is referenced by the manifest
    // "nav" property and stays out of the spine (a linear="no" itemref
    // would require a hyperlink target to stay reachable per OPF-096)
    gsf_xml_out_start_element(opfXml, "spine");
    gsf_xml_out_add_cstr(opfXml, "toc", "ncx");

    /* the index document holds any preamble content that precedes the
     * first heading; when the TOC did not reference it, it must still
     * lead the spine or its content is unreachable */
    if (std::find(m_opsId.begin(), m_opsId.end(),
                  escapeForId("index.xhtml")) == m_opsId.end())
    {
        m_opsId.insert(m_opsId.begin(), escapeForId("index.xhtml"));
    }

    for(std::vector<std::string>::iterator i = m_opsId.begin(); i != m_opsId.end(); i++)
    {
        gsf_xml_out_start_element(opfXml, "itemref");
        gsf_xml_out_add_cstr(opfXml, "idref", (*i).c_str());
        gsf_xml_out_end_element(opfXml);
    }


    
    // </spine>
    gsf_xml_out_end_element(opfXml);

    // </package>
    gsf_xml_out_end_element(opfXml);
    g_object_unref(opfXml);
    gsf_output_close(opf.get());
    return compress();
}

std::vector<std::string> IE_Exp_EPUB::getFileList(
						  const std::string &directory)
{
  std::vector<std::string> result;
  std::vector<std::string> dirs;

    dirs.push_back(directory);

    while (dirs.size() > 0)
    {
      std::string currentDir = dirs.back();
        dirs.pop_back();
        GDir* baseDir = g_dir_open(currentDir.c_str(), 0, NULL);
        if (!baseDir)
            continue;

        gchar const *entryName = NULL;
        while ((entryName = g_dir_read_name(baseDir)) != NULL)
        {
            if (entryName[0] == '.')
            {
                // Files starting with dot should be skipped - it can be temporary files 
                // created by gsf
                continue;
            }
	    std::string entryFullPath = currentDir + G_DIR_SEPARATOR_S;
            entryFullPath += entryName;

            if (g_file_test(entryFullPath.c_str(), G_FILE_TEST_IS_DIR))
            {
                dirs.push_back(entryFullPath);
            }
            else
            {
                result.push_back(
                        entryFullPath.substr(directory.length() + 1,
                                entryFullPath.length() - directory.length()));
            }
        }

        g_dir_close(baseDir);

    }

    return result;
}

UT_Error IE_Exp_EPUB::compress()
{

    GsfInfile* oebpsDir = gsf_infile_stdio_new(
            UT_go_filename_from_uri(m_oebpsDir.c_str()), NULL);

    if (oebpsDir == NULL)
    {
        UT_DEBUGMSG(("RUDYJ: Can`t open temporary OEBPS directory\n"));
        return UT_ERROR;
    }

    std::vector<std::string> listing = getFileList(
            UT_go_filename_from_uri(m_oebpsDir.c_str()));
    for (std::vector<std::string>::iterator i = listing.begin(); i
            != listing.end(); i++)
    {
        GsfOutput* item = gsf_outfile_new_child(GSF_OUTFILE(m_oebps),
                (*i).c_str(), FALSE);
	std::string fullPath = m_oebpsDir + G_DIR_SEPARATOR_S + *i;
        GsfInput* file = UT_go_file_open(fullPath.c_str(), NULL);

        if (item == NULL || file == NULL)
        {
            UT_DEBUGMSG(("RUDYJ: Can`t open file\n"));
            if (item)
            {
                gsf_output_close(item);
                g_object_unref(item);
            }
            if (file)
                g_object_unref(file);
            g_object_unref(oebpsDir);
            return UT_ERROR;
        }

        gsf_output_seek(item, 0, G_SEEK_SET);
        gsf_input_seek(file, 0, G_SEEK_SET);
        gsf_input_copy(file, item);
        gsf_output_close(item);
        g_object_unref(item);
        g_object_unref(file);
        // Time to delete temporary file
        UT_go_file_remove(fullPath.c_str(), NULL);
    }

    g_object_unref(oebpsDir);

    UT_go_file_remove((m_oebpsDir + G_DIR_SEPARATOR_S + "index.xhtml_files").c_str(), NULL);
    UT_go_file_remove(m_oebpsDir.c_str(), NULL);
	return UT_OK;
}

void IE_Exp_EPUB::closeNTags(GsfXMLOut* xml, int n)
{
    for (int i = 0; i < n; i++)
    {
        gsf_xml_out_end_element(xml);
    }

}

std::string IE_Exp_EPUB::escapeForId(const std::string& src)
{
    /* manifest item ids are xs:ID / NCName values: no '/', spaces or
     * other markup chars, and a letter or '_' first */
    std::string id = UT_escapeXML(src);
    for (std::string::iterator c = id.begin(); c != id.end(); ++c)
    {
        if (!(g_ascii_isalnum(*c) || *c == '.' || *c == '_' || *c == '-'))
            *c = '_';
    }
    if (id.empty() || !(g_ascii_isalpha(id[0]) || id[0] == '_'))
        id.insert(0, "id-");
    return id;
}

std::string IE_Exp_EPUB::getMimeType(const std::string &uri)
{
    /* OCF/EPUB requires accurate media types for manifest items.
     * Cover the content documents, stylesheets, images, fonts and
     * media we may emit explicitly instead of trusting the platform
     * mime resolver. */
    static const struct { const char *ext; const char *mime; } s_mimes[] =
    {
        { "xhtml", "application/xhtml+xml" },
        { "html",  "application/xhtml+xml" },
        { "css",   "text/css" },
        { "ncx",   "application/x-dtbncx+xml" },
        { "opf",   OPF_MIMETYPE },
        { "png",   "image/png" },
        { "jpg",   "image/jpeg" },
        { "jpeg",  "image/jpeg" },
        { "gif",   "image/gif" },
        { "svg",   "image/svg+xml" },
        { "webp",  "image/webp" },
        { "js",    "text/javascript" },
        { "mp3",   "audio/mpeg" },
        { "mp4",   "video/mp4" },
        { "m4a",   "audio/mp4" },
        { "ttf",   "font/ttf" },
        { "otf",   "font/otf" },
        { "woff",  "font/woff" },
        { "woff2", "font/woff2" },
    };

    const gchar *extension = strrchr(uri.c_str(), '.');

    if (extension != NULL)
    {
        for (size_t i = 0; i < G_N_ELEMENTS(s_mimes); i++)
        {
            if (!g_ascii_strcasecmp(extension + 1, s_mimes[i].ext))
                return s_mimes[i].mime;
        }
    }

    return UT_go_get_mime_type(uri.c_str());
}

std::string IE_Exp_EPUB::getAuthor() const
{
    std::string property("");

    if (getDoc()->getMetaDataProp(PD_META_KEY_CREATOR, property)
            && property.size())
    {
        return property;
    }
    return "Converted by Abinova (https://github.com/janos-szenfner/Abinova)";
}

std::string IE_Exp_EPUB::getTitle() const
{
    std::string property("");

    if (getDoc()->getMetaDataProp(PD_META_KEY_TITLE, property)
            && property.size())
    {
        return property;
    }
    return "Untitled";
}

std::string IE_Exp_EPUB::getLanguage() const
{
    std::string property("");

    if (getDoc()->getMetaDataProp(PD_META_KEY_LANGUAGE, property)
            && property.size())
    {
        // EPUB 3.3 requires a BCP 47 tag (en-US), not a POSIX locale
        // name (en_US). Normalize underscores to hyphens.
        std::replace(property.begin(), property.end(), '_', '-');
        return property;
    }
    return "en-US";
}

UT_Error IE_Exp_EPUB::doOptions()
{    
    XAP_Frame * pFrame = XAP_App::getApp()->getLastFocussedFrame();

    if (!pFrame || isCopying()) return UT_OK;
    if (pFrame)
    {
        AV_View * pView = pFrame->getCurrentView();
        if (pView)
        {
            GR_Graphics * pG = pView->getGraphics();
            if (pG && pG->queryProperties(GR_Graphics::DGP_PAPER))
            {
                return UT_OK;
            }
        }
    }

//FIXME:FIDENCIO: Remove this clause when Cocoa's dialog is implemented
    /* run the dialog
     */

    XAP_Dialog_Id id = m_iDialogExport;

    XAP_DialogFactory * pDialogFactory
            = static_cast<XAP_DialogFactory *> (XAP_App::getApp()->getDialogFactory());

    AP_Dialog_EpubExportOptions* pDialog
            = static_cast<AP_Dialog_EpubExportOptions*> (pDialogFactory->requestDialog(id));

    if (pDialog == NULL)
    {
        return UT_OK;
    }

    pDialog->setEpubExportOptions(&m_exp_opt, XAP_App::getApp());

    pDialog->runModal(pFrame);

    /* extract what they did
     */
    bool bSave = pDialog->shouldSave();

    pDialogFactory->releaseDialog(pDialog);

    if (!bSave)
    {
        return UT_SAVE_CANCELLED;
    }
    return UT_OK;
}

void IE_Exp_EPUB::registerDialogs()
{
    // Because there is no implementation of export options dialog 
    // for Mac OS we just use defaults for that platform
#ifdef _WIN32
    XAP_DialogFactory * pFactory = static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());
	m_iDialogExport = pFactory->registerDialog(ap_Dialog_EpubExportOptions_Constructor, XAP_DLGT_NON_PERSISTENT);
#else
    XAP_DialogFactory * pFactory = static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());
	m_iDialogExport = pFactory->registerDialog(ap_Dialog_EpubExportOptions_Constructor, XAP_DLGT_NON_PERSISTENT);
#endif
}
