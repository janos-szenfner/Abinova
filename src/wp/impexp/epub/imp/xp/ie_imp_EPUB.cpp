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

#include "ie_imp_EPUB.h"

/* Strip a namespace prefix ("opf:item" -> "item"). UT_XML does no
 * namespace processing, so prefixed names reach listeners verbatim.
 */
static const gchar* s_localName(const gchar* name)
{
    const gchar* colon = strrchr(name, ':');
    return colon ? colon + 1 : name;
}

static bool s_isElement(const gchar* name, const char* local)
{
    return UT_go_utf8_collate_casefold(s_localName(name), local) == 0;
}

/* Decode %XX escapes in an href/full-path (OCF paths are URIs, so they
 * may be percent-encoded). '+' is left alone: it is only meaningful in
 * query strings, not in path segments.
 */
static std::string s_percentDecode(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '%' && i + 2 < s.size()
                && g_ascii_isxdigit(s[i + 1]) && g_ascii_isxdigit(s[i + 2]))
        {
            out += static_cast<char>((g_ascii_xdigit_value(s[i + 1]) << 4)
                    | g_ascii_xdigit_value(s[i + 2]));
            i += 2;
        }
        else
        {
            out += s[i];
        }
    }

    return out;
}

/* Lexically normalize a '/'-separated relative path: collapse empty
 * and "." components and resolve ".." against the accumulated path.
 * Returns false if the path escapes its root (more ".." than leading
 * components) - such paths must never be used for zip lookup or file
 * extraction.
 */
static bool s_normalizePath(const std::string& path, std::string& out)
{
    std::vector<std::string> stack;
    size_t pos = 0;

    while (pos <= path.size())
    {
        size_t slash = path.find('/', pos);
        std::string comp =
                (slash == std::string::npos) ?
                    path.substr(pos) : path.substr(pos, slash - pos);
        pos = (slash == std::string::npos) ? path.size() + 1 : slash + 1;

        if (comp.empty() || comp == ".")
        {
            continue;
        }
        if (comp == "..")
        {
            if (stack.empty())
            {
                return false;
            }
            stack.pop_back();
            continue;
        }
        stack.push_back(comp);
    }

    out.clear();
    for (std::vector<std::string>::const_iterator i = stack.begin(); i
            != stack.end(); i++)
    {
        if (!out.empty())
        {
            out += '/';
        }
        out += *i;
    }

    return true;
}

/* Open a zip member by its normalized '/'-separated path, one path
 * component at a time (the same approach as the OOXML importer's
 * _childByPath).
 */
static GsfInput* s_childByPath(GsfInfile* zip, const std::string& path)
{
    if (zip == NULL || path.empty() || path[0] == '/')
    {
        return NULL;
    }
    GsfInput* cur = GSF_INPUT(zip);
    g_object_ref(G_OBJECT(cur));

    size_t pos = 0;
    while (pos < path.size() && cur != NULL)
    {
        size_t slash = path.find('/', pos);
        std::string comp = path.substr(pos,
                slash == std::string::npos ?
                    std::string::npos : slash - pos);
        if (comp.empty() || comp == "." || comp == "..")
        {
            g_object_unref(G_OBJECT(cur));
            return NULL;
        }
        GsfInput* next = gsf_infile_child_by_name(GSF_INFILE(cur),
                comp.c_str());
        g_object_unref(G_OBJECT(cur));
        cur = next;
        pos = (slash == std::string::npos) ? path.size() : slash + 1;
    }
    return cur;
}

IE_Imp_EPUB::IE_Imp_EPUB(PD_Document* pDocument) :
    IE_Imp(pDocument),
    m_epub(NULL)
{

}

IE_Imp_EPUB::~IE_Imp_EPUB()
{
    if (m_epub != NULL)
    {
        g_object_unref(G_OBJECT(m_epub));
    }
}

bool IE_Imp_EPUB::pasteFromBuffer(PD_DocumentRange* pDocRange,
				  const unsigned char* pData, UT_uint32 lenData, const char* /*szEncoding*/)
{
    UT_return_val_if_fail(getDoc() == pDocRange->m_pDoc,false);
    UT_return_val_if_fail(pDocRange->m_pos1 == pDocRange->m_pos2,false);

    PD_Document * newDoc = new PD_Document();
    newDoc->createRawDocument();
    IE_Imp_EPUB * pEPUBImp = new IE_Imp_EPUB(newDoc);
    //
    // Turn pData into something that can be imported by the open documenb
    // importer.
    //
    GsfInput * pInStream = gsf_input_memory_new(static_cast<const guint8 *>( pData),
            static_cast<gsf_off_t>( lenData), FALSE);
    pEPUBImp->loadFile(newDoc, pInStream);
    g_object_unref(G_OBJECT(pInStream));

    newDoc->finishRawCreation();

    IE_Imp_PasteListener * pPasteListen = new IE_Imp_PasteListener(getDoc(),
            pDocRange->m_pos1, newDoc);
    newDoc->tellListener(static_cast<PL_Listener *> (pPasteListen));
    delete pPasteListen;
    delete pEPUBImp;
    UNREFP( newDoc);
    return true;
}

UT_Error IE_Imp_EPUB::_loadFile(GsfInput* input)
{
    m_epub = gsf_infile_zip_new(input, NULL);

    if (m_epub == NULL)
    {
        UT_DEBUGMSG(("Can`t create gsf input zip object\n"));
        return UT_ERROR;
    }

    UT_DEBUGMSG(("Reading metadata\n"));
    if (readMetadata() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to read metadata\n"));
        return UT_ERROR;
    }

    UT_DEBUGMSG(("Reading package information\n"));
    if (readPackage() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to read package information\n"));
        return UT_ERROR;
    }

    UT_DEBUGMSG(("Uncompressing OPS data\n"));
    if (uncompress() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to uncompress data\n"));
        return UT_ERROR;
    }

    UT_DEBUGMSG(("Reading OPS data\n"));
    if (readStructure() != UT_OK)
    {
        UT_DEBUGMSG(("Failed to read OPS data\n"));
        return UT_ERROR;
    }

    /* Dublin Core metadata from the OPF package document -> the
     * document's meta properties (dc:title etc.)
     */
    for (std::map<std::string, std::string>::const_iterator m =
            m_metaProps.begin(); m != m_metaProps.end(); m++)
    {
        getDoc()->setMetaDataProp(m->first, m->second);
    }

    return UT_OK;

}

UT_Error IE_Imp_EPUB::readMetadata()
{
    GsfInput* metaInf = gsf_infile_child_by_name(m_epub, "META-INF");

    if (metaInf == NULL)
    {
        UT_DEBUGMSG(("Can`t open container META-INF dir\n"));
        return UT_ERROR;
    }

    GsfInput* meta = gsf_infile_child_by_name(GSF_INFILE(metaInf),
            "container.xml");
    g_object_unref(G_OBJECT(metaInf));

    if (meta == NULL)
    {
        UT_DEBUGMSG(("Can`t open container metadata\n"));
        return UT_ERROR;
    }

    /* zip children share the infile's inflate state - rewind before
     * reading or we get bytes from wherever the previous child left off
     */
    gsf_input_seek(meta, 0, G_SEEK_SET);
    size_t metaSize = gsf_input_size(meta);

    if (metaSize == 0)
    {
        UT_DEBUGMSG(("Container metadata file is empty\n"));
        g_object_unref(G_OBJECT(meta));
        return UT_ERROR;
    }

    const guint8* metaData = gsf_input_read(meta, metaSize, NULL);

    if (metaData == NULL)
    {
        UT_DEBUGMSG(("Can`t read container metadata\n"));
        g_object_unref(G_OBJECT(meta));
        return UT_ERROR;
    }
    /* the returned buffer is owned by the input - copy before unref */
    std::string metaXml(reinterpret_cast<const char*>( metaData), metaSize);
    g_object_unref(G_OBJECT(meta));

    UT_XML metaParser;
    ContainerListener containerListener;
    metaParser.setListener(&containerListener);

    if (metaParser.parse(metaXml.c_str(), metaXml.size()) != UT_OK
            || !containerListener.isRootOk())
    {
        UT_DEBUGMSG(("Incorrect container.xml file\n"));
        return UT_ERROR;
    }

    m_rootfilePath = containerListener.getRootFilePath();

    if (m_rootfilePath.empty())
    {
        UT_DEBUGMSG(("No rootfile declared in container.xml\n"));
        return UT_ERROR;
    }

    return UT_OK;
}

UT_Error IE_Imp_EPUB::readPackage()
{
    /* The rootfile full-path is a '/'-separated path relative to the
     * archive root (OCF 2.0.1 / 3.x). It may be percent-encoded and is
     * the base for resolving all manifest hrefs.
     */
    std::string opfPath;
    if (!s_normalizePath(s_percentDecode(m_rootfilePath), opfPath)
            || opfPath.empty())
    {
        UT_DEBUGMSG(("Invalid rootfile path %s\n", m_rootfilePath.c_str()));
        return UT_ERROR;
    }

    size_t slash = opfPath.find_last_of('/');
    m_opsDir = (slash == std::string::npos) ? "" : opfPath.substr(0, slash);
    UT_DEBUGMSG(("OPS dir: %s\n", m_opsDir.c_str()));

    GsfInput* opf = s_childByPath(m_epub, opfPath);

    if (opf == NULL)
    {
        UT_DEBUGMSG(("Can`t open .opf file %s\n", opfPath.c_str()));
        return UT_ERROR;
    }

    gsf_input_seek(opf, 0, G_SEEK_SET);
    size_t opfSize = gsf_input_size(opf);
    const guint8* opfData = (opfSize > 0) ? gsf_input_read(opf, opfSize, NULL)
            : NULL;
    /* the returned buffer is owned by the input - copy before unref */
    std::string opfXml;
    if (opfData != NULL)
    {
        opfXml.assign(reinterpret_cast<const char*>( opfData), opfSize);
    }
    g_object_unref(G_OBJECT(opf));

    if (opfXml.empty())
    {
        UT_DEBUGMSG(("Can`t read .opf file\n"));
        return UT_ERROR;
    }

    UT_XML opfParser;
    OpfListener opfListener;
    opfParser.setListener(&opfListener);
    if (opfParser.parse(opfXml.c_str(), opfXml.size()) != UT_OK
            || !opfListener.isRootOk())
    {
        UT_DEBUGMSG(("Incorrect opf file found \n"));
        return UT_ERROR;
    }

    m_spine = opfListener.getSpine();
    m_manifestItems = opfListener.getManifestItems();
    m_metaProps = opfListener.getMetadata();

    if (m_spine.empty())
    {
        UT_DEBUGMSG(("Empty spine in .opf file\n"));
        return UT_ERROR;
    }

    return UT_OK;
}

UT_Error IE_Imp_EPUB::uncompress()
{
    gchar* tmpUri = UT_go_filename_to_uri(g_get_tmp_dir());
    m_tmpDir = tmpUri;
    g_free(tmpUri);
    m_tmpDir += G_DIR_SEPARATOR_S;
    m_tmpDir += getDoc()->getDocUUIDString();

    if (!UT_go_directory_create(m_tmpDir.c_str(), NULL))
    {
        UT_DEBUGMSG(("Can`t create temporary directory\n"));
        return UT_ERROR;
    }

    for (std::map<std::string, std::string>::iterator i =
            m_manifestItems.begin(); i != m_manifestItems.end(); i++)
    {
        /* href is a URI relative to the OPF location (OPF 2.0.1/3.x
         * manifest). Decode escapes, join with the OPF directory and
         * resolve ".." lexically against it.
         */
        std::string joined = m_opsDir.empty() ? s_percentDecode(i->second)
                : m_opsDir + "/" + s_percentDecode(i->second);
        std::string zipPath;
        if (!s_normalizePath(joined, zipPath))
        {
            UT_DEBUGMSG(("Href %s escapes the archive - skipped\n",
                    i->second.c_str()));
            continue;
        }
        if (zipPath.empty())
        {
            continue;
        }

        GsfInput* itemInput = s_childByPath(m_epub, zipPath);

        if (itemInput == NULL)
        {
            UT_DEBUGMSG(("Manifest item %s not in archive - skipped\n",
                    zipPath.c_str()));
            continue;
        }

        std::string itemUri = m_tmpDir + G_DIR_SEPARATOR_S + zipPath;
        gchar *itemFileName = UT_go_filename_from_uri(itemUri.c_str());
        GsfOutput* itemOutput = itemFileName ?
                createFileByPath(itemFileName) : NULL;
        g_free(itemFileName);

        if (itemOutput == NULL)
        {
            UT_DEBUGMSG(("Can`t create temp file for %s - skipped\n",
                    zipPath.c_str()));
            g_object_unref(G_OBJECT(itemInput));
            continue;
        }

        gsf_input_seek(itemInput, 0, G_SEEK_SET);
        gsf_input_copy(itemInput, itemOutput);
        gsf_output_close(itemOutput);
        g_object_unref(G_OBJECT(itemInput));
        g_object_unref(G_OBJECT(itemOutput));

        m_extractedItems.insert(make_pair(i->first, itemUri));
    }

    return UT_OK;
}

UT_Error IE_Imp_EPUB::readStructure()
{
    /* the document already has a loading piece table (built by
     * PD_Document::_importFile on the open path and by the caller's
     * createRawDocument() on the paste path) -- a second
     * createRawDocument() here would leak it
     */
    getDoc()->finishRawCreation();

    bool bFirstItem = true;
    for (std::vector<std::string>::iterator i = m_spine.begin(); i
            != m_spine.end(); i++)
    {
        /* A spine itemref with no matching manifest id is malformed;
         * skip it and keep importing the rest of the book.
         */
        std::map<std::string, std::string>::iterator iter =
                m_extractedItems.find(*i);

        if (iter == m_extractedItems.end())
        {
            UT_DEBUGMSG(("Spine item %s not found - skipped\n", (*i).c_str()));
            continue;
        }

        std::string itemPath = iter->second;

        PD_Document *currentDoc = new PD_Document();
        /* importFile() builds the piece table itself; calling
         * createRawDocument() first would orphan it
         */
        const char *suffix = strrchr(itemPath.c_str(), '.');
        XAP_App::getApp()->getPrefs()->setIgnoreNextRecent();
        if (currentDoc->importFile(itemPath.c_str(),
                IE_Imp::fileTypeForSuffix(suffix), true, false, NULL) != UT_OK)
        {
            UT_DEBUGMSG(("Failed to import file %s - skipped\n",
                    itemPath.c_str()));
            UNREFP(currentDoc);
            continue;
        }

        currentDoc->finishRawCreation();
        // const gchar * attributes[3] = {
        //     "listid",
        //     "0",
        //     0
        // };

        // PT_DocPosition pos;
        // currentDoc->getBounds(true, pos);
        // currentDoc->insertStrux(pos, PTX_Block, attributes, PP_NOPROPS, PP_NOPROPS);

        PT_DocPosition posEnd = 0;
        getDoc()->getBounds(true, posEnd);

        if (!bFirstItem)
        {
            getDoc()->insertStrux(posEnd, PTX_Section, PP_NOPROPS, PP_NOPROPS);
            getDoc()->insertStrux(posEnd+1, PTX_Block, PP_NOPROPS, PP_NOPROPS);
            posEnd+=2;
        }

        IE_Imp_PasteListener * pPasteListener = new IE_Imp_PasteListener(
                getDoc(), posEnd, currentDoc);
        /* each spine item is pasted into a fresh empty block; let the
         * chapter's first paragraph donate its style/props to it
         * (otherwise a chapter opening with e.g. a centered Heading 1
         * would lose both)
         */
        pPasteListener->setAdoptFirstBlockFmt(true);
        currentDoc->tellListener(static_cast<PL_Listener *> (pPasteListener));


        DELETEP(pPasteListener);
        UNREFP(currentDoc);
        bFirstItem = false;
    }

    return UT_OK;
}

GsfOutput* IE_Imp_EPUB::createFileByPath(const char* path)
{
    /* Create each missing parent directory (UT_go_directory_create is
     * not recursive), then the file itself. Returns NULL when the
     * file already exists - extraction tmpdirs are per-document so a
     * collision means two manifest hrefs normalized to the same path.
     */
    std::string p(path);
    for (size_t slash = p.find('/'); slash != std::string::npos; slash =
            p.find('/', slash + 1))
    {
        std::string dir = p.substr(0, slash);
        if (dir.empty())
        {
            continue;
        }
        gchar* uri = UT_go_filename_to_uri(dir.c_str());
        if (!UT_go_file_exists(uri))
        {
            UT_go_directory_create(uri, NULL);
        }
        g_free(uri);
    }

    gchar* uri = UT_go_filename_to_uri(path);
    GsfOutput* output = UT_go_file_exists(uri) ?
        NULL : UT_go_file_create(uri, NULL);
    g_free(uri);
    return output;
}

/* Decide whether the element that opened the document matches the
 * expected root, so container/package checks work regardless of
 * namespace prefixes (the old UT_XML::sniff only matched unprefixed
 * names).
 */
static bool s_checkRoot(const gchar* name, const char* expected,
        bool& checked, bool& ok)
{
    if (checked)
    {
        return ok;
    }
    checked = true;
    ok = s_isElement(name, expected);
    return ok;
}

ContainerListener::ContainerListener() :
    m_rootOk(false),
    m_checkedRoot(false)
{

}

void ContainerListener::startElement(const gchar* name, const gchar** atts)
{
    if (!s_checkRoot(name, "container", m_checkedRoot, m_rootOk))
    {
        return;
    }

    if (s_isElement(name, "rootfile"))
    {
        const gchar* fullPath = UT_getAttribute("full-path", atts);
        const gchar* mediaType = UT_getAttribute("media-type", atts);

        if (fullPath == NULL || *fullPath == '\0')
        {
            return;
        }
        m_rootFiles.push_back(
                make_pair(std::string(fullPath),
                        mediaType ? std::string(mediaType) : std::string()));
        UT_DEBUGMSG(("Found rootfile %s\n", fullPath));
    }
}

void ContainerListener::endElement(const gchar* /*name*/)
{
}

void ContainerListener::charData(const gchar* /*buffer*/, int /*length*/)
{

}

const std::string & ContainerListener::getRootFilePath() const
{
    /* Per OCF the right rootfile carries
     * media-type="application/oebps-package+xml"; fall back to the
     * first declared rootfile when none does.
     */
    for (std::vector<string_pair>::const_iterator i = m_rootFiles.begin(); i
            != m_rootFiles.end(); i++)
    {
        if (UT_go_utf8_collate_casefold(i->second.c_str(),
                "application/oebps-package+xml") == 0)
        {
            const_cast<ContainerListener*>(this)->m_rootFilePath = i->first;
            return m_rootFilePath;
        }
    }
    if (!m_rootFiles.empty())
    {
        const_cast<ContainerListener*>(this)->m_rootFilePath =
                m_rootFiles.begin()->first;
    }
    return m_rootFilePath;
}

/*

 */

/* Dublin Core element local-names (namespace prefix stripped by
 * s_localName) -> document metadata keys. OPF 2.0.1 dc-metadata /
 * OPF 3.x dc:* elements inside <metadata>.
 */
static const struct {
    const char* dc;
    const char* key;
} s_dcMeta[] = {
    { "title",       PD_META_KEY_TITLE },
    { "creator",     PD_META_KEY_CREATOR },
    { "subject",     PD_META_KEY_SUBJECT },
    { "description", PD_META_KEY_DESCRIPTION },
    { "publisher",   PD_META_KEY_PUBLISHER },
    { "contributor", PD_META_KEY_CONTRIBUTOR },
    { "date",        PD_META_KEY_DATE },
    { "type",        PD_META_KEY_TYPE },
    { "format",      PD_META_KEY_FORMAT },
    { "source",      PD_META_KEY_SOURCE },
    { "language",    PD_META_KEY_LANGUAGE },
    { "relation",    PD_META_KEY_RELATION },
    { "coverage",    PD_META_KEY_COVERAGE },
    { "rights",      PD_META_KEY_RIGHTS },
};

OpfListener::OpfListener() :
    m_inManifest(false),
    m_inSpine(false),
    m_inMetadata(false),
    m_rootOk(false),
    m_checkedRoot(false)
{

}

void OpfListener::startElement(const gchar* name, const gchar** atts)
{
    if (!s_checkRoot(name, "package", m_checkedRoot, m_rootOk))
    {
        return;
    }

    if (s_isElement(name, "manifest"))
    {
        m_inManifest = true;
    }
    else if (s_isElement(name, "spine"))
    {
        m_inSpine = true;
    }
    else if (s_isElement(name, "metadata"))
    {
        m_inMetadata = true;
    }
    else if (m_inMetadata && m_metaElem.empty())
    {
        /* dc:* children of <metadata> carry the Dublin Core
         * document metadata
         */
        const gchar* local = s_localName(name);
        for (size_t i = 0; i < sizeof(s_dcMeta) / sizeof(s_dcMeta[0]);
                i++)
        {
            if (UT_go_utf8_collate_casefold(local, s_dcMeta[i].dc)
                    == 0)
            {
                m_metaElem = local;
                m_metaKey = s_dcMeta[i].key;
                m_metaText.clear();
                break;
            }
        }
    }
    else if (m_inManifest && s_isElement(name, "item"))
    {
        const gchar* id = UT_getAttribute("id", atts);
        const gchar* href = UT_getAttribute("href", atts);

        if (id == NULL || *id == '\0' || href == NULL || *href == '\0')
        {
            return;
        }
        /* OPF 2.0.1 and 3.x share this manifest shape; a duplicate id
         * keeps the first entry.
         */
        m_manifestItems.insert(
			   make_pair(std::string(id), std::string(href)));
        UT_DEBUGMSG(("Found manifest item: %s\n", href));
    }
    else if (m_inSpine && s_isElement(name, "itemref"))
    {
        /* We can ignore the "linear" attribute - non-linear items are
         * still content and are imported in spine order.
         */
        const gchar* idref = UT_getAttribute("idref", atts);

        if (idref == NULL || *idref == '\0')
        {
            return;
        }
        m_spine.push_back(std::string(idref));
        UT_DEBUGMSG(("Found spine itemref: %s\n", idref));
    }

}

void OpfListener::endElement(const gchar* name)
{
    if (s_isElement(name, "manifest"))
    {
        m_inManifest = false;
    }
    else if (s_isElement(name, "spine"))
    {
        m_inSpine = false;
    }
    else if (s_isElement(name, "metadata"))
    {
        m_inMetadata = false;
    }

    if (!m_metaElem.empty()
            && (UT_go_utf8_collate_casefold(s_localName(name),
                    m_metaElem.c_str()) == 0))
    {
        /* commit the dc element's text; repeatable fields
         * (creator, subject) join with "; "
         */
        size_t b = m_metaText.find_first_not_of(" \t\r\n");
        if (b != std::string::npos)
        {
            size_t e = m_metaText.find_last_not_of(" \t\r\n");
            std::string& cur = m_metadata[m_metaKey];
            if (!cur.empty())
            {
                cur += "; ";
            }
            cur += m_metaText.substr(b, e - b + 1);
        }
        m_metaElem.clear();
        m_metaKey.clear();
        m_metaText.clear();
    }
}

void OpfListener::charData(const gchar* buffer, int length)
{
    if (!m_metaElem.empty())
    {
        m_metaText.append(buffer, length);
    }
}

/*

 */

void NavigationListener::startElement(const gchar* /*name*/, const gchar** /*atts*/)
{

}

void NavigationListener::endElement(const gchar* /*name*/)
{

}

void NavigationListener::charData(const gchar* /*buffer*/, int /*length*/)
{

}
