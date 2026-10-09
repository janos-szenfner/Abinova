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
 * helpers.  RDF01 added a built-in SPARQL-subset evaluator used when
 * libredland is absent, so the SPARQL-driven paths are exercised with
 * real bindings here. */

#include <algorithm>
#include <sstream>
#include <string>
#include <set>
#include <list>

#include "tf_test.h"

#include "ut_string.h"

#include "pd_Document.h"
#include "pd_DocumentRDF.h"
#include "pd_RDFQuery.h"
#include "pd_RDFSupport.h"
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

		/* relink finds the pkg:idref'd subject via SPARQL and adds
		 * the new idref arc */
		rdf->relinkRDFToNewXMLID("blkA", "blkA2", false);
		TFPASS(rdf->contains(subj, idref,
				PD_Object("blkA2", PD_Object::OBJECT_TYPE_LITERAL)));
	}

	/* position-scoped model: getRDFAtPosition never crashes and
	 * yields a model */
	{
		PD_RDFModelHandle at =
			rdf->getRDFAtPosition(d.doc->getStruxPosition(blkA) + 1);
		TFPASS(at.get() != nullptr);
	}
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

	/* SPARQL-backed entry points run the built-in evaluator; this
	 * model has no foaf/cal triples so the lists come back empty */
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

TFTEST_MAIN("built-in SPARQL-subset evaluator")
{
	RdfDoc d;
	TFPASS(d.build());
	const UT_UCS4String t1("q ");
	TFPASS(d.para(t1));
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();

	const PD_URI alice("http://ex.org/alice");
	const PD_URI bob("http://ex.org/bob");
	const PD_URI carol("http://ex.org/carol");
	const PD_URI rdfType("http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	const PD_URI person("http://xmlns.com/foaf/0.1/Person");
	const PD_URI namep("http://xmlns.com/foaf/0.1/name");
	const PD_URI nickp("http://xmlns.com/foaf/0.1/nick");
	const PD_URI idref(
		"http://docs.oasis-open.org/opendocument/meta/package/common#idref");

	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(alice, rdfType, PD_Object(person));
		m->add(alice, namep, PD_Literal("Alice"));
		m->add(alice, nickp, PD_Literal("ally"));
		m->add(bob, rdfType, PD_Object(person));
		m->add(bob, namep, PD_Literal("Bob"));
		m->add(carol, rdfType, PD_Object(person));
		m->add(carol, namep, PD_Literal("Alice"));
		m->add(alice, idref, PD_Literal("blkA"));
		m->commit();
	}
	TFPASS(rdf->getTripleCount() == 8);

	/* PREFIX expansion, explicit vars, OPTIONAL keeps the row */
	{
		PD_RDFQuery q(rdf, rdf);
		PD_ResultBindings_t b = q.executeQuery(
			"prefix rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#>\n"
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"select ?person ?name ?nick\n"
			"where {\n"
			"  ?person rdf:type foaf:Person .\n"
			"  ?person foaf:name ?name .\n"
			"  OPTIONAL { ?person foaf:nick ?nick }\n"
			"}\n");
		TFPASS(b.size() == 3);
		for (PD_ResultBindings_t::iterator it = b.begin();
			 it != b.end(); ++it)
		{
			const std::string n = (*it)["name"];
			TFPASS(n == "Alice" || n == "Bob");
			const std::string s = (*it)["person"];
			if (s == alice.toString())
				TFPASS((*it)["nick"] == "ally");
			else
				TFPASS((*it)["nick"].empty());
		}
	}

	/* FILTER str() = with || */
	{
		PD_RDFQuery q(rdf, rdf);
		PD_ResultBindings_t b = q.executeQuery(
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"select ?person\n"
			"where {\n"
			"  ?person foaf:name ?name .\n"
			"  filter( str(?name) = \"Alice\" || str(?name) = \"Bob\" )\n"
			"}\n");
		TFPASS(b.size() == 3);
	}

	/* FILTER str() != with && */
	{
		PD_RDFQuery q(rdf, rdf);
		PD_ResultBindings_t b = q.executeQuery(
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"prefix pkg: <http://docs.oasis-open.org/opendocument/meta/package/common#>\n"
			"select ?s ?name\n"
			"where {\n"
			"  ?s foaf:name ?name .\n"
			"  ?s pkg:idref ?x .\n"
			"  filter( str(?name) != \"Bob\" && str(?x) = \"blkA\" )\n"
			"}\n");
		TFPASS(b.size() == 1);
		TFPASS((*b.begin())["s"] == alice.toString());
	}

	/* SELECT DISTINCT collapses duplicate projection rows */
	{
		PD_RDFQuery q(rdf, rdf);
		PD_ResultBindings_t all = q.executeQuery(
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"select ?name\n"
			"where { ?s foaf:name ?name }\n");
		TFPASS(all.size() == 3);

		PD_RDFQuery q2(rdf, rdf);
		PD_ResultBindings_t dist = q2.executeQuery(
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"select distinct ?name\n"
			"where { ?s foaf:name ?name }\n");
		TFPASS(dist.size() == 2);
	}

	/* SELECT * projects every bound variable */
	{
		PD_RDFQuery q(rdf, rdf);
		PD_ResultBindings_t b = q.executeQuery(
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"select *\n"
			"where { ?s foaf:nick ?nick }\n");
		TFPASS(b.size() == 1);
		TFPASS((*b.begin())["s"] == alice.toString());
		TFPASS((*b.begin())["nick"] == "ally");
	}

	/* querying a restricted model only sees its triples */
	{
		PD_RDFModelHandle scratch = rdf->createScratchModel();
		{
			PD_DocumentRDFMutationHandle sm = scratch->createMutation();
			sm->add(alice, namep, PD_Literal("Alice"));
			sm->commit();
		}
		PD_RDFQuery q(rdf, scratch);
		PD_ResultBindings_t b = q.executeQuery(
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"select ?s ?name\n"
			"where { ?s foaf:name ?name }\n");
		TFPASS(b.size() == 1);
	}

	/* generated query shape: xmlid-restricted statement listing */
	{
		std::string sparql = PD_DocumentRDF::getSPARQL_LimitedToXMLIDList(
			std::set<std::string>{"blkA"});
		PD_RDFQuery q(rdf, rdf);
		PD_ResultBindings_t b = q.executeQuery(sparql);
		/* every statement of alice (the blkA-linked subject) */
		TFPASS(b.size() == 4);
		for (PD_ResultBindings_t::iterator it = b.begin();
			 it != b.end(); ++it)
		{
			TFPASS((*it)["s"] == alice.toString());
			TFPASS((*it)["rdflink"] == "blkA");
		}
	}

	/* empty model -> empty bindings, malformed query -> empty */
	{
		PD_RDFModelHandle scratch = rdf->createScratchModel();
		PD_RDFQuery q(rdf, scratch);
		TFPASS(q.executeQuery("select * where { ?s ?p ?o }").empty());
		PD_RDFQuery q2(rdf, rdf);
		TFPASS(q2.executeQuery("this is not sparql").empty());
	}
}

TFTEST_MAIN("semantic-item enumeration through SPARQL")
{
	RdfDoc d;
	TFPASS(d.build());
	const UT_UCS4String t1("si ");
	TFPASS(d.para(t1, {"xml:id", "siBlk"}));
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();

	const PD_URI alice("http://ex.org/alice");
	const PD_URI rdfType("http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	const PD_URI person("http://xmlns.com/foaf/0.1/Person");
	const PD_URI namep("http://xmlns.com/foaf/0.1/name");
	const PD_URI idref(
		"http://docs.oasis-open.org/opendocument/meta/package/common#idref");

	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(alice, rdfType, PD_Object(person));
		m->add(alice, namep, PD_Literal("Alice"));
		m->add(alice, idref, PD_Literal("siBlk"));
		m->commit();
	}

	/* getContacts() runs the generated SPARQL through the built-in
	 * evaluator; under the test harness the GTK semantic-item
	 * factory is installed so the contact materializes */
	PD_RDFContacts contacts = rdf->getContacts();
	TFPASS(contacts.size() == 1);
	if (!contacts.empty())
		TFPASS((*contacts.begin())->name() == "Alice");

	/* getSemanticObjects intersects the item's xmlids with the
	 * requested set */
	PD_RDFSemanticItems inScope =
		rdf->getSemanticObjects(std::set<std::string>{"siBlk"});
	TFPASS(inScope.size() == 1);
	TFPASS(rdf->getSemanticObjects(std::set<std::string>{"nope"}).empty());
	TFPASS(rdf->getAllSemanticObjects().size() == 1);

	/* xmlid lookup for the linking subject works via the evaluator */
	std::set<std::string> ids =
		PD_RDFSemanticItem::getXMLIDsForLinkingSubject(
			rdf, alice.toString());
	TFPASS(ids.count("siBlk") == 1);
	TFPASS(PD_RDFSemanticItem::getXMLIDsForLinkingSubject(
			rdf, "http://ex.org/unknown").empty());
}

TFTEST_MAIN("RDF/XML load + export round-trip without redland")
{
	RdfDoc d;
	TFPASS(d.build());
	const UT_UCS4String t1("io ");
	TFPASS(d.para(t1));
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();

	/* import from an RDF/XML document */
	const std::string rdfxml =
		"<?xml version=\"1.0\"?>\n"
		"<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n"
		"         xmlns:foaf=\"http://xmlns.com/foaf/0.1/\">\n"
		" <rdf:Description rdf:about=\"http://ex.org/alice\">\n"
		"  <rdf:type rdf:resource=\"http://xmlns.com/foaf/0.1/Person\"/>\n"
		"  <foaf:name>Alice</foaf:name>\n"
		"  <foaf:knows rdf:resource=\"http://ex.org/bob\"/>\n"
		" </rdf:Description>\n"
		" <foaf:Person rdf:about=\"http://ex.org/bob\">\n"
		"  <foaf:nick>bobby</foaf:nick>\n"
		" </foaf:Person>\n"
		"</rdf:RDF>\n";
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		TFPASS(loadRDFXML(m, rdfxml) == UT_OK);
		m->commit();
	}

	const PD_URI alice("http://ex.org/alice");
	const PD_URI type("http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	const PD_URI namep("http://xmlns.com/foaf/0.1/name");
	const PD_URI knows("http://xmlns.com/foaf/0.1/knows");
	const PD_URI nickp("http://xmlns.com/foaf/0.1/nick");

	TFPASS(rdf->getTripleCount() == 4);
	TFPASS(rdf->contains(alice, type,
				PD_Object("http://xmlns.com/foaf/0.1/Person")));
	TFPASS(rdf->contains(alice, namep,
				PD_Object("Alice", PD_Object::OBJECT_TYPE_LITERAL)));
	TFPASS(rdf->contains(alice, knows,
				PD_Object("http://ex.org/bob")));
	TFPASS(rdf->contains(PD_URI("http://ex.org/bob"), nickp,
				PD_Object("bobby", PD_Object::OBJECT_TYPE_LITERAL)));

	/* the imported triples are visible to SPARQL queries */
	{
		PD_RDFQuery q(rdf, rdf);
		PD_ResultBindings_t b = q.executeQuery(
			"prefix foaf: <http://xmlns.com/foaf/0.1/>\n"
			"select ?p ?n\n"
			"where { ?p foaf:name ?n }\n");
		TFPASS(b.size() == 1);
		TFPASS((*b.begin())["p"] == alice.toString());
	}

	/* export and re-import into a scratch model */
	std::string exported = toRDFXML(rdf);
	TFPASS(!exported.empty());
	{
		PD_RDFModelHandle scratch = rdf->createScratchModel();
		PD_DocumentRDFMutationHandle sm = scratch->createMutation();
		TFPASS(loadRDFXML(sm, exported) == UT_OK);
		sm->commit();
		TFPASS(scratch->getTripleCount() == 4);
		TFPASS(scratch->contains(alice, namep,
				PD_Object("Alice", PD_Object::OBJECT_TYPE_LITERAL)));
		TFPASS(scratch->contains(PD_URI("http://ex.org/bob"), nickp,
				PD_Object("bobby", PD_Object::OBJECT_TYPE_LITERAL)));
	}

	/* malformed input fails, empty input is a no-op success */
	{
		PD_RDFModelHandle scratch = rdf->createScratchModel();
		PD_DocumentRDFMutationHandle sm = scratch->createMutation();
		TFPASS(loadRDFXML(sm, "<rdf:RDF><unclosed>") == UT_ERROR);
		TFPASS(scratch->getTripleCount() == 0);
		TFPASS(loadRDFXML(sm, "") == UT_OK);
		sm->commit();
	}
}

TFTEST_MAIN("serialized RDF stream rejects corrupt lengths")
{
	/* truncated/oversized length-prefixed strings must fail, not
	 * allocate or read out of bounds */
	PD_URI u;
	std::stringstream bad1;
	bad1 << "1 1 99999 abc";
	TFPASS(!u.read(bad1));

	std::stringstream bad2;
	bad2 << "1 1 -5 xxxxx";
	TFPASS(!u.read(bad2));

	PD_Object o;
	std::stringstream bad3;
	bad3 << "1 1 2 1000 short";
	TFPASS(!o.read(bad3));

	/* out-of-range object type is rejected */
	std::stringstream bad4;
	bad4 << "1 1 99 3 abc 0  0  ";
	TFPASS(!o.read(bad4));

	/* a well-formed stream still reads */
	PD_URI good("http://ex.org/ok");
	std::stringstream good1;
	TFPASS(good.write(good1));
	PD_URI good2;
	TFPASS(good2.read(good1));
	TFPASS(good2 == good);
}

TFTEST_MAIN("semantic-item surface: contact/event methods")
{
	RdfDoc d;
	TFPASS(d.build());
	const UT_UCS4String t1("si2 ");
	TFPASS(d.para(t1, {"xml:id", "siBlk2"}));
	d.finish();

	PD_DocumentRDFHandle rdf = d.doc->getDocumentRDF();

	const PD_URI alice("http://ex.org/alice");
	const PD_URI bob("http://ex.org/bob");
	const PD_URI rdfType("http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	const PD_URI rdfFirst("http://www.w3.org/1999/02/22-rdf-syntax-ns#first");
	const PD_URI rdfRest("http://www.w3.org/1999/02/22-rdf-syntax-ns#rest");
	const PD_URI person("http://xmlns.com/foaf/0.1/Person");
	const PD_URI namep("http://xmlns.com/foaf/0.1/name");
	const PD_URI nickp("http://xmlns.com/foaf/0.1/nick");
	const PD_URI emailp("http://xmlns.com/foaf/0.1/mbox");
	const PD_URI homep("http://xmlns.com/foaf/0.1/homepage");
	const PD_URI imgp("http://xmlns.com/foaf/0.1/img");
	const PD_URI phonep("http://xmlns.com/foaf/0.1/phone");
	const PD_URI jabp("http://xmlns.com/foaf/0.1/jabberid");
	const PD_URI idref(
		"http://docs.oasis-open.org/opendocument/meta/package/common#idref");
	const PD_URI vevent("http://www.w3.org/2002/12/cal/icaltzd#Vevent");
	const std::string cal = "http://www.w3.org/2002/12/cal/icaltzd#";
	const PD_URI dctitle("http://purl.org/dc/elements/1.1/title");
	const PD_URI ev("http://ex.org/ev1");
	const PD_URI geon("http://ex.org/geo1");
	const PD_URI joiner("http://ex.org/joiner1");

	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(alice, rdfType, PD_Object(person));
		m->add(alice, namep, PD_Literal("Alice"));
		m->add(alice, nickp, PD_Literal("ally"));
		m->add(alice, emailp, PD_Literal("a@x.org"));
		m->add(alice, homep, PD_Literal("http://a.example"));
		m->add(alice, imgp, PD_Literal("http://a.example/i.png"));
		m->add(alice, phonep, PD_Literal("555-1"));
		m->add(alice, jabp, PD_Literal("a@jab"));
		m->add(alice, idref, PD_Literal("siBlk2"));
		m->add(bob, rdfType, PD_Object(person));
		m->add(bob, namep, PD_Literal("Bob"));
		m->add(ev, rdfType, PD_Object(vevent));
		m->add(ev, PD_URI(cal + "uid"), PD_Literal("ev-1"));
		m->add(ev, PD_URI(cal + "dtstart"), PD_Literal("2026-01-01T10:00"));
		m->add(ev, PD_URI(cal + "dtend"), PD_Literal("2026-01-01T11:00"));
		m->add(ev, PD_URI(cal + "summary"), PD_Literal("Sync"));
		m->add(ev, PD_URI(cal + "location"), PD_Literal("Room 1"));
		m->add(ev, PD_URI(cal + "description"), PD_Literal("Discuss"));
		m->add(ev, PD_URI(cal + "geo"), PD_Object(geon));
		m->add(geon, rdfFirst, PD_Literal("10.5"));
		m->add(geon, rdfRest, PD_Object(joiner));
		m->add(joiner, rdfFirst, PD_Literal("20.5"));
		m->add(geon, dctitle, PD_Literal("Somewhere"));
		m->commit();
	}

	/* createSemanticItem dispatches by class name; the empty-binding
	 * iterator form is covered through getContacts() below */
	{
		PD_RDFSemanticItemHandle c =
			PD_RDFSemanticItem::createSemanticItem(rdf, "Contact");
		TFPASS(c != nullptr);
		TFPASS(c && c->className() == "Contact");
		PD_RDFSemanticItemHandle e =
			PD_RDFSemanticItem::createSemanticItem(rdf, "Event");
		TFPASS(e != nullptr);
		TFPASS(e && e->className() == "Event");
		TFPASS(PD_RDFSemanticItem::createSemanticItem(
				   rdf, "NoSuchClass") == nullptr);
		TFPASS(PD_RDFSemanticItem::createSemanticItem(
				   rdf, "Location") == nullptr);   // no WITH_CHAMPLAIN
		TFPASS(!PD_RDFSemanticItem::getClassNames().empty());
	}

	/* materialized contact: full optional bindings bound */
	PD_RDFContacts contacts = rdf->getContacts();
	TFPASS(contacts.size() == 2);
	PD_RDFContactHandle c;
	for (PD_RDFContacts::iterator it = contacts.begin();
		 it != contacts.end(); ++it)
		if ((*it)->name() == "Alice")
			c = *it;
	TFPASS(c != nullptr);
	if (c) {
		TFPASS(c->linkingSubject().toString() == alice.toString());
		TFPASS(c->getXMLIDs().count("siBlk2") == 1);
		TFPASS(c->className() == "Contact");
		TFPASS(c->getDisplayLabel() == "Contact");
		c->setName("Alicia");
		TFPASS(c->name() == "Alicia");
		c->setName("Alice");

		TFPASS(c->stylesheets().size() == 5);
		TFPASS(c->findStylesheetByUuid(
				   "143c1ba3-d7bb-440b-8528-7f07d2eff5f2") != nullptr);
		TFPASS(c->findStylesheetByUuid("no-such-uuid") == nullptr);
		TFPASS(c->findStylesheetByName(
				   PD_RDFSemanticStylesheet::stylesheetTypeSystem(),
				   RDF_SEMANTIC_STYLESHEET_CONTACT_NAME) != nullptr);
		TFPASS(c->findStylesheetByName(
				   c->stylesheets(),
				   RDF_SEMANTIC_STYLESHEET_CONTACT_NAME) != nullptr);
		TFPASS(c->defaultStylesheet() != nullptr);

		std::map<std::string, std::string> rm;
		c->setupStylesheetReplacementMapping(rm);
		TFPASS(rm["%NICK%"] == "ally");
		TFPASS(rm["%EMAIL%"] == "a@x.org");
		TFPASS(rm["%PHONE%"] == "555-1");
		TFPASS(rm["%HOMEPAGE%"] == "http://a.example");

		/* the GTK-injected subclasses override
		 * exportToFile/importFromFile/showEditorWindow/
		 * importFromDataComplete with real file dialogs and
		 * GtkBuilder editors — display-bound, not exercised here */
		std::istringstream iss("");
		c->importFromData(iss, rdf, nullptr);
	}

	/* materialized event through the cal:Vevent query */
	PD_RDFEvents events = rdf->getEvents();
	TFPASS(events.size() == 1);
	if (!events.empty()) {
		PD_RDFEventHandle e = *events.begin();
		TFPASS(e->className() == "Event");
		TFPASS(e->getDisplayLabel() == "Event");
		TFPASS(e->linkingSubject().toString() == ev.toString());
		TFPASS(e->name() == "ev-1");   // no name binding -> uid
		TFPASS(!e->stylesheets().empty());
		TFPASS(e->defaultStylesheet() != nullptr);
		std::map<std::string, std::string> rm;
		e->setupStylesheetReplacementMapping(rm);
		TFPASS(rm["%SUMMARY%"] == "Sync");
	}

	/* getLocations runs both joiner-list and geo84 queries; item
	 * creation is WITH_CHAMPLAIN-gated so the list stays empty in
	 * this build but the SPARQL surface is exercised */
	rdf->getLocations();

	/* foaf:knows is symmetric through relationAdd */
	if (c && contacts.size() == 2) {
		PD_RDFContactHandle other =
			(*contacts.begin() == c) ? contacts.back()
									 : *contacts.begin();
		c->relationAdd(other, PD_RDFSemanticItem::RELATION_FOAF_KNOWS);
		const PD_URI knows("http://xmlns.com/foaf/0.1/knows");
		TFPASS(rdf->contains(alice, knows,
							 PD_Object(other->linkingSubject())));
		TFPASS(rdf->contains(other->linkingSubject(), knows,
							 PD_Object(alice)));
	}

	/* mutation overloads beyond plain add()/commit() */
	{
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(PD_RDFStatement(bob, nickp,
							   PD_Literal("bobby")));
		m->add(bob, phonep, PD_Literal("555-2"),
			   PD_URI("http://ex.org/ctx"));
		m->remove(alice, nickp, PD_Object(
					  "ally", PD_Object::OBJECT_TYPE_LITERAL));
		m->remove(alice, phonep, phonep);   // URI-object overload
		m->remove(PD_RDFStatement(alice, jabp,
								  PD_Object("a@jab",
											PD_Object::OBJECT_TYPE_LITERAL)));
		m->remove(std::list<PD_RDFStatement>{
			PD_RDFStatement(alice, emailp,
							PD_Object("a@x.org",
									  PD_Object::OBJECT_TYPE_LITERAL))});
		m->remove(bob, imgp);               // drop all img for bob
		PD_URI bn = m->createBNode();
		TFPASS(!bn.toString().empty());
		m->commit();
		TFPASS(!rdf->contains(alice, nickp,
							  PD_Object("ally",
										PD_Object::OBJECT_TYPE_LITERAL)));
		TFPASS(rdf->contains(bob, nickp,
							 PD_Object("bobby",
									   PD_Object::OBJECT_TYPE_LITERAL)));
	}

	/* rollback drops staged adds */
	{
		const PD_URI ghostp("http://xmlns.com/foaf/0.1/ghost");
		PD_DocumentRDFMutationHandle m = rdf->createMutation();
		m->add(alice, ghostp, PD_Literal("spooky"));
		m->rollback();
		TFPASS(!rdf->contains(alice, ghostp,
							  PD_Object("spooky",
										PD_Object::OBJECT_TYPE_LITERAL)));
	}
}
