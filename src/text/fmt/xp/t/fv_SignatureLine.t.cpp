/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

#include "tf_test.h"

#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "fp_Page.h"
#include "pf_Frag_Strux.h"
#include "pp_AttrProp.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <cstring>
#include <string>

#define TFSUITE "core.text.fmt.signatureline"

namespace {

const char *SIG_FIXTURE = "/test/wp/Gettysburg.abw";

/* Widget-less view stack, same as fv_ViewModes — painting goes to a
 * private cairo surface so no display is needed. */
struct HeadlessSigView
{
	HeadlessSigView() = default;
	HeadlessSigView(const HeadlessSigView &) = delete;
	HeadlessSigView &operator=(const HeadlessSigView &) = delete;

	bool load(const char *relPath)
	{
		std::string data_file;
		if (!TF_Test::ensure_test_data(relPath, data_file))
			return false;
		doc = new PD_Document;
		if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr) != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		GR_UnixCairoAllocInfo ai(nullptr);
		graphics = XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		if (!graphics)
			return false;
		layout = new FL_DocLayout(doc, graphics);
		view = new FV_View(XAP_App::getApp(), nullptr, layout);
		layout->fillLayouts();
		layout->formatAll();
		return layout->countPages() > 0;
	}

	~HeadlessSigView()
	{
		delete view;    /* before layout: ~FV_View detaches from it */
		delete layout;
		delete graphics;
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	GR_Graphics *graphics = nullptr;
	FL_DocLayout *layout = nullptr;
	FV_View *view = nullptr;
};

/* locate the (only) signature frame in the document */
const pf_Frag_Strux *findSigFrame(PD_Document *doc, FV_View *view)
{
	PT_DocPosition posEOD = 0;
	view->getEditableBounds(true, posEOD);
	const pf_Frag_Strux *sdh = nullptr;
	if (!doc->getStruxOfTypeFromPosition(posEOD, PTX_SectionFrame, &sdh)
		|| !sdh)
		return nullptr;

	const PP_AttrProp *pAP = nullptr;
	const gchar *v = nullptr;
	if (!doc->getAttrProp(sdh->getIndexAP(), &pAP) || !pAP)
		return nullptr;
	if (!pAP->getProperty("signature-line", v) || !v || strcmp(v, "1"))
		return nullptr;
	return sdh;
}

const PP_AttrProp *sigFrameAP(PD_Document *doc, FV_View *view)
{
	const pf_Frag_Strux *sdh = findSigFrame(doc, view);
	if (!sdh)
		return nullptr;
	const PP_AttrProp *pAP = nullptr;
	doc->getAttrProp(sdh->getIndexAP(), &pAP);
	return pAP;
}

UT_Error exportFile(PD_Document *doc, const char *path, const char *suffix)
{
	GError *err = nullptr;
	GsfOutput *out = gsf_output_stdio_new(path, &err);
	if (!out)
	{
		g_clear_error(&err);
		return UT_ERROR;
	}
	UT_Error rc = doc->saveAs(out, IE_Exp::fileTypeForSuffix(suffix),
							  false, nullptr);
	g_object_unref(out);
	return rc;
}

/* read a member of a zip package into a std::string */
bool readZipMember(const char *zipPath, const char *member, std::string & out)
{
	GError *err = nullptr;
	GsfInput *input = gsf_input_stdio_new(zipPath, &err);
	if (!input)
	{
		g_clear_error(&err);
		return false;
	}
	GsfInfile *zip = gsf_infile_zip_new(input, nullptr);
	g_object_unref(input);
	if (!zip)
		return false;
	/* the infile is a tree of dir children — descend by path parts */
	GsfInput *entry = nullptr;
	{
		gchar **parts = g_strsplit(member, "/", -1);
		GsfInput *cur = GSF_INPUT(zip);
		for (gchar **p = parts; *p && **p; ++p)
		{
			GsfInput *next = gsf_infile_child_by_name(
				GSF_INFILE(cur), *p);
			if (cur != GSF_INPUT(zip))
				g_object_unref(cur);
			cur = next;
			if (!cur)
				break;
		}
		g_strfreev(parts);
		entry = (cur == GSF_INPUT(zip)) ? nullptr : cur;
	}
	bool ok = false;
	if (entry)
	{
		size_t size = gsf_input_size(entry);
		const guint8 *data = gsf_input_read(entry, size, nullptr);
		if (data)
		{
			out.assign(reinterpret_cast<const char *>(data), size);
			ok = true;
		}
		g_object_unref(entry);
	}
	g_object_unref(zip);
	return ok;
}

}

TFTEST_MAIN("signature line inserts an identifiable bordered frame")
{
	HeadlessSigView hv;
	TFPASS(hv.load(SIG_FIXTURE));
	if (!hv.view)
		return;

	FV_SignatureSetup sig;
	sig.sSigner = "Jane Doe";
	sig.sTitle = "Chief Officer";
	sig.sEmail = "jane@example.com";
	sig.sInstructions = "Please sign; on the dotted line";
	sig.bAllowComments = true;
	sig.bShowSignDate = true;
	TFPASS(hv.view->insertSignatureLine(sig));

	const PP_AttrProp *pAP = sigFrameAP(hv.doc, hv.view);
	TFPASS(pAP != nullptr);
	if (!pAP)
		return;

	const gchar *v = nullptr;
	TFPASS(pAP->getProperty("frame-type", v) && v && !strcmp(v, "textbox"));
	v = nullptr;
	TFPASS(pAP->getProperty("signature-id", v) && v && *v);
	v = nullptr;
	TFPASS(pAP->getProperty("signature-name", v) && v
		   && !strcmp(v, "Jane Doe"));
	v = nullptr;
	TFPASS(pAP->getProperty("signature-title", v) && v
		   && !strcmp(v, "Chief Officer"));
	v = nullptr;
	TFPASS(pAP->getProperty("signature-email", v) && v
		   && !strcmp(v, "jane@example.com"));
	v = nullptr;
	/* ';' must not reach the props string — it would split a
	 * phantom property on reload */
	TFPASS(pAP->getProperty("signature-instructions", v) && v
		   && strstr(v, ";") == nullptr && strstr(v, "dotted") != nullptr);
	v = nullptr;
	TFPASS(pAP->getProperty("signature-allow-comments", v) && v
		   && !strcmp(v, "1"));
	v = nullptr;
	TFPASS(pAP->getProperty("signature-show-date", v) && v
		   && !strcmp(v, "1"));
	v = nullptr;
	TFPASS(pAP->getProperty("bot-style", v) && v && !strcmp(v, "solid"));
}

TFTEST_MAIN("signature props survive an abwn save/load round-trip")
{
	HeadlessSigView hv;
	TFPASS(hv.load(SIG_FIXTURE));
	if (!hv.view)
		return;

	FV_SignatureSetup sig;
	sig.sSigner = "Round Trip";
	sig.sTitle = "Vice President";
	sig.sEmail = "rt@example.com";
	sig.sInstructions = "Sign here";
	sig.bAllowComments = false;
	sig.bShowSignDate = false;
	TFPASS(hv.view->insertSignatureLine(sig));

	const gchar *v = nullptr;
	std::string sId;
	const PP_AttrProp *pAP = sigFrameAP(hv.doc, hv.view);
	TFPASS(pAP && pAP->getProperty("signature-id", v) && v && *v);
	if (v)
		sId = v;

	std::string tmp = std::string("/tmp/abn_sig_") +
		std::to_string(::getpid()) + ".abwn";
	TFPASS(exportFile(hv.doc, tmp.c_str(), ".abwn") == UT_OK);

	PD_Document *doc2 = new PD_Document;
	TFPASS(doc2->readFromFile(tmp.c_str(), IEFT_Unknown, nullptr) == UT_OK);

	/* the reloaded doc has no view; find the frame by walking struxes
	 * back from the end of the document */
	pf_Frag *last = doc2->getLastFrag();
	PT_DocPosition posEnd = last ? last->getPos() + 1 : 0;
	const pf_Frag_Strux *sdh = nullptr;
	bool found = false;
	for (PT_DocPosition pos = posEnd; pos > 1 && !found; --pos)
	{
		if (doc2->getStruxOfTypeFromPosition(pos, PTX_SectionFrame, &sdh)
			&& sdh)
		{
			const PP_AttrProp *ap2 = nullptr;
			const gchar *pv = nullptr;
			if (doc2->getAttrProp(sdh->getIndexAP(), &ap2) && ap2
				&& ap2->getProperty("signature-line", pv) && pv
				&& !strcmp(pv, "1"))
				found = true;
			else
				break; /* nearest frame is not ours — give up */
		}
	}
	TFPASS(found && sdh);
	if (found && sdh)
	{
		const PP_AttrProp *ap2 = nullptr;
		TFPASS(doc2->getAttrProp(sdh->getIndexAP(), &ap2) && ap2);
		const gchar *pv = nullptr;
		TFPASS(ap2->getProperty("signature-id", pv) && pv
			   && sId == pv);
		pv = nullptr;
		TFPASS(ap2->getProperty("signature-name", pv) && pv
			   && !strcmp(pv, "Round Trip"));
		pv = nullptr;
		TFPASS(ap2->getProperty("signature-email", pv) && pv
			   && !strcmp(pv, "rt@example.com"));
		pv = nullptr;
		TFPASS(ap2->getProperty("signature-instructions", pv) && pv
			   && !strcmp(pv, "Sign here"));
		pv = nullptr;
		TFPASS(ap2->getProperty("signature-allow-comments", pv) && pv
			   && !strcmp(pv, "0"));
		pv = nullptr;
		TFPASS(ap2->getProperty("signature-show-date", pv) && pv
			   && !strcmp(pv, "0"));
	}
	doc2->unref();
	unlink(tmp.c_str());
}

TFTEST_MAIN("docx export maps the frame to o:signatureline")
{
	HeadlessSigView hv;
	TFPASS(hv.load(SIG_FIXTURE));
	if (!hv.view)
		return;

	FV_SignatureSetup sig;
	sig.sSigner = "Export & Co";
	sig.sTitle = "Director";
	sig.sEmail = "export@example.com";
	sig.sInstructions = "Sign <here> please";
	sig.bAllowComments = true;
	sig.bShowSignDate = true;
	TFPASS(hv.view->insertSignatureLine(sig));

	std::string tmp = std::string("/tmp/abn_sig_") +
		std::to_string(::getpid()) + ".docx";
	TFPASS(exportFile(hv.doc, tmp.c_str(), ".docx") == UT_OK);

	std::string documentXml;
	TFPASS(readZipMember(tmp.c_str(), "word/document.xml", documentXml));
	unlink(tmp.c_str());
	TFPASS(documentXml.find("<o:signatureline") != std::string::npos);
	TFPASS(documentXml.find("o:issignatureline=\"t\"") != std::string::npos);
	TFPASS(documentXml.find("o:suggestedsigner=\"Export &amp; Co\"")
		   != std::string::npos);
	TFPASS(documentXml.find("o:suggestedsigner2=\"Director\"")
		   != std::string::npos);
	TFPASS(documentXml.find("o:suggestedsigneremail=\"export@example.com\"")
		   != std::string::npos);
	TFPASS(documentXml.find("o:signinginstructions=\"Sign &lt;here&gt; please\"")
		   != std::string::npos);
	TFPASS(documentXml.find("o:showsigndate=\"t\"") != std::string::npos);
	/* the block content is still inside the textbox */
	TFPASS(documentXml.find("<w:txbxContent>") != std::string::npos);
	TFPASS(documentXml.find("Director") != std::string::npos);
}
