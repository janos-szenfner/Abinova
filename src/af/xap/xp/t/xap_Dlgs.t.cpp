/* AbiSource Application Framework
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

#include <string.h>

#include "tf_test.h"

#include "xap_App.h"
#include "xap_Dialog.h"
#include "xap_Dialog_Id.h"
#include "xap_DialogFactory.h"
#include "xap_Dlg_FontChooser.h"
#include "xap_Dlg_Encoding.h"
#include "xap_Dlg_Zoom.h"
#include "xap_Dlg_MessageBox.h"
#include "xap_Dlg_ClipArt.h"
#include "xap_Dlg_ListDocuments.h"
#include "xap_Dlg_FileOpenSaveAs.h"
#include "xap_Dlg_Password.h"
#include "xap_Dlg_PrintPreview.h"
#include "xap_Dlg_WindowMore.h"
#include "xap_Dlg_HTMLOptions.h"
#include "xap_Frame.h"
#include "xap_Prefs.h"
#include "xap_Prefs_SchemeIds.h"
#include "ut_string.h"
#include "ut_string_class.h"

#include "../../../../wp/impexp/xp/ie_types.h"

#define TFSUITE "core.af.xap.dlgs"

static XAP_DialogFactory *dlgFactory()
{
	return XAP_App::getApp()->getDialogFactory();
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog base class (through a trivial concrete subclass)         */
/* ------------------------------------------------------------------ */

namespace {

class TestDialog : public XAP_Dialog
{
public:
	TestDialog(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog(f, id, "test/help") {}
	void runModal(XAP_Frame *) override { m_ran = true; }
	bool m_ran = false;

	void exerciseWidgetHelpers()
	{
		/* the base getWidget() returns nullptr -> all the widget
		 * helpers are no-ops / default returns */
		m_widgetNull = (getWidget(42) == nullptr);
		m_intVal = getWidgetValueInt(1);
		setWidgetValueInt(1, 5);
		setWidgetLabel(1, UT_UTF8String("lbl"));
		setWidgetLabel(1, std::string("lbl2"));
	}
	bool m_widgetNull = false;
	int m_intVal = 99;
};

class TestNonPersistent : public XAP_Dialog_NonPersistent
{
public:
	TestNonPersistent(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_NonPersistent(f, id, "test/np") {}
	void runModal(XAP_Frame *) override {}
};

class TestTabbed : public XAP_TabbedDialog_NonPersistent
{
public:
	TestTabbed(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_TabbedDialog_NonPersistent(f, id, "test/tab") {}
	void runModal(XAP_Frame *) override {}
};

class TestPersistent : public XAP_Dialog_Persistent
{
public:
	TestPersistent(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_Persistent(f, id, "test/p") {}
	void runModal(XAP_Frame *) override {}
};

class TestFramePersistent : public XAP_Dialog_FramePersistent
{
public:
	TestFramePersistent(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_FramePersistent(f, id, "test/fp") {}
	void runModal(XAP_Frame *) override {}
};

class TestAppPersistent : public XAP_Dialog_AppPersistent
{
public:
	TestAppPersistent(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_AppPersistent(f, id, "test/ap") {}
	void runModal(XAP_Frame *) override {}
};

class TestModeless : public XAP_Dialog_Modeless
{
public:
	TestModeless(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_Modeless(f, id, "test/ml") {}
	void runModal(XAP_Frame *) override {}
	void runModeless(XAP_Frame *) override {}
	void destroy(void) override { m_destroyed = true; }
	void activate(void) override { m_activated = true; }
	bool m_destroyed = false;
	bool m_activated = false;
};

} // namespace

TFTEST_MAIN("XAP_Dialog base")
{
	TestDialog dlg(dlgFactory(), XAP_DIALOG_ID_MESSAGE_BOX);

	TFPASS(dlg.getApp() == XAP_App::getApp());
	TFPASS(dlg.getDialogId() == XAP_DIALOG_ID_MESSAGE_BOX);
	TFPASS(dlg.getHelpUrl() == "test/help");

	dlg.exerciseWidgetHelpers();
	TFPASS(dlg.m_widgetNull);
	TFPASS(dlg.m_intVal == 0);

	/* virtual defaults are harmless no-ops */
	dlg.maybeClosePopupPreviewBubbles();
	dlg.maybeReallowPopupPreviewBubbles();

	dlg.runModal(nullptr);
	TFPASS(dlg.m_ran);
}

TFTEST_MAIN("XAP_Dialog persistence classes")
{
	/* persistence flavour constants */
	TFPASS(XAP_Dialog_NonPersistent::s_getPersistence() == XAP_DLGT_NON_PERSISTENT);
	TFPASS(XAP_Dialog_FramePersistent::s_getPersistence() == XAP_DLGT_FRAME_PERSISTENT);
	TFPASS(XAP_Dialog_AppPersistent::s_getPersistence() == XAP_DLGT_APP_PERSISTENT);
	/* modeless dialogs are tracked app-wide like app-persistent ones */
	TFPASS(XAP_Dialog_Modeless::s_getPersistence() == XAP_DLGT_APP_PERSISTENT);

	{
		TestNonPersistent d(dlgFactory(), XAP_DIALOG_ID_ZOOM);
		TFPASS(d.getHelpUrl() == "test/np");
	}
	{
		TestTabbed d(dlgFactory(), XAP_DIALOG_ID_ZOOM);
		d.setInitialPageId("page42");
		TFPASS(d.getInitialPageId() == "page42");
	}
	{
		TestPersistent d(dlgFactory(), XAP_DIALOG_ID_ZOOM);
		d.useStart();
		d.useEnd();
	}
	{
		TestFramePersistent d(dlgFactory(), XAP_DIALOG_ID_ZOOM);
		d.useStart();
		d.useEnd();
	}
	{
		TestAppPersistent d(dlgFactory(), XAP_DIALOG_ID_ZOOM);
		d.useStart();
		d.useEnd();
	}
	{
		TestModeless d(dlgFactory(), XAP_DIALOG_ID_ZOOM);
		d.useStart();
		d.useEnd();
		TFPASS(!d.isRunning());
		/* no frames in the test app -> nullptr */
		TFPASS(d.getActiveFrame() == XAP_App::getApp()->getLastFocussedFrame()
			   || d.getActiveFrame() == nullptr);
	}
}

/* ------------------------------------------------------------------ */
/* XAP_DialogFactory                                                   */
/* ------------------------------------------------------------------ */

static XAP_Dialog *testDlgCtor(XAP_DialogFactory * f, XAP_Dialog_Id id)
{
	return new TestNonPersistent(f, id);
}

TFTEST_MAIN("XAP_DialogFactory")
{
	XAP_DialogFactory *f = dlgFactory();
	TFPASS(f != nullptr);
	TFPASS(f->getApp() == XAP_App::getApp());

	UT_uint32 tableSize = f->getDialogTableSize();
	TFPASS(tableSize > 0);

	/* static table entries are readable */
	TFPASS(f->getDialogTableEntry(0) != nullptr);
	TFPASS(f->getDialogTableEntry(tableSize - 1) != nullptr);
	TFPASS(f->getDialogTableEntry(tableSize) == nullptr);

	/* registering a dynamic dialog grows the table and yields a
	 * fresh id */
	XAP_Dialog_Id id = f->registerDialog(testDlgCtor, XAP_DLGT_NON_PERSISTENT);
	TFPASS(f->getDialogTableSize() == tableSize + 1);
	TFPASS(static_cast<UT_sint32>(id) >= 0);

	/* justMakeTheDialog builds a fresh instance */
	XAP_Dialog *d = f->justMakeTheDialog(id);
	TFPASS(d != nullptr);
	TFPASS(d->getDialogId() == id);
	delete d;

	/* unknown id -> nullptr */
	TFPASS(f->justMakeTheDialog(static_cast<XAP_Dialog_Id>(9876)) == nullptr);

	/* registering an app-persistent dialog takes the "remember me"
	 * path through requestDialog */
	auto persistentCtor = [](XAP_DialogFactory * pf, XAP_Dialog_Id pid) -> XAP_Dialog * {
		return new TestAppPersistent(pf, pid);
	};
	XAP_Dialog_Id pid = f->registerDialog(persistentCtor,
										XAP_DLGT_APP_PERSISTENT);
	XAP_Dialog *req = f->requestDialog(pid);
	TFPASS(req != nullptr);
	/* requesting again returns the same tracked instance */
	TFPASS(f->requestDialog(pid) == req);

	/* unregistering forgets the live instance (a fresh request
	 * must still succeed; the allocator may legitimately reuse
	 * the freed address, so no pointer-inequality check) */
	f->unregisterDialog(pid);
	XAP_Dialog *req2 = f->requestDialog(pid);
	TFPASS(req2 != nullptr);

	f->releaseDialog(req2);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_FontChooser                                              */
/* ------------------------------------------------------------------ */

namespace {
class TestFontChooser : public XAP_Dialog_FontChooser
{
public:
	TestFontChooser(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_FontChooser(f, id) {}
	void runModal(XAP_Frame *) override {}
};
}

TFTEST_MAIN("XAP_Dialog_FontChooser")
{
	TestFontChooser dlg(dlgFactory(), XAP_DIALOG_ID_FONT);

	TFPASS(dlg.getAnswer() == XAP_Dialog_FontChooser::a_CANCEL);
	TFPASS(dlg.getDrawString() != nullptr);

	std::string s;
	bool b = false;

	/* nothing changed yet */
	TFPASS(!dlg.getChangedFontFamily(s));
	TFPASS(!dlg.getChangedFontSize(s));
	TFPASS(!dlg.getChangedFontWeight(s));
	TFPASS(!dlg.getChangedFontStyle(s));
	TFPASS(!dlg.getChangedColor(s));
	TFPASS(!dlg.getChangedBGColor(s));
	TFPASS(!dlg.getChangedTextTransform(s));
	TFPASS(!dlg.getChangedUnderline(&b));
	TFPASS(!dlg.getChangedHidden(&b));

	dlg.setFontFamily("Serif");
	dlg.setFontSize("24pt");
	dlg.setFontWeight("bold");
	dlg.setFontStyle("italic");
	dlg.setColor("112233");
	dlg.setBGColor("ddeeff");
	dlg.setBackGroundColor("aabbcc");
	dlg.setTextTransform("capitalize");

	/* the setters update both the baseline member and the prop
	 * map, so getChanged* reports "no change" and hands back the
	 * baseline value */
	TFPASS(!dlg.getChangedFontFamily(s) && s == "Serif");
	TFPASS(!dlg.getChangedFontSize(s) && s == "24pt");
	TFPASS(!dlg.getChangedFontWeight(s) && s == "bold");
	TFPASS(!dlg.getChangedFontStyle(s) && s == "italic");
	TFPASS(!dlg.getChangedColor(s) && s == "112233");
	TFPASS(!dlg.getChangedBGColor(s) && s == "ddeeff");
	TFPASS(!dlg.getChangedTextTransform(s) && s == "capitalize");

	/* writing a different value straight into the prop map is
	 * seen as a change relative to the baseline */
	dlg.addOrReplaceVecProp("font-family", "Mono");
	TFPASS(dlg.getChangedFontFamily(s) && s == "Mono");

	/* the property map accumulated the values */
	TFPASS(dlg.getVal("font-family") == "Mono");
	TFPASS(dlg.getVal("font-size") == "24pt");
	TFPASS(dlg.getVal("font-weight") == "bold");
	TFPASS(dlg.getVal("font-style") == "italic");
	TFPASS(dlg.getVal("color") == "112233");
	TFPASS(dlg.getVal("bgcolor") == "ddeeff");
	TFPASS(dlg.getVal("text-transform") == "capitalize");

	/* decorations: the getChanged* flags are output-only (set by
	 * the platform dialog), but the out-param carries the state */
	dlg.setFontDecoration(true, false, false, false, false);
	TFPASS(!dlg.getChangedUnderline(&b) && b);
	TFPASS(!dlg.getChangedOverline(&b) && !b);
	TFPASS(dlg.getVal("text-decoration").find("underline")
	       != std::string::npos);
	dlg.setFontDecoration(false, true, false, false, false);
	TFPASS(!dlg.getChangedOverline(&b) && b);
	dlg.setFontDecoration(false, false, true, false, true);
	TFPASS(!dlg.getChangedStrikeOut(&b) && b);
	TFPASS(!dlg.getChangedTopline(&b) && !b);
	TFPASS(!dlg.getChangedBottomline(&b) && b);

	dlg.setHidden(true);
	TFPASS(!dlg.getChangedHidden(&b) && b);
	TFPASS(dlg.getVal("display") == "none");
	dlg.setSuperScript(true);
	TFPASS(!dlg.getChangedSuperScript(&b) && b);
	dlg.setSubScript(true);
	TFPASS(!dlg.getChangedSubScript(&b) && b);

	/* replacing an existing prop */
	dlg.addOrReplaceVecProp("font-family", "Sans");
	TFPASS(dlg.getVal("font-family") == "Sans");
	TFPASS(dlg.getVal("no-such-prop").empty());

	/* setAllPropsFromVec consumes name/value pairs and refreshes
	 * the decoration/hidden flags from the new map */
	std::vector<std::string> v = {"font-family", "Mono", "font-size", "8pt",
	                              "text-decoration", "underline overline",
	                              "display", "none"};
	dlg.setAllPropsFromVec(v);
	TFPASS(dlg.getVal("font-family") == "Mono");
	TFPASS(!dlg.getChangedUnderline(&b) && b);
	TFPASS(!dlg.getChangedOverline(&b) && b);
	TFPASS(!dlg.getChangedHidden(&b) && !b); /* display "none" -> hidden */

	UT_UCS4Char draw[] = {'S','a','m','p','l','e',0};
	dlg.setDrawString(draw);
	TFPASS(dlg.getDrawString() != nullptr);
	TFPASS(UT_UCS4_strcmp(dlg.getDrawString(), draw) == 0);
	/* an empty string restores the default preview text */
	UT_UCS4Char empty[] = {0};
	dlg.setDrawString(empty);
	TFPASS(dlg.getDrawString() != nullptr);
	TFPASS(dlg.getDrawString()[0] != 0);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_Encoding                                                 */
/* ------------------------------------------------------------------ */

namespace {
class TestEncoding : public XAP_Dialog_Encoding
{
public:
	TestEncoding(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_Encoding(f, id) {}
	void runModal(XAP_Frame *) override {}

	using XAP_Dialog_Encoding::_setAnswer;
	using XAP_Dialog_Encoding::_setSelectionIndex;
	using XAP_Dialog_Encoding::_setEncoding;
	using XAP_Dialog_Encoding::_getSelectionIndex;
	using XAP_Dialog_Encoding::_getAllEncodings;
	using XAP_Dialog_Encoding::_getEncodingsCount;
};
}

TFTEST_MAIN("XAP_Dialog_Encoding")
{
	TestEncoding dlg(dlgFactory(), XAP_DIALOG_ID_ENCODING);

	TFPASS(dlg._getEncodingsCount() > 0);
	TFPASS(dlg._getAllEncodings() != nullptr);

	dlg.setEncoding("UTF-8");
	const gchar *enc = dlg.getEncoding();
	TFPASS(enc != nullptr);

	/* selecting by index updates what getEncoding returns */
	if (dlg._getEncodingsCount() > 1)
	{
		dlg._setSelectionIndex(0);
		dlg._setEncoding(dlg._getAllEncodings()[0]);
	}

	dlg._setAnswer(XAP_Dialog_Encoding::a_OK);
	TFPASS(dlg.getAnswer() == XAP_Dialog_Encoding::a_OK);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_Zoom                                                     */
/* ------------------------------------------------------------------ */

namespace {
class TestZoom : public XAP_Dialog_Zoom
{
public:
	TestZoom(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_Zoom(f, id) {}
	void runModal(XAP_Frame *) override {}
};
}

TFTEST_MAIN("XAP_Dialog_Zoom")
{
	TestZoom dlg(dlgFactory(), XAP_DIALOG_ID_ZOOM);

	TFPASS(dlg.getAnswer() == XAP_Dialog_Zoom::a_OK);

	dlg.setZoomType(XAP_Frame::z_PERCENT);
	dlg.setZoomPercent(150);
	TFPASS(dlg.getZoomPercent() == 150);

	/* values are clamped to [20, 500] */
	dlg.setZoomPercent(1);
	TFPASS(dlg.getZoomPercent() == XAP_DLG_ZOOM_MINIMUM_ZOOM);
	dlg.setZoomPercent(600);
	TFPASS(dlg.getZoomPercent() == XAP_DLG_ZOOM_MAXIMUM_ZOOM);

	/* preset zoom types report their fixed percentage */
	dlg.setZoomType(XAP_Frame::z_200);
	TFPASS(dlg.getZoomType() == XAP_Frame::z_200);
	TFPASS(dlg.getZoomPercent() == 200);
	dlg.setZoomType(XAP_Frame::z_100);
	TFPASS(dlg.getZoomPercent() == 100);
	dlg.setZoomType(XAP_Frame::z_75);
	TFPASS(dlg.getZoomPercent() == 75);

	/* page-width/whole-page with no frame fall back to 100 */
	dlg.setZoomType(XAP_Frame::z_PAGEWIDTH);
	TFPASS(dlg.getZoomType() == XAP_Frame::z_PAGEWIDTH);
	TFPASS(dlg.getZoomPercent() == 100);
	dlg.setZoomType(XAP_Frame::z_WHOLEPAGE);
	TFPASS(dlg.getZoomPercent() == 100);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_MessageBox                                               */
/* ------------------------------------------------------------------ */

namespace {
class TestMessageBox : public XAP_Dialog_MessageBox
{
public:
	TestMessageBox(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_MessageBox(f, id) {}
	void runModal(XAP_Frame *) override {}
};
}

TFTEST_MAIN("XAP_Dialog_MessageBox")
{
	TestMessageBox dlg(dlgFactory(), XAP_DIALOG_ID_MESSAGE_BOX);

	dlg.setMessage("a %s message %d", "test", 42);
	dlg.setSecondaryMessage("secondary %s", "detail");
	dlg.setButtons(XAP_Dialog_MessageBox::b_O);
	dlg.setDefaultAnswer(XAP_Dialog_MessageBox::a_OK);
	TFPASS(dlg.getAnswer() == XAP_Dialog_MessageBox::a_OK);

	dlg.setButtons(XAP_Dialog_MessageBox::b_YN);
	dlg.setButtons(XAP_Dialog_MessageBox::b_OC);
	dlg.setButtons(XAP_Dialog_MessageBox::b_YNC);
	dlg.setDefaultAnswer(XAP_Dialog_MessageBox::a_NO);
	TFPASS(dlg.getAnswer() == XAP_Dialog_MessageBox::a_NO);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_ClipArt                                                  */
/* ------------------------------------------------------------------ */

namespace {
class TestClipArt : public XAP_Dialog_ClipArt
{
public:
	TestClipArt(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_ClipArt(f, id) {}
	void runModal(XAP_Frame *) override {}

	using XAP_Dialog_ClipArt::getInitialDir;
	using XAP_Dialog_ClipArt::setGraphicName;
	using XAP_Dialog_ClipArt::setAnswer;
};
}

TFTEST_MAIN("XAP_Dialog_ClipArt")
{
	TestClipArt dlg(dlgFactory(), XAP_DIALOG_ID_CLIPART);

	dlg.setInitialDir("/usr/share/pixmaps");
	TFPASS(dlg.getInitialDir() && !strcmp(dlg.getInitialDir(), "/usr/share/pixmaps"));

	dlg.setGraphicName("logo.png");
	TFPASS(dlg.getGraphicName() && !strcmp(dlg.getGraphicName(), "logo.png"));

	dlg.setAnswer(XAP_Dialog_ClipArt::a_OK);
	TFPASS(dlg.getAnswer() == XAP_Dialog_ClipArt::a_OK);
	dlg.setAnswer(XAP_Dialog_ClipArt::a_CANCEL);
	TFPASS(dlg.getAnswer() == XAP_Dialog_ClipArt::a_CANCEL);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_ListDocuments                                            */
/* ------------------------------------------------------------------ */

namespace {
class TestListDocuments : public XAP_Dialog_ListDocuments
{
public:
	TestListDocuments(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_ListDocuments(f, id) {}
	void runModal(XAP_Frame *) override {}

	using XAP_Dialog_ListDocuments::_getDocumentCount;
	using XAP_Dialog_ListDocuments::_getNthDocumentName;
	using XAP_Dialog_ListDocuments::_getTitle;
	using XAP_Dialog_ListDocuments::_getOKButtonText;
	using XAP_Dialog_ListDocuments::_getHeading;
	using XAP_Dialog_ListDocuments::_setSelDocumentIndx;
	using XAP_Dialog_ListDocuments::setIncludeActiveDoc;
};
}

TFTEST_MAIN("XAP_Dialog_ListDocuments")
{
	TestListDocuments dlg(dlgFactory(), XAP_DIALOG_ID_LISTDOCUMENTS);

	/* the test app has no open documents */
	TFPASS(dlg._getDocumentCount() >= 0);
	TFPASS(dlg._getNthDocumentName(dlg._getDocumentCount() + 5) == nullptr);
	TFPASS(dlg.getDocument() == nullptr);
	TFPASS(dlg.getAnswer() == XAP_Dialog_ListDocuments::a_OK);

	/* title/heading/ok-text come from the string set */
	TFPASS(dlg._getTitle() != nullptr);
	TFPASS(dlg._getOKButtonText() != nullptr);
	TFPASS(dlg._getHeading() != nullptr);

	dlg.setIncludeActiveDoc(true);
	dlg.setIncludeActiveDoc(false);
	dlg._setSelDocumentIndx(-1);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_FileOpenSaveAs                                           */
/* ------------------------------------------------------------------ */

namespace {
class TestFileOpenSaveAs : public XAP_Dialog_FileOpenSaveAs
{
public:
	TestFileOpenSaveAs(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_FileOpenSaveAs(f, id) {}
	void runModal(XAP_Frame *) override {}
};
}

TFTEST_MAIN("XAP_Dialog_FileOpenSaveAs")
{
	TestFileOpenSaveAs dlg(dlgFactory(), XAP_DIALOG_ID_FILE_SAVEAS);

	dlg.useStart();
	/* useStart resets the answer/result pathname */
	TFPASS(dlg.getAnswer() == XAP_Dialog_FileOpenSaveAs::a_VOID);
	TFPASS(dlg.getPathname().empty());

	dlg.setCurrentPathname("/tmp/somefile");
	dlg.setSuggestFilename(true);

	/* a file type list is just stored for the platform subclass */
	static const char *descs[] = {"AbiWord", nullptr};
	static const char *sufs[] = {".abw", nullptr};
	static const UT_sint32 types[] = {1};
	dlg.setFileTypeList(descs, sufs, types);
	(void)dlg.getFileType();

	TFPASS(dlg.getEncryptionPassword().empty());

	dlg.setAppendDefaultSuffixFunctor(
		[](std::string path, UT_sint32) -> std::string {
			return path + ".abw";
		});
	dlg.useEnd();

	/* the preferred-suffix helper (a free function): unknown
	 * type -> name unchanged */
	std::string suffix =
		getAppendDefaultSuffixFunctorUsing_IE_Exp_preferredSuffixForFileType()(
			"doc", static_cast<UT_sint32>(IEFT_Unknown));
	TFPASS(suffix == "doc");
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_Password                                                 */
/* ------------------------------------------------------------------ */

namespace {
class TestPassword : public XAP_Dialog_Password
{
public:
	TestPassword(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_Password(f, id) {}
	void runModal(XAP_Frame *) override {}

	using XAP_Dialog_Password::setPassword;
	using XAP_Dialog_Password::setAnswer;
};
}

TFTEST_MAIN("XAP_Dialog_Password")
{
	TestPassword dlg(dlgFactory(), XAP_DIALOG_ID_PASSWORD);

	dlg.setPassword("s3cret");
	TFPASS(dlg.getPassword() == "s3cret");

	UT_UTF8String p("other");
	dlg.setPassword(p);
	TFPASS(dlg.getPassword() == "other");

	dlg.setAnswer(XAP_Dialog_Password::a_OK);
	TFPASS(dlg.getAnswer() == XAP_Dialog_Password::a_OK);
	dlg.setAnswer(XAP_Dialog_Password::a_Cancel);
	TFPASS(dlg.getAnswer() == XAP_Dialog_Password::a_Cancel);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_PrintPreview                                             */
/* ------------------------------------------------------------------ */

namespace {
class TestPrintPreview : public XAP_Dialog_PrintPreview
{
public:
	TestPrintPreview(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_PrintPreview(f, id) {}
	void runModal(XAP_Frame *) override {}
	GR_Graphics *getPrinterGraphicsContext(void) override { return nullptr; }
	void releasePrinterGraphicsContext(GR_Graphics *) override {}
};
}

TFTEST_MAIN("XAP_Dialog_PrintPreview")
{
	TestPrintPreview dlg(dlgFactory(), XAP_DIALOG_ID_PRINTPREVIEW);

	dlg.setPaperSize("A4");
	dlg.setDocumentTitle("Test Document");
	dlg.setDocumentPathname("/tmp/test.abw");
	TFPASS(true);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_WindowMore                                               */
/* ------------------------------------------------------------------ */

namespace {
class TestWindowMore : public XAP_Dialog_WindowMore
{
public:
	TestWindowMore(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_WindowMore(f, id) {}
	void runModal(XAP_Frame *) override {}
};
}

TFTEST_MAIN("XAP_Dialog_WindowMore")
{
	TestWindowMore dlg(dlgFactory(), XAP_DIALOG_ID_WINDOWMORE);

	TFPASS(dlg.getAnswer() == XAP_Dialog_WindowMore::a_OK
		   || dlg.getAnswer() == XAP_Dialog_WindowMore::a_CANCEL);
	TFPASS(dlg.getSelFrame() == nullptr);
}

/* ------------------------------------------------------------------ */
/* XAP_Dialog_HTMLOptions                                              */
/* ------------------------------------------------------------------ */

namespace {
class TestHTMLOptions : public XAP_Dialog_HTMLOptions
{
public:
	TestHTMLOptions(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_HTMLOptions(f, id) {}
	void runModal(XAP_Frame *) override {}

	using XAP_Dialog_HTMLOptions::get_HTML4;
	using XAP_Dialog_HTMLOptions::get_PHTML;
	using XAP_Dialog_HTMLOptions::get_Declare_XML;
	using XAP_Dialog_HTMLOptions::get_Allow_AWML;
	using XAP_Dialog_HTMLOptions::get_Embed_CSS;
	using XAP_Dialog_HTMLOptions::get_Link_CSS;
	using XAP_Dialog_HTMLOptions::get_Class_Only;
	using XAP_Dialog_HTMLOptions::get_Embed_Images;
	using XAP_Dialog_HTMLOptions::get_Multipart;
	using XAP_Dialog_HTMLOptions::get_Abs_Units;
	using XAP_Dialog_HTMLOptions::get_Scale_Units;
	using XAP_Dialog_HTMLOptions::get_MathML_Render_PNG;
	using XAP_Dialog_HTMLOptions::get_Split_Document;
	using XAP_Dialog_HTMLOptions::get_Compact;
	using XAP_Dialog_HTMLOptions::get_Link_CSS_File;
	using XAP_Dialog_HTMLOptions::can_set_Declare_XML;
	using XAP_Dialog_HTMLOptions::can_set_Allow_AWML;
	using XAP_Dialog_HTMLOptions::can_set_Embed_CSS;
	using XAP_Dialog_HTMLOptions::can_set_Link_CSS;
	using XAP_Dialog_HTMLOptions::can_set_Class_Only;
	using XAP_Dialog_HTMLOptions::can_set_Abs_Units;
	using XAP_Dialog_HTMLOptions::can_set_Scale_Units;
	using XAP_Dialog_HTMLOptions::can_set_Embed_Images;
	using XAP_Dialog_HTMLOptions::can_set_MathML_Render_PNG;
	using XAP_Dialog_HTMLOptions::can_set_Split_Document;
	using XAP_Dialog_HTMLOptions::set_HTML4;
	using XAP_Dialog_HTMLOptions::set_PHTML;
	using XAP_Dialog_HTMLOptions::set_Declare_XML;
	using XAP_Dialog_HTMLOptions::set_Allow_AWML;
	using XAP_Dialog_HTMLOptions::set_Embed_CSS;
	using XAP_Dialog_HTMLOptions::set_Link_CSS;
	using XAP_Dialog_HTMLOptions::set_Class_Only;
	using XAP_Dialog_HTMLOptions::set_Embed_Images;
	using XAP_Dialog_HTMLOptions::set_MathML_Render_PNG;
	using XAP_Dialog_HTMLOptions::set_Split_Document;
	using XAP_Dialog_HTMLOptions::set_Link_CSS_File;
	using XAP_Dialog_HTMLOptions::set_Abs_Units;
	using XAP_Dialog_HTMLOptions::set_Scale_Units;
	using XAP_Dialog_HTMLOptions::set_Compact;
	using XAP_Dialog_HTMLOptions::saveDefaults;
	using XAP_Dialog_HTMLOptions::restoreDefaults;
};
}

TFTEST_MAIN("XAP_Dialog_HTMLOptions")
{
	TestHTMLOptions dlg(dlgFactory(), XAP_DIALOG_ID_HTMLOPTIONS);
	XAP_Exp_HTMLOptions opt = {};

	XAP_Dialog_HTMLOptions::getHTMLDefaults(&opt, XAP_App::getApp());
	dlg.setHTMLOptions(&opt, XAP_App::getApp());

	dlg.set_HTML4(true);
	TFPASS(dlg.get_HTML4());
	/* when Is4 is set, Declare_XML/Allow_AWML cannot be set */
	TFPASS(!dlg.can_set_Declare_XML());
	TFPASS(!dlg.can_set_Allow_AWML());
	dlg.set_HTML4(false);

	dlg.set_PHTML(true);
	TFPASS(dlg.get_PHTML());
	/* AbiWebDoc forbids embedding CSS */
	TFPASS(!dlg.can_set_Embed_CSS());
	dlg.set_PHTML(false);

	dlg.set_Declare_XML(true);
	TFPASS(dlg.get_Declare_XML());
	dlg.set_Allow_AWML(true);
	TFPASS(dlg.get_Allow_AWML());
	dlg.set_Embed_CSS(true);
	TFPASS(dlg.get_Embed_CSS());
	dlg.set_Link_CSS(true);
	TFPASS(dlg.get_Link_CSS());
	dlg.set_Class_Only(true);
	TFPASS(dlg.get_Class_Only());
	dlg.set_MathML_Render_PNG(true);
	TFPASS(dlg.get_MathML_Render_PNG());

	/* Multipart on -> Embed_Images/Split_Document cannot be set */
	opt.bMultipart = true;
	TFPASS(!dlg.can_set_Embed_Images());
	TFPASS(!dlg.can_set_Split_Document());
	opt.bMultipart = false;
	TFPASS(dlg.can_set_Embed_Images());
	TFPASS(dlg.can_set_Split_Document());

	dlg.set_Embed_Images(true);
	TFPASS(dlg.get_Embed_Images());
	dlg.set_Split_Document(true);
	TFPASS(dlg.get_Split_Document());
	dlg.set_Abs_Units(true);
	TFPASS(dlg.get_Abs_Units());
	dlg.set_Scale_Units(true);
	TFPASS(dlg.get_Scale_Units());
	dlg.set_Compact(3);
	TFPASS(dlg.get_Compact() == 3);

	dlg.set_Link_CSS_File("/tmp/style.css");
	TFPASS(dlg.get_Link_CSS_File() == "/tmp/style.css");

	/* remaining can_set_* are unconditional */
	TFPASS(dlg.can_set_Declare_XML());
	TFPASS(dlg.can_set_Allow_AWML());
	TFPASS(dlg.can_set_Link_CSS());
	TFPASS(dlg.can_set_Class_Only());
	TFPASS(dlg.can_set_Abs_Units());
	TFPASS(dlg.can_set_Scale_Units());
	TFPASS(dlg.can_set_MathML_Render_PNG());

	/* save/restore defaults go through the app's prefs;
	 * m_bShouldSave starts true and only a cancel flips it.
	 * saveDefaults() mutates the live user profile, so stash
	 * the original value and put it back afterwards. */
	{
		XAP_Prefs *prefs = XAP_App::getApp()->getPrefs();
		std::string orig;
		bool hadOrig = prefs && prefs->getPrefsValue(
			XAP_PREF_KEY_HTMLExportOptions, orig, false);

		dlg.saveDefaults();
		dlg.restoreDefaults();
		TFPASS(dlg.shouldSave());

		if (prefs && prefs->getCurrentScheme())
			prefs->getCurrentScheme()->setValue(
				XAP_PREF_KEY_HTMLExportOptions, hadOrig ? orig : "");
	}
}
