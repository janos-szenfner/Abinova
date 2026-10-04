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
#include <map>

#include "tf_test.h"

#include "xad_Document.h"
#include "xap_App.h"
#include "ut_uuid.h"

#define TFSUITE "core.af.xap.addoc"

namespace {

class TestDoc : public AD_Document
{
public:
	TestDoc() : AD_Document(), m_dirty(false), m_saveCalls(0),
				m_crCount(0), m_purged(false) {}
	virtual ~TestDoc() {}

	AD_DOCUMENT_TYPE getType() const override { return ADDOCUMENT_ABIWORD; }
	UT_Error readFromFile(const char*, int, const char* = nullptr) override { return UT_OK; }
	UT_Error importFile(const char*, int, bool = false, bool = true,
						const char* = nullptr) override { return UT_OK; }
	UT_Error newDocument() override { return UT_OK; }
	bool isDirty() const override { return m_dirty; }
	bool canDo(bool) const override { return false; }
	bool undoCmd(UT_uint32) override { return false; }
	bool redoCmd(UT_uint32) override { return false; }
	bool createDataItem(const char*, bool, const UT_ConstByteBufPtr&,
						const std::string&, PD_DataItemHandle*) override { return false; }
	bool replaceDataItem(const char*, const UT_ConstByteBufPtr&) override { return false; }
	bool getDataItemDataByName(const char*, UT_ConstByteBufPtr&,
							   std::string*, PD_DataItemHandle*) const override { return false; }
	UT_uint32 getLastSavedAsType() const override { return 0; }
	void setMetaDataProp(const std::string& k, const std::string& v) override { m_meta[k] = v; }
	bool getMetaDataProp(const std::string& k, std::string& o) const override
	{
		auto it = m_meta.find(k);
		if (it == m_meta.end()) return false;
		o = it->second;
		return true;
	}
	void setAnnotationProp(const std::string& k, const std::string& v) override { m_ann[k] = v; }
	bool getAnnotationProp(const std::string& k, std::string& o) const override
	{
		auto it = m_ann.find(k);
		if (it == m_ann.end()) return false;
		o = it->second;
		return true;
	}
	bool areDocumentContentsEqual(const AD_Document&, UT_uint32&) const override { return true; }
	bool areDocumentFormatsEqual(const AD_Document&, UT_uint32&) const override { return true; }
	bool areDocumentStylesheetsEqual(const AD_Document&) const override { return true; }
	bool createAndSendDocPropCR(const gchar**, const gchar**) override
	{ m_crCount++; return true; }
	void purgeRevisionTable(bool = false) override { _purgeRevisionTable(); m_purged = true; }
	bool acceptRejectRevision(bool, UT_uint32, UT_uint32, UT_uint32) override { return true; }
	bool rejectAllHigherRevisions(UT_uint32) override { return true; }
	bool acceptAllRevisions() override { return true; }
	UT_uint32 getXID() const override { return 5; }
	UT_uint32 getTopXID() const override { return 5; }

	bool m_dirty;
	int m_saveCalls;
	int m_crCount;
	bool m_purged;
	std::map<std::string, std::string> m_meta;
	std::map<std::string, std::string> m_ann;

protected:
	UT_Error _saveAs(const char*, int, const char* = nullptr) override
	{ m_saveCalls++; return UT_OK; }
	UT_Error _saveAs(const char*, int, bool, const char* = nullptr) override
	{ m_saveCalls++; return UT_OK; }
	UT_Error _save() override { m_saveCalls++; return UT_OK; }
	void _clearUndo() override {}
};

} // namespace

TFTEST_MAIN("AD_Document basics")
{
	TestDoc *doc = new TestDoc();

	TFPASS(doc->getType() == ADDOCUMENT_ABIWORD);

	/* refcounting starts at 1; unref() on last ref deletes */
	doc->ref();
	doc->unref();

	/* filename accessors */
	doc->setFilename("/tmp/doc.abw");
	TFPASS(doc->getFilename() == "/tmp/doc.abw");
	doc->setFilename(nullptr);
	TFPASS(doc->getFilename().empty());
	doc->setPrintFilename("/tmp/print.abw");
	TFPASS(doc->getPrintFilename() == "/tmp/print.abw");

	/* encoding name: empty -> nullptr */
	TFPASS(doc->getEncodingName() == nullptr);
	doc->setEncodingName("UTF-8");
	TFPASS(doc->getEncodingName() && !strcmp(doc->getEncodingName(), "UTF-8"));
	doc->setEncodingName(nullptr);
	TFPASS(doc->getEncodingName() == nullptr);

	/* dirty flag plumbing */
	TFPASS(!doc->isForcedDirty());
	doc->forceDirty();
	TFPASS(doc->isForcedDirty());
	TFPASS(!doc->isDirty()); /* our own flag stays independent */

	/* times */
	TFPASS(doc->getLastOpenedTime() > 0);
	doc->setLastSavedTime(1000);
	TFPASS(doc->getLastSavedTime() == 1000);
	TFPASS(doc->getTimeSinceSave() >= 0);
	doc->setEditTime(42);
	TFPASS(doc->getEditTime() >= 42);
	doc->setDocVersion(7);
	TFPASS(doc->getDocVersion() == 7);

	/* the ctor made a valid UUID; my/orig copies equal it */
	TFPASS(doc->getDocUUID() != nullptr);
	TFPASS(doc->isOrigUUID());
	TFPASS(doc->getDocUUIDString() != nullptr);
	TFPASS(doc->getOrigDocUUIDString() != nullptr);
	TFPASS(!doc->getMyUUIDString().empty());

	/* document-unique UUID generation */
	UT_UUIDPtr u1 = doc->getNewUUID();
	TFPASS(u1 != nullptr);
	TFPASS(u1->isValid());
	UT_UUIDPtr u2 = doc->getNewUUID();
	TFPASS(u2 != nullptr);
	TFPASS(!(*u1 == *u2));
	TFPASS(doc->getNewUUID32() != 0);
	TFPASS(doc->getNewUUID64() != 0);

	doc->unref(); /* deletes */
}

TFTEST_MAIN("AD_Document related documents")
{
	TestDoc *d1 = new TestDoc();
	TestDoc *d2 = new TestDoc();

	/* fresh docs have distinct uuids -> unrelated */
	TFPASS(!d1->areDocumentsRelated(*d2));
	TFPASS(d1->areDocumentsRelated(*d1));

	/* point d2 at d1's uuid -> related */
	d2->setDocUUID(d1->getDocUUIDString());
	TFPASS(d1->areDocumentsRelated(*d2));

	/* histories: different histories are not equal even for
	 * related docs */
	UT_uint32 ver = 999;
	TFPASS(d1->areDocumentHistoriesEqual(*d2, ver));
	TFPASS(ver == 0);

	time_t t = time(nullptr);
	d1->addRecordToHistory(AD_VersionData(1, d1->getDocUUIDString(), t, false, 5));
	TFPASS(d1->getHistoryCount() == 1);
	TFPASS(!d1->areDocumentHistoriesEqual(*d2, ver));

	/* identical uuid + identical record -> equal */
	d2->addRecordToHistory(AD_VersionData(1, d1->getDocUUIDString(), t, false, 5));
	TFPASS(d1->areDocumentHistoriesEqual(*d2, ver));
	TFPASS(ver == 1);

	/* history accessors */
	TFPASS(d1->getHistoryNthId(0) == 1);
	TFPASS(d1->getHistoryNthTopXID(0) == 5);
	TFPASS(d1->getHistoryNthTime(0) > 0);
	TFPASS(d1->getHistoryNthTimeStarted(0) == t);
	TFPASS(d1->getHistoryNthEditTime(0) >= 0);
	TFPASS(!d1->getHistoryNthAutoRevisioned(0));
	(void)d1->getHistoryNthUID(0);
	TFPASS(d1->findHistoryRecord(1) != nullptr);
	TFPASS(d1->findHistoryRecord(99) == nullptr);

	d1->purgeHistory();
	TFPASS(d1->getHistoryCount() == 1); /* purge only clears the flag */

	d1->unref();
	d2->unref();
}

TFTEST_MAIN("AD_Document verifyHistoryState")
{
	TestDoc *doc = new TestDoc();
	UT_uint32 ver;

	/* empty history -> nothing to restore */
	ver = 0;
	TFPASS(doc->verifyHistoryState(ver) == ADHIST_NO_RESTORE);

	time_t t = time(nullptr);

	/* a non-auto-revisioned record does not enable restore */
	doc->addRecordToHistory(AD_VersionData(1, t, false, 5));
	ver = 0;
	TFPASS(doc->verifyHistoryState(ver) == ADHIST_NO_RESTORE);

	/* an autorevisioned record for version+1 -> full restore */
	doc->addRecordToHistory(AD_VersionData(2, t, true, 5));
	doc->addRecordToHistory(AD_VersionData(3, t, true, 5));
	ver = 1;
	TFPASS(doc->verifyHistoryState(ver) == ADHIST_FULL_RESTORE);

	/* a gap between requested version and auto records ->
	 * partial restore, version advanced to first restorable */
	doc->addRecordToHistory(AD_VersionData(5, t, true, 5));
	ver = 3;
	TFPASS(doc->verifyHistoryState(ver) == ADHIST_PARTIAL_RESTORE);
	TFPASS(ver == 5);

	doc->unref();
}

TFTEST_MAIN("AD_Document revisions")
{
	TestDoc *doc = new TestDoc();

	TFPASS(!doc->usingChangeTracking());
	TFPASS(doc->getHighestRevisionId() == 0);
	TFPASS(doc->getHighestRevision() == nullptr);
	TFPASS(doc->getRevisionIndxFromId(1) == -1);

	const UT_UCS4Char desc[] = {'d','e','s','c',0};
	TFPASS(doc->addRevision(1, desc, time(nullptr), 0, false));
	TFPASS(!doc->addRevision(1, desc, time(nullptr), 0, false)); /* dup id */
	TFPASS(doc->addRevision(3, desc, time(nullptr), 0, false, "author1"));

	TFPASS(doc->getHighestRevisionId() == 3);
	TFPASS(doc->getHighestRevision() != nullptr);
	TFPASS(doc->getHighestRevision()->getId() == 3);
	TFPASS(doc->getHighestRevision()->getAuthor() == "author1");
	TFPASS(doc->getRevisionIndxFromId(3) == 1);
	TFPASS(doc->getRevisionIndxFromId(1) == 0);
	TFPASS(doc->getRevisionIndxFromId(77) == -1);
	TFPASS(doc->getRevisions().size() == 2);

	/* highest revision > 1 counts as change tracking */
	TFPASS(doc->usingChangeTracking());

	/* bGenCR path notifies through createAndSendDocPropCR */
	TFPASS(doc->addRevision(4, desc, time(nullptr), 0, true));
	TFPASS(doc->m_crCount == 1);

	/* flags */
	TFPASS(!doc->isMarkRevisions());
	doc->setMarkRevisions(true);
	TFPASS(doc->isMarkRevisions());
	doc->toggleMarkRevisions();
	TFPASS(!doc->isMarkRevisions());

	TFPASS(doc->isShowRevisions());
	doc->setShowRevisions(false);
	TFPASS(!doc->isShowRevisions());
	doc->toggleShowRevisions();
	TFPASS(doc->isShowRevisions());

	doc->setShowRevisionId(9);
	TFPASS(doc->getShowRevisionId() == 9);
	doc->setRevisionId(9);
	TFPASS(doc->getRevisionId() == 9);

	/* purge through the public (stubbed) entry point */
	doc->purgeRevisionTable();
	TFPASS(doc->m_purged);
	TFPASS(doc->getRevisions().empty());

	doc->unref();
}

TFTEST_MAIN("AD_Document save wrappers")
{
	TestDoc *doc = new TestDoc();

	TFPASS(doc->save() == UT_OK);
	TFPASS(doc->m_saveCalls == 1);
	TFPASS(doc->saveAs("/tmp/x.abw", 0) == UT_OK);
	TFPASS(doc->saveAs("/tmp/x.abw", 0, true) == UT_OK);
	TFPASS(doc->m_saveCalls == 3);

	/* after a save, autorevisioning records history + a revision */
	doc->setAutoRevisioning(true);
	TFPASS(doc->isAutoRevisioning());
	TFPASS(doc->getHistoryCount() >= 1);
	TFPASS(doc->getRevisions().size() >= 1);
	doc->setAutoRevisioning(false);
	TFPASS(!doc->isAutoRevisioning());

	/* findAutoRevisionId maps version -> revision */
	UT_uint32 rid = doc->findAutoRevisionId(doc->getDocVersion());
	(void)rid;
	(void)doc->findNearestAutoRevisionId(1, true);
	(void)doc->findNearestAutoRevisionId(1, false);

	/* purgeAllRevisions needs a view -> false without one */
	TFPASS(!doc->purgeAllRevisions(nullptr));

	/* metadata pass-through */
	doc->setMetaDataProp("dc:title", "Hello");
	std::string out;
	TFPASS(doc->getMetaDataProp("dc:title", out));
	TFPASS(out == "Hello");
	TFPASS(!doc->getMetaDataProp("dc:missing", out));
	doc->setAnnotationProp("a", "b");
	TFPASS(doc->getAnnotationProp("a", out));
	TFPASS(out == "b");

	doc->unref();
}
