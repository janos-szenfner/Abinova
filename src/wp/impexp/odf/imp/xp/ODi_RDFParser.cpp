/* Copyright (C) 2026 Cognition, Inc.
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

#include <cstdio>
#include <cstring>

#include "ODi_RDFParser.h"

namespace {

const char* const RDF_NS  = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const char* const XML_NS  = "http://www.w3.org/XML/1998/namespace";

bool isRdfName(const std::string& name, const char* local)
{
    return name == std::string("rdf:") + local;
}

std::string xmlEscape(const gchar* s, int len)
{
    std::string out;
    out.reserve(len);
    for (int i = 0; i < len; i++)
    {
        switch (s[i])
        {
        case '<':  out += "&lt;";  break;
        case '>':  out += "&gt;";  break;
        case '&':  out += "&amp;"; break;
        default:   out += s[i];    break;
        }
    }
    return out;
}

} // anonymous namespace


ODi_RDFParser::ODi_RDFParser(const std::string& baseURI)
    : m_baseURI(baseURI)
{
    Frame root;
    root.kind = F_ROOT;
    m_stack.push_back(root);

    // pre-seed well-known prefixes; document xmlns:* declarations
    // override these as they are encountered
    std::map<std::string, std::string> ns;
    ns["rdf"] = RDF_NS;
    ns["xml"] = XML_NS;
    m_nsStack.push_back(ns);
}


const gchar* ODi_RDFParser::findAtt(const gchar** atts, const char* name) const
{
    if (!atts)
        return nullptr;
    for (const gchar** p = atts; p[0]; p += 2)
        if (!strcmp(p[0], name))
            return p[1];
    return nullptr;
}


void ODi_RDFParser::pushNS(const gchar** atts)
{
    std::map<std::string, std::string> ns;
    if (!m_nsStack.empty())
        ns = m_nsStack.back();   // inherit outer scope
    if (atts)
        for (const gchar** p = atts; p[0]; p += 2)
        {
            if (!strncmp(p[0], "xmlns:", 6))
                ns[p[0] + 6] = p[1];
            else if (!strcmp(p[0], "xmlns"))
                ns[""] = p[1];
        }
    m_nsStack.push_back(ns);
}


void ODi_RDFParser::popNS()
{
    if (m_nsStack.size() > 1)
        m_nsStack.pop_back();
}


std::string ODi_RDFParser::expand(const std::string& qname) const
{
    std::string::size_type colon = qname.find(':');
    if (colon == std::string::npos)
        return qname;
    const std::string prefix = qname.substr(0, colon);
    for (auto it = m_nsStack.rbegin(); it != m_nsStack.rend(); ++it)
    {
        auto f = it->find(prefix);
        if (f != it->end())
            return f->second + qname.substr(colon + 1);
    }
    return qname;
}


std::string ODi_RDFParser::resolveURI(const std::string& ref) const
{
    if (ref.empty())
        return m_baseURI;
    if (ref[0] == '#')
        return m_baseURI + ref;
    // absolute URI: scheme ':' before any '/' or '#'
    std::string::size_type colon = ref.find(':');
    std::string::size_type slash = ref.find_first_of("/#");
    if (colon != std::string::npos && (slash == std::string::npos || colon < slash))
        return ref;
    // relative: resolve against the directory part of the base URI
    std::string::size_type lastSlash = m_baseURI.find_last_of('/');
    if (lastSlash == std::string::npos)
        return ref;
    return m_baseURI.substr(0, lastSlash + 1) + ref;
}


std::string ODi_RDFParser::newBNode()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "_:b%d", ++m_bnodeCounter);
    return buf;
}


void ODi_RDFParser::emit(const std::string& s, bool sIsBNode,
                         const std::string& p, const PD_Object& o)
{
    ODi_RDFTriple t;
    t.subject = s;
    t.subjectIsBNode = sIsBNode;
    t.predicate = p;
    t.object = o;
    m_triples.push_back(t);
}


void ODi_RDFParser::startElement(const gchar* name, const gchar** atts)
{
    pushNS(atts);

    // inside rdf:parseType="Literal": everything is serialized XML
    if (m_literalDepth > 0)
    {
        Frame& f = m_stack.back();
        f.text += '<';
        f.text += name;
        if (atts)
            for (const gchar** p = atts; p[0]; p += 2)
            {
                f.text += ' ';
                f.text += p[0];
                f.text += "=\"";
                f.text += xmlEscape(p[1], strlen(p[1]));
                f.text += '"';
            }
        f.text += '>';
        m_literalDepth++;
        return;
    }

    const Frame& top = m_stack.back();

    if (isRdfName(name, "RDF"))
    {
        // transparent container; children are node elements
        Frame f;
        f.kind = F_ROOT;
        m_stack.push_back(f);
        return;
    }

    if (top.kind == F_NODE || top.kind == F_ROOT)
    {
        /* --- this element is a property element --- */
        Frame f;
        f.kind = F_PROP;
        f.subject = top.subject;
        f.subjectIsBNode = top.subjectIsBNode;
        f.xsdType = "";

        // predicate URI; rdf:li expands to rdf:_N
        if (isRdfName(name, "li"))
        {
            Frame& parent = m_stack.back();
            char buf[64];
            snprintf(buf, sizeof(buf), "%s_%d", RDF_NS, ++parent.liCount);
            f.predicate = buf;
        }
        else
            f.predicate = expand(name);

        const gchar* v;
        if ((v = findAtt(atts, "rdf:parseType")))
        {
            if (!strcmp(v, "Literal"))
            {
                // object is the serialized XML content
                m_literalDepth = 1;
                m_stack.push_back(f);
                return;
            }
            // Resource (and Collection, approximated): children are
            // properties of a fresh blank node
            const std::string bn = newBNode();
            emit(f.subject, f.subjectIsBNode, f.predicate,
                 PD_Object(bn, PD_Object::OBJECT_TYPE_BNODE));
            Frame node;
            node.kind = F_NODE;
            node.subject = bn;
            node.subjectIsBNode = true;
            m_stack.push_back(node);
            return;
        }

        if ((v = findAtt(atts, "rdf:resource")))
        {
            emit(f.subject, f.subjectIsBNode, f.predicate,
                 PD_Object(PD_URI(resolveURI(v))));
            f.objectEmitted = true;
        }
        else if ((v = findAtt(atts, "rdf:nodeID")))
        {
            emit(f.subject, f.subjectIsBNode, f.predicate,
                 PD_Object(std::string("_:") + v, PD_Object::OBJECT_TYPE_BNODE));
            f.objectEmitted = true;
        }
        else if ((v = findAtt(atts, "rdf:datatype")))
        {
            f.xsdType = v;
        }
        else if (findAtt(atts, "rdf:about") || findAtt(atts, "rdf:ID") ||
                 findAtt(atts, "rdf:nodeID"))
        {
            // striped syntax: the object is itself a node element
            std::string subj;
            bool subjIsBNode = false;
            if ((v = findAtt(atts, "rdf:about")))
                subj = resolveURI(v);
            else if ((v = findAtt(atts, "rdf:ID")))
                subj = m_baseURI + "#" + v;
            else if ((v = findAtt(atts, "rdf:nodeID")))
            {
                subj = std::string("_:") + v;
                subjIsBNode = true;
            }
            emit(f.subject, f.subjectIsBNode, f.predicate,
                 PD_Object(subj, subjIsBNode ? PD_Object::OBJECT_TYPE_BNODE
                                           : PD_Object::OBJECT_TYPE_URI));
            // children of this element are properties of that node
            Frame node;
            node.kind = F_NODE;
            node.subject = subj;
            node.subjectIsBNode = subjIsBNode;
            m_stack.push_back(node);
            return;
        }

        m_stack.push_back(f);
        return;
    }

    /* --- top is F_PROP: this element is a node element --- */
    Frame& prop = m_stack.back();

    std::string subj;
    bool subjIsBNode = false;
    const gchar* v;
    if ((v = findAtt(atts, "rdf:about")))
        subj = resolveURI(v);
    else if ((v = findAtt(atts, "rdf:ID")))
        subj = m_baseURI + "#" + v;
    else if ((v = findAtt(atts, "rdf:nodeID")))
    {
        subj = std::string("_:") + v;
        subjIsBNode = true;
    }
    else
    {
        subj = newBNode();
        subjIsBNode = true;
    }

    if (!prop.objectEmitted)
    {
        emit(prop.subject, prop.subjectIsBNode, prop.predicate,
             PD_Object(subj, subjIsBNode ? PD_Object::OBJECT_TYPE_BNODE
                                       : PD_Object::OBJECT_TYPE_URI));
        prop.objectEmitted = true;
    }

    // typed node element -> rdf:type triple
    if (!isRdfName(name, "Description"))
        emit(subj, subjIsBNode, std::string(RDF_NS) + "type",
             PD_Object(PD_URI(expand(name))));

    // striped syntax: non-rdf attributes are literal properties
    if (atts)
        for (const gchar** p = atts; p[0]; p += 2)
        {
            if (!strncmp(p[0], "rdf:", 4) || !strncmp(p[0], "xmlns", 5) ||
                !strncmp(p[0], "xml:", 4))
                continue;
            emit(subj, subjIsBNode, expand(p[0]),
                 PD_Object(std::string(p[1]), PD_Object::OBJECT_TYPE_LITERAL));
        }

    Frame node;
    node.kind = F_NODE;
    node.subject = subj;
    node.subjectIsBNode = subjIsBNode;
    m_stack.push_back(node);
}


void ODi_RDFParser::endElement(const gchar* name)
{
    if (m_literalDepth > 0)
    {
        m_literalDepth--;
        Frame& f = m_stack.back();
        f.text += "</";
        f.text += name;
        f.text += '>';
        popNS();
        if (m_literalDepth == 0)
        {
            // the literal element itself was the last </> we appended;
            // the frame's own close tag must not be part of the object
            std::string closeTag = std::string("</") + name + ">";
            if (f.text.size() >= closeTag.size() &&
                f.text.compare(f.text.size() - closeTag.size(),
                               closeTag.size(), closeTag) == 0)
                f.text.erase(f.text.size() - closeTag.size());

            emit(f.subject, f.subjectIsBNode, f.predicate,
                 PD_Object(f.text, PD_Object::OBJECT_TYPE_LITERAL,
                           std::string(RDF_NS) + "XMLLiteral"));
            m_stack.pop_back();
        }
        return;
    }

    const Frame& top = m_stack.back();
    if (top.kind == F_PROP && !top.objectEmitted)
    {
        emit(top.subject, top.subjectIsBNode, top.predicate,
             PD_Object(top.text, PD_Object::OBJECT_TYPE_LITERAL, top.xsdType));
    }

    m_stack.pop_back();
    popNS();
}


void ODi_RDFParser::charData(const gchar* buffer, int length)
{
    if (m_stack.empty())
        return;
    Frame& f = m_stack.back();
    if (f.kind != F_PROP)
        return;
    if (m_literalDepth > 0)
        f.text += xmlEscape(buffer, length);
    else
        f.text.append(buffer, length);
}
