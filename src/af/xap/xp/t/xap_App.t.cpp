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
#include <vector>

#include "tf_test.h"

#include "xap_App.h"
#include "xap_Prefs.h"
#include "xap_Dialog.h"
#include "xap_Dialog_Id.h"
#include "xap_DialogFactory.h"
#include "xav_Listener.h"

#define TFSUITE "core.af.xap.app"

namespace {
class AppTestModeless : public XAP_Dialog_Modeless
{
public:
	AppTestModeless(XAP_DialogFactory * f, XAP_Dialog_Id id)
		: XAP_Dialog_Modeless(f, id) {}
	void runModal(XAP_Frame *) override {}
	void runModeless(XAP_Frame *) override {}
	void destroy(void) override {}
	void activate(void) override {}
};
}

TFTEST_MAIN("XAP_App singleton and identity")
{
	XAP_App *app = XAP_App::getApp();
	TFPASS(app != nullptr);

	TFPASS(app->getApplicationName() != nullptr);
	TFPASS(app->getApplicationTitleForTitleBar() != nullptr);
	TFPASS(app->getApplicationDisplayName() != nullptr);
	TFPASS(app->getAbiSuiteLibDir() != nullptr);
	TFPASS(app->getAbiSuiteAppDir() != nullptr);
	TFPASS(app->getUserPrivateDirectory() != nullptr);

	/* static build info strings exist (may be empty) */
	(void)XAP_App::s_szBuild_ID;

	/* core services are initialized */
	TFPASS(app->getDialogFactory() != nullptr);
	TFPASS(app->getControlFactory() != nullptr);
	TFPASS(app->getStringSet() != nullptr);
	TFPASS(app->getEncodingManager() != nullptr);
	TFPASS(app->getMenuActionSet() != nullptr);
	TFPASS(app->getToolbarActionSet() != nullptr);
	TFPASS(app->getUUIDGenerator() != nullptr);
	TFPASS(app->getPrefs() != nullptr);
	TFPASS(app->getEditMethodContainer() != nullptr);
}

TFTEST_MAIN("XAP_App frames")
{
	XAP_App *app = XAP_App::getApp();

	/* the test binary opens no frames */
	TFPASS(app->getFrameCount() == 0);
	TFPASS(app->getFrame(0) == nullptr);
	TFPASS(app->getLastFocussedFrame() == nullptr);
	TFPASS(app->findValidFrame() == nullptr);
	TFPASS(app->findFrame(static_cast<XAP_Frame*>(nullptr)) == -1);
	TFPASS(app->findFrame("no/such/file.abw") == -1);
	TFPASS(app->safefindFrame(nullptr) == -1);

	std::vector<XAP_Frame *> v;
	app->enumerateFrames(v);
	TFPASS(v.empty());

	app->clearLastFocussedFrame();
	app->notifyModelessDlgsOfActiveFrame(nullptr);
	app->notifyModelessDlgsCloseFrame(nullptr);
	app->rebuildMenus();

	/* listener notification with no frames/view is a no-op */
	app->notifyListeners(nullptr, AV_CHG_INPUTMODE);
}

TFTEST_MAIN("XAP_App modeless dialog table")
{
	XAP_App *app = XAP_App::getApp();
	AppTestModeless dlg(app->getDialogFactory(), XAP_DIALOG_ID_ZOOM);

	TFPASS(!app->isModelessRunning(555));

	app->rememberModelessId(555, &dlg);
	TFPASS(app->isModelessRunning(555));
	/* slot 0 is the first free slot -> the one we just used */
	TFPASS(app->getModelessDialog(0) == &dlg);

	app->forgetModelessId(555);
	TFPASS(!app->isModelessRunning(555));

	/* forgetting an absent id is harmless */
	app->forgetModelessId(555);
}

TFTEST_MAIN("XAP_App bindings and input modes")
{
	XAP_App *app = XAP_App::getApp();

	TFPASS(app->getBindingSet() != nullptr);
	TFPASS(app->getBindingMap("default") != nullptr);
	TFPASS(app->getBindingMap("no-such-binding") == nullptr);

	const char *mode = app->getInputMode();
	TFPASS(mode != nullptr);

	/* re-selecting the current mode is a no-change */
	TFPASS(app->setInputMode(mode) == 0);

	/* an unknown mode fails */
	TFPASS(app->setInputMode("no-such-mode") == -1);

	/* ... but the current mode is still in force */
	TFPASS(app->getInputMode() != nullptr);
}

TFTEST_MAIN("XAP_App prefs and geometry")
{
	XAP_App *app = XAP_App::getApp();
	XAP_Prefs *prefs = app->getPrefs();
	TFPASS(prefs != nullptr);

	/* getPrefsValue* go through the app's real prefs */
	std::string v;
	(void)app->getPrefsValue("no-such-pref-key-xyz", v);
	bool b = false;
	(void)app->getPrefsValueBool("no-such-pref-key-xyz", b);

	/* geometry round-trips through the prefs */
	TFPASS(app->setGeometry(11, 22, 800, 600, PREF_FLAG_GEOMETRY_SIZE | PREF_FLAG_GEOMETRY_POS));
	UT_sint32 x = 0, y = 0;
	UT_uint32 w = 0, h = 0, fl = 0;
	TFPASS(app->getGeometry(&x, &y, &w, &h, &fl));
	TFPASS(x == 11 && y == 22 && w == 800 && h == 600);

	/* GNOME-style geometry strings are parsed: "WxH+X+Y" */
	app->parseAndSetGeometry("640x480+7+9");
	TFPASS(app->getGeometry(&x, &y, &w, &h, &fl));
	TFPASS(w == 640 && h == 480);
	TFPASS(fl & PREF_FLAG_GEOMETRY_NOUPDATE);

	/* a bare size with no position */
	app->parseAndSetGeometry("500x400");

	/* an empty string sets nothing */
	app->parseAndSetGeometry("");

	/* default geometry query */
	UT_uint32 dw = 0, dh = 0, df = 0;
	app->getDefaultGeometry(dw, dh, df);
}

TFTEST_MAIN("XAP_App misc services")
{
	XAP_App *app = XAP_App::getApp();

	/* lib-file lookup: a file that cannot exist */
	std::string path;
	TFPASS(!app->findAbiSuiteLibFile(path, "definitely-not-here-12345"));
	TFPASS(!app->findAbiSuiteAppFile(path, "definitely-not-here-12345"));

	/* clipboard format strings are remembered (no-op impl) */
	app->addClipboardFmt("text/plain");
	app->deleteClipboardFmt("text/plain");

	/* keyboard language set/clear (getter is protected) */
	app->setKbdLanguage("en");
	app->setKbdLanguage(nullptr);
}
