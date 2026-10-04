/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 2026 Abinova contributors
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

/* Tests for the document RDF model: value classes (PD_URI/PD_Object/
 * PD_Literal/PD_RDFStatement), the PP_AttrProp-backed triple store
 * behind PD_DocumentRDF, batch mutations (add/remove/commit/rollback/
 * scope-commit), prefix expansion, and the xml:id-anchored scoping
 * helpers.  SPARQL execution itself needs redland which this build
 * lacks, so the SPARQL-driven paths are exercised only up to their
 * empty-bindings early-outs. */

#include <algorithm>
#include <sstream>
#include <string>
#include <set>
#include <list>

#include "tf_test.h"

#include "ut_string.h"

#include "pd_Document.h"
#include "pd_DocumentRDF.h"
#include "pf_Frag.h"
#include "pf_Frag_Object.h"
#include "pf_Frag_Strux.h"
#include "pt_Types.h"
#include "xad_Document.h"

#define TFSUITE "core.text.ptbl.documentrdf"

namespace {

/* minimal document: section + block text; the caller can append more
 * content before calling finish() */
struct RdfDoc
{
	RdfDoc() = default;
	RdfDoc(const RdfDoc &) = delete;
	RdfDoc &operator=(const RdfDoc &) = delete;

	bool build()
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
			return false;
		if (!doc->appendStrux(PTX_Section, PP_NOPROPS, &sdhSection))
			return false;
		return true;
	}

	bool para(const UT_UCS4String & text,
			  const PP_PropertyVector & attrs = PP_NOPROPS,
			  pf_Frag_Strux **out = nullptr)
	{
		if (!doc->appendStrux(PTX_Block, attrs, out))
			return false;
		return doc->appendSpan(text.ucs4_str(), text.length());
	}

	void finish()
	{
		doc->finishRawCreation();
	}

	~RdfDoc()
	{
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	pf_Frag_Strux *sdhSection = nullptr;
};

/* every triple in the model, via the public iterator */
std::list<PD_RDFStatement> allTriples(PD_RDFModelHandle model)
{
	std::list<PD_RDFStatement> ret;
	for (PD_RDFModelIterator it = model->begin();
		 it != model->end(); ++it)
		ret.push_back(*it);
	return ret;
}

} // namespace

TFTEST_MAIN("PD_URI value semantics + stream round-trip")
{
	PD_URI empty;
	TFPASS(!empty.isValid());
	TFPASS(empty.empty());
	TFPASS(empty.length() == 0);

	PD_URI u("http://example.org/x");
	TFPASS(u.isValid());
	TFPASS(!u.empty());
	TFPASS(u.length() == 20);
	TFPASS(u.toString() == "http://example.org/x");
	TFPASS(std::string(u.c_str()) == "http://example.org/x");

	TFPASS(u == PD_URI("http://example.org/x"));
	TFPASS(u == std::string("http://example.org/x"));
	TFPASS(!(u == PD_URI("http://example.org/y")));
	TFPASS(PD_URI("aaa") < PD_URI("bbb"));
	TFPASS(!(PD_URI("bbb") < PD_URI("aaa")));

	/* write/read round-trip preserves the value */
	std::stringstream ss;
	TFPASS(u.write(ss));
	PD_URI u2;
	TFPASS(u2.read(ss));
	TFPASS(u2 == u);

	/* a second value can follow in the same stream */
	PD_URI other("pred");
	TFPASS(other.write(ss));
	PD_URI other2;
	TFPASS(other2.read(ss));
	TFPASS(other2 == other);
}

TFTEST_MAIN("PD_Object types + stream round-trip")
{
	/* default ctor: URI object */
	PD_Object uriObj("http://example.org/o");
	TFPASS(uriObj.isURI());
	TFPASS(!uriObj.isLiteral());
	TFPASS(!uriObj.isBNode());
	TFPASS(!uriObj.hasXSDType());
	TFPASS(uriObj.getXSDType().empty());
	TFPASS(uriObj.getObjectType() == PD_Object::OBJECT_TYPE_URI);

	/* PD_URI conversion ctor */
	PD_Object fromUri(PD_URI("http://example.org/u"));
	TFPASS(fromUri.isURI());
	TFPASS(fromUri == PD_URI("http://example.org/u"));

	/* literal with XSD type */
	PD_Object lit("42", PD_Object::OBJECT_TYPE_LITERAL,
				  "http://www.w3.org/2001/XMLSchema#int");
	TFPASS(lit.isLiteral());
	TFPASS(!lit.isURI());
	TFPASS(lit.hasXSDType());
	TFPASS(lit.getXSDType() == "http://www.w3.org/2001/XMLSchema#int");

	/* bnode */
	PD_Object bn("_:b0", PD_Object::OBJECT_TYPE_BNODE);
	TFPASS(bn.isBNode());
	TFPASS(!bn.isLiteral());

	/* PD_Literal convenience ctor */
	PD_Literal l1("hello");
	TFPASS(l1.isLiteral());
	TFPASS(!l1.hasXSDType());
	PD_Literal l2("7", "http://www.w3.org/2001/XMLSchema#int");
	TFPASS(l2.hasXSDType());

	/* write/read round-trip preserves value, type and xsdtype */
	std::stringstream ss;
	TFPASS(lit.write(ss));
	PD_Object lit2;
	TFPASS(lit2.read(ss));
	TFPASS(lit2.isLiteral());
	TFPASS(lit2.hasXSDType());
	TFPASS(lit2.getXSDType() == lit.getXSDType());
	TFPASS(lit2.toString() == "42");

	TFPASS(uriObj.write(ss));
	PD_Object uriObj2;
	TFPASS(uriObj2.read(ss));
	TFPASS(uriObj2.isURI());
	TFPASS(uriObj2.toString() == "http://example.org/o");
}

TFTEST_MAIN("PD_RDFStatement construction + prefix conversion")
{
	PD_RDFStatement empty;
	TFPASS(!empty.isValid());

	PD_URI s("http://xmlns.com/foaf/0.1/Person");
	PD_URI p("http://xmlns.com/foaf/0.1/name");
	PD_Literal o("Alice");

	PD_RDFStatement st(s, p, o);
	TFPASS(st.isValid());
	TFPASS(st.getSubject() == s);
	TFPASS(st.getPredicate() == p);
	TFPASS(st.getObject() == PD_Object(o));
	TFPASS(st.toString().find("Alice") != std::string::npos);

	/* string ctor + literal ctor */
	PD_RDFStatement st2(std::string("http://a/s"), std::string("http://a/p"), o);
	TFPASS(st2.isValid());
	PD_RDFStatement st3(std::string("http://a/s"), std::string("http://a/p"),
						PD_Literal("x"));
	TFPASS(st3.isValid());

	TFPASS(st == PD_RDFStatement(s, p, PD_Object("Alice",
				 PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(!(st == st2));

	/* model-based ctor expands prefixed names */
	RdfDoc d;
	TFPASS(d.build());
	d.finish();
	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();
	TFPASS(rdf.get() != nullptr);

	PD_RDFStatement pst(rdf, PD_URI("foaf:Person"), PD_URI("foaf:name"),
						PD_Object("foaf:X"));
	TFPASS(pst.getSubject() == PD_URI("http://xmlns.com/foaf/0.1/Person"));
	TFPASS(pst.getPredicate() == PD_URI("http://xmlns.com/foaf/0.1/name"));
	TFPASS(pst.getObject().toString() == "http://xmlns.com/foaf/0.1/X");

	/* statement-level prefix round-trip */
	PD_RDFStatement prefixed = st.uriToPrefixed(rdf);
	TFPASS(prefixed.getSubject() == PD_URI("foaf:Person"));
	TFPASS(prefixed.getPredicate() == PD_URI("foaf:name"));
	PD_RDFStatement expanded = prefixed.prefixedToURI(rdf);
	TFPASS(expanded == st);
}

TFTEST_MAIN("PD_RDFModel prefix maps")
{
	RdfDoc d;
	TFPASS(d.build());
	d.finish();
	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();
	TFPASS(rdf.get() != nullptr);

	/* builtin prefix table */
	TFPASS(rdf->uriToPrefixed("http://xmlns.com/foaf/0.1/name") == "foaf:name");
	TFPASS(rdf->prefixedToURI("foaf:name") == "http://xmlns.com/foaf/0.1/name");
	/* unknown namespaces pass through unchanged */
	TFPASS(rdf->uriToPrefixed("http://other.org/x") == "http://other.org/x");
	TFPASS(rdf->prefixedToURI("nonsuch:x") == "nonsuch:x");
	TFPASS(rdf->prefixedToURI("nocolon") == "nocolon");

	/* PD_URI::prefixedToURI via model */
	PD_URI prefixed("pkg:idref");
	PD_URI full = prefixed.prefixedToURI(rdf);
	TFPASS(full.toString().find("opendocument") != std::string::npos);
	TFPASS(full.toString().find("idref") != std::string::npos);

	TFPASS(PD_DocumentRDF::getManifestURI().toString() ==
		   "http://abiword.org/manifest.rdf");
	TFPASS(rdf->makeLegalXMLID("a b-c.d!e") == "a_b_c_d_e");
	TFPASS(rdf->makeLegalXMLID("keepMe09") == "keepMe09");
	TFPASS(rdf->makeLegalXMLID("").empty());
}

TFTEST_MAIN("mutation add/commit/query")
{
	RdfDoc d;
	TFPASS(d.build());
	const UT_UCS4String t1("alpha ");
	TFPASS(d.para(t1));
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();
	TFPASS(rdf.get() != nullptr);
	TFPASS(rdf->getTripleCount() == 0);
	TFPASS(rdf->empty());
	TFPASS(!rdf->haveSemItems());
	TFPASS(rdf->begin() == rdf->end());

	const PD_URI s1("http://ex.org/s1");
	const PD_URI s2("http://ex.org/s2");
	const PD_URI pName("http://ex.org/name");
	const PD_URI pSize("http://ex.org/size");

	/* three triples over two subjects; (s1,pName) gets two objects */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		TFPASS(m.get() != nullptr);
		TFPASS(m->add(s1, pName, PD_Literal("one")));
		TFPASS(m->add(s1, pName, PD_Literal("two")));
		TFPASS(m->add(s1, pSize, PD_Object("http://ex.org/big")));
		TFPASS(m->add(s2, pName, PD_Literal("s2name")));
		TFPASS(m->commit() == UT_OK);
		/* a second commit on the same mutation is a no-op */
		TFPASS(m->commit() == UT_OK);
	}

	TFPASS(rdf->getTripleCount() == 4);
	TFPASS(!rdf->empty());
	TFPASS(rdf->contains(s1, pName, PD_Object("one", PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(rdf->contains(s1, pName, PD_Object("two", PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(rdf->contains(s1, pSize, PD_Object("http://ex.org/big")));
	TFPASS(rdf->contains(s2, pName, PD_Object("s2name", PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(!rdf->contains(s1, pName, PD_Object("three", PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(rdf->contains(PD_RDFStatement(s1, pName, PD_Literal("one"))));

	/* getObjects / getObject */
	{
		PD_ObjectList objs = rdf->getObjects(s1, pName);
		TFPASS(objs.size() == 2);
		/* getObject returns any one of the matches */
		const std::string got = rdf->getObject(s1, pName).toString();
		TFPASS(got == "one" || got == "two");
		PD_Object missing = rdf->getObject(s1, PD_URI("http://ex.org/none"));
		TFPASS(!missing.isValid() || missing.toString().empty());
	}

	/* getSubjects / getSubject / getAllSubjects */
	{
		PD_URIList subs = rdf->getSubjects(pName, PD_Object("s2name",
						PD_Object::OBJECT_TYPE_LITERAL));
		TFPASS(subs.size() == 1);
		TFPASS(subs.front() == s2);
		TFPASS(rdf->getSubject(pName, PD_Object("s2name",
					 PD_Object::OBJECT_TYPE_LITERAL)) == s2);
		PD_URIList all = rdf->getAllSubjects();
		TFPASS(all.size() == 2);
	}

	/* getArcsOut returns the whole (p,o) multiset for a subject */
	{
		POCol arcs = rdf->getArcsOut(s1);
		TFPASS(arcs.size() == 3);
		TFPASS(arcs.count(pName) == 2);
		TFPASS(arcs.count(pSize) == 1);
		TFPASS(rdf->getArcsOut(PD_URI("http://ex.org/notthere")).empty());
	}

	/* iteration walks every triple exactly once */
	{
		std::list<PD_RDFStatement> all = allTriples(rdf);
		TFPASS(all.size() == 4);
		TFPASS(std::count_if(all.begin(), all.end(),
			[&](const PD_RDFStatement & st) {
				return st.getSubject() == s1 &&
					   st.getPredicate() == pName;
			}) == 2);
	}
}

TFTEST_MAIN("mutation remove variants + rollback")
{
	RdfDoc d;
	TFPASS(d.build());
	const UT_UCS4String t1("beta ");
	TFPASS(d.para(t1));
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();
	const PD_URI s1("http://ex.org/r1");
	const PD_URI p1("http://ex.org/p");
	const PD_URI p2("http://ex.org/q");

	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(s1, p1, PD_Literal("a"));
		m->add(s1, p1, PD_Literal("b"));
		m->add(s1, p2, PD_Literal("c"));
		m->commit();
	}
	TFPASS(rdf->getTripleCount() == 3);

	/* remove(s,p,o) drops just that object */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->remove(s1, p1, PD_Object("a", PD_Object::OBJECT_TYPE_LITERAL));
		m->commit();
	}
	TFPASS(rdf->getTripleCount() == 2);
	TFPASS(!rdf->contains(s1, p1, PD_Object("a", PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(rdf->contains(s1, p1, PD_Object("b", PD_Object::OBJECT_TYPE_LITERAL)));

	/* add-then-remove inside one mutation never persists it */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(s1, p1, PD_Literal("ephemeral"));
		m->remove(s1, p1, PD_Object("ephemeral",
				  PD_Object::OBJECT_TYPE_LITERAL));
		m->commit();
	}
	TFPASS(!rdf->contains(s1, p1, PD_Object("ephemeral",
				 PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(rdf->getTripleCount() == 2);

	/* remove(s,p) drops every object of that predicate */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->remove(s1, p1);
		m->commit();
	}
	TFPASS(rdf->getTripleCount() == 1);
	TFPASS(!rdf->contains(s1, p1, PD_Object("b", PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(rdf->contains(s1, p2, PD_Object("c", PD_Object::OBJECT_TYPE_LITERAL)));

	/* rollback abandons pending adds */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(s1, p1, PD_Literal("gone"));
		m->rollback();
		/* dtor must not resurrect the change */
	}
	TFPASS(!rdf->contains(s1, p1, PD_Object("gone",
				 PD_Object::OBJECT_TYPE_LITERAL)));

	/* scope-exit auto-commit */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(s1, p1, PD_Literal("scoped"));
		/* no explicit commit -- dtor does it */
	}
	TFPASS(rdf->contains(s1, p1, PD_Object("scoped",
				PD_Object::OBJECT_TYPE_LITERAL)));

	/* statement + statement-list remove */
	{
		PD_RDFStatement stA(s1, p1, PD_Literal("scoped"));
		PD_RDFStatement stB(s1, p2, PD_Literal("c"));
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->remove(stA);
		m->commit();
		TFPASS(rdf->getTripleCount() == 1);

		std::list<PD_RDFStatement> sl;
		sl.push_back(stB);
		PD_DocumentRDFMutationHandle m2 = rdf->createMutation();
		m2->remove(sl);
		m2->commit();
		TFPASS(rdf->getTripleCount() == 0);
		TFPASS(rdf->empty());
	}

	/* remove(s,p,PD_URI) overload + add(statement) + add(model) */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(PD_RDFStatement(s1, p1, PD_Literal("viaSt")));
		m->add(s1, p2, PD_Object("http://ex.org/uriobj"));
		m->commit();

		PD_RDFModelHandle scratch = rdf->createScratchModel();
		TFPASS(scratch.get() != nullptr);
		{
			PD_DocumentRDFMutationHandle sm = scratch->createMutation();
			sm->add(PD_URI("http://ex.org/sx"), p1, PD_Literal("x1"));
			sm->commit();
		}
		TFPASS(scratch->getTripleCount() == 1);

		PD_DocumentRDFMutationHandle m2 = rdf->createMutation();
		TFPASS(m2->add(scratch) == 1);
		m2->commit();
		TFPASS(rdf->contains(PD_URI("http://ex.org/sx"), p1,
				 PD_Object("x1", PD_Object::OBJECT_TYPE_LITERAL)));

		PD_DocumentRDFMutationHandle m3 = rdf->createMutation();
		m3->remove(s1, p2, PD_URI("http://ex.org/uriobj"));
		m3->commit();
		TFPASS(!rdf->contains(s1, p2, PD_Object("http://ex.org/uriobj")));
	}

	/* createBNode mints distinct bnodes */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		PD_URI b1 = m->createBNode();
		PD_URI b2 = m->createBNode();
		TFPASS(b1.isValid());
		TFPASS(!(b1 == b2));
		m->rollback();
	}
}

TFTEST_MAIN("xml:id anchors scope RDF lookups")
{
	RdfDoc d;
	TFPASS(d.build());

	const UT_UCS4String t1("first ");
	const UT_UCS4String t2("mid anchored ");
	const UT_UCS4String t3("last ");

	pf_Frag_Strux *blkA = nullptr;
	pf_Frag_Strux *blkB = nullptr;

	TFPASS(d.para(t1, PP_NOPROPS));
	TFPASS(d.para(t2, {"xml:id", "blkA"}, &blkA));
	/* anchor pair around the tail of block B */
	{
		PP_PropertyVector a1 = {"xml:id", "anchor1"};
		TFPASS(d.doc->appendObject(PTO_RDFAnchor, a1));
	}
	TFPASS(d.para(t3, PP_NOPROPS, &blkB));
	{
		PP_PropertyVector a2 = {"xml:id", "anchor1", "rdf:end", "yes"};
		TFPASS(d.doc->appendObject(PTO_RDFAnchor, a2));
		PP_PropertyVector bm = {"xml:id", "bm1", "type", "start", "name", "b1"};
		TFPASS(d.doc->appendObject(PTO_Bookmark, bm));
	}
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();
	TFPASS(rdf.get() != nullptr);

	/* getAllIDs collects xml:id from strux + object frags */
	std::set<std::string> ids;
	rdf->getAllIDs(ids);
	TFPASS(ids.count("blkA") == 1);
	TFPASS(ids.count("anchor1") == 1);
	TFPASS(ids.count("bm1") == 1);

	/* getIDRange covers the marked block */
	{
		std::pair<PT_DocPosition, PT_DocPosition> r =
			rdf->getIDRange("blkA");
		TFPASS(r.first == d.doc->getStruxPosition(blkA));
		TFPASS(r.second > r.first);
		TFPASS(rdf->getIDRange("nonexistent").first == 0);
	}

	/* RDFAnchor decodes xml:id + rdf:end from object frags */
	{
		pf_Frag *pf = d.doc->getFragFromPosition(0);
		int anchorsSeen = 0;
		for (; pf; pf = pf->getNext())
		{
			if (pf->getType() != pf_Frag::PFT_Object)
				continue;
			const pf_Frag_Object *po =
				static_cast<const pf_Frag_Object*>(pf);
			if (po->getObjectType() != PTO_RDFAnchor)
				continue;
			RDFAnchor a(d.doc, pf);
			TFPASS(a.getID() == "anchor1");
			++anchorsSeen;
			if (anchorsSeen == 1)
				TFPASS(!a.isEnd());
			else
				TFPASS(a.isEnd());
		}
		TFPASS(anchorsSeen == 2);
	}

	/* a position between the anchor pair resolves anchor1 (+blkA
	 * via the containing block for positions inside it) */
	{
		std::set<std::string> got;
		PT_DocPosition posInBlkA =
			d.doc->getStruxPosition(blkA) + 2;
		rdf->addRelevantIDsForPosition(got, posInBlkA);
		TFPASS(got.count("blkA") == 1);

		std::set<std::string> got2;
		PT_DocPosition posAfterAnchor =
			d.doc->getStruxPosition(blkB) + 1;
		rdf->addRelevantIDsForPosition(got2, posAfterAnchor);
		TFPASS(got2.count("anchor1") == 1);

		std::set<std::string> got3;
		rdf->addRelevantIDsForRange(got3,
			std::make_pair(posInBlkA, posAfterAnchor));
		TFPASS(got3.count("blkA") == 1);
		TFPASS(got3.count("anchor1") == 1);
	}

	/* RDF tied to blkA via pkg:idref shows up in restricted models */
	{
		const PD_URI idref(
			"http://docs.oasis-open.org/opendocument/meta/package/common#idref");
		const PD_URI subj("http://ex.org/about-blkA");
		const PD_URI pred("http://ex.org/note");

		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(subj, pred, PD_Literal("blkA note"));
		m->add(subj, idref, PD_Literal("blkA"));
		m->add(PD_URI("http://ex.org/other"), pred, PD_Literal("unrelated"));
		m->commit();

		/* single-xmlid restricted model copies the linked subject */
		PD_RDFModelHandle limited =
			rdf->createRestrictedModelForXMLIDs("blkA",
											  std::set<std::string>{"blkA"});
		TFPASS(limited.get() != nullptr);
		TFPASS(limited->contains(subj, pred,
					 PD_Object("blkA note", PD_Object::OBJECT_TYPE_LITERAL)));
		TFPASS(limited->contains(subj, idref,
					 PD_Object("blkA", PD_Object::OBJECT_TYPE_LITERAL)));
		TFPASS(!limited->contains(PD_URI("http://ex.org/other"), pred,
					 PD_Object("unrelated", PD_Object::OBJECT_TYPE_LITERAL)));

		/* set-of-xmlids form */
		PD_RDFModelHandle limited2 =
			rdf->createRestrictedModelForXMLIDs(std::set<std::string>{"blkA"});
		TFPASS(limited2.get() != nullptr);

		/* addRDFForID copies the linked subject into a scratch model */
		PD_RDFModelHandle scratch = rdf->createScratchModel();
		PD_DocumentRDFMutationHandle sm = scratch->createMutation();
		rdf->addRDFForID("blkA", sm);
		sm->commit();
		TFPASS(scratch->contains(subj, pred,
					 PD_Object("blkA note", PD_Object::OBJECT_TYPE_LITERAL)));
		TFPASS(!scratch->contains(PD_URI("http://ex.org/other"), pred,
					 PD_Object("unrelated", PD_Object::OBJECT_TYPE_LITERAL)));
	}

	/* position-scoped model: getRDFAtPosition never crashes and
	 * yields a model */
	{
		PD_RDFModelHandle at =
			rdf->getRDFAtPosition(d.doc->getStruxPosition(blkA) + 1);
		TFPASS(at.get() != nullptr);
	}

	/* relink path is a no-op without redland but must not crash */
	rdf->relinkRDFToNewXMLID("blkA", "blkA2", false);
}

TFTEST_MAIN("object-scope helpers + semantic-item entry points")
{
	RdfDoc d;
	TFPASS(d.build());
	const UT_UCS4String t1("obj scoped ");
	TFPASS(d.para(t1));
	{
		PP_PropertyVector bm = {"name", "p1", "type", "start",
								"xml:id", "scopebm"};
		TFPASS(d.doc->appendObject(PTO_Bookmark, bm));
	}
	const UT_UCS4String t2(" tail");
	TFPASS(d.doc->appendSpan(t2.ucs4_str(), t2.length()));
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();

	/* getObjectsInScopeOfTypesForRange finds the bookmark object;
	 * the scan walks back from range.second so it must reach the
	 * object's position */
	{
		std::set<PTObjectType> types = {PTO_Bookmark};
		std::list<const pf_Frag_Object*> objs =
			rdf->getObjectsInScopeOfTypesForRange(types,
				std::make_pair(0, 30));
		TFPASS(!objs.empty());
		TFPASS((*objs.begin())->getObjectType() == PTO_Bookmark);
	}

	/* SPARQL-backed entry points degrade to empty without redland */
	TFPASS(rdf->getAllSemanticObjects().empty());
	TFPASS(rdf->getSemanticObjects(std::set<std::string>{"x"}).empty());
	TFPASS(rdf->getContacts().empty());
	TFPASS(rdf->getEvents().empty());
	TFPASS(rdf->getLocations().empty());

	/* static factory/dialog accessors */
	TFPASS(PD_DocumentRDF::getSemanticItemFactory() != nullptr);
	PD_RDFDialogs *dlgs = PD_DocumentRDF::getRDFDialogs();
	TFPASS(dlgs == dlgs); /* exercise accessor; value is env-set */

	/* getSPARQL_LimitedToXMLIDList builds a filter string */
	{
		std::string q1 = PD_DocumentRDF::getSPARQL_LimitedToXMLIDList(
			std::set<std::string>{"a", "b"});
		TFPASS(q1.find("\"a\"") != std::string::npos);
		TFPASS(q1.find("\"b\"") != std::string::npos);
		TFPASS(q1.find("pkg:idref") != std::string::npos);
		TFPASS(PD_DocumentRDF::getSPARQL_LimitedToXMLIDList(
			std::set<std::string>()).empty());
	}
}
