/* Copyright (C) 2026 Cognition, Inc.
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

#ifndef ODI_RDFPARSER_H
#define ODI_RDFPARSER_H

#include <map>
#include <string>
#include <vector>

#include "ut_xml.h"
#include "pd_DocumentRDF.h"

/**
 * Minimal built-in RDF/XML -> triple parser used when Abinova is built
 * without libredland (WITH_REDLAND). It understands the subset of the
 * RDF/XML grammar that shows up in ODF packages: rdf:Description and
 * typed node elements, rdf:about/rdf:ID/rdf:nodeID subjects,
 * rdf:resource/rdf:nodeID/rdf:datatype objects, literal content,
 * striped syntax (attributes on node elements), rdf:parseType
 * Literal/Resource and rdf:li list items.
 */

struct ODi_RDFTriple
{
    std::string subject;      // URI or "_:name" for a blank node
    bool        subjectIsBNode = false;
    std::string predicate;    // always a URI
    PD_Object   object;
};

class ODi_RDFParser : public UT_XML::Listener
{
public:
    explicit ODi_RDFParser(const std::string& baseURI);
    virtual ~ODi_RDFParser() {}

    virtual void startElement(const gchar* name, const gchar** atts) override;
    virtual void endElement(const gchar* name) override;
    virtual void charData(const gchar* buffer, int length) override;

    const std::vector<ODi_RDFTriple>& triples() const { return m_triples; }

private:
    enum FrameKind
    {
        F_ROOT,       // transparent container (rdf:RDF / document root)
        F_NODE,       // node element; children are property elements
        F_PROP        // property element; object pending
    };

    struct Frame
    {
        FrameKind kind = F_ROOT;
        std::string subject;      // F_NODE: own subject; F_PROP: enclosing subject
        bool subjectIsBNode = false;
        std::string predicate;    // F_PROP: predicate URI
        std::string xsdType;      // F_PROP: rdf:datatype
        std::string text;         // accumulated literal content / XML literal
        bool objectEmitted = false;
        int liCount = 0;          // F_NODE: rdf:li counter
    };

    std::string expand(const std::string& qname) const;
    std::string resolveURI(const std::string& ref) const;
    std::string newBNode();
    void emit(const std::string& s, bool sIsBNode,
              const std::string& p, const PD_Object& o);
    void pushNS(const gchar** atts);
    void popNS();
    const gchar* findAtt(const gchar** atts, const char* name) const;

    std::string m_baseURI;
    std::vector<Frame> m_stack;
    std::vector<ODi_RDFTriple> m_triples;
    std::vector<std::map<std::string, std::string>> m_nsStack;
    int m_bnodeCounter = 0;
    int m_literalDepth = 0;    // >0 inside rdf:parseType="Literal" content
};

#endif /* ODI_RDFPARSER_H */
