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

/* TST09 — fp_FieldTableSumRun: the sum_rows/sum_cols field runs only
 * exist once a document is laid out, so import legs never reach them
 * (0% coverage at baseline).  The committed fixture
 * test/wp/cov15/tablesum.abw carries all three cases: a sum field
 * outside a table ("???"), sum_rows over a numeric column, and
 * sum_cols across a row that references the computed field itself.
 */

#include "tf_test.h"
#include "tf_guard.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fv_View.h"
#include "fp_Run.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_types.h"
#include "ut_growbuf.h"

#include <cstring>
#include <string>

#define TFSUITE "core.text.fmt.tablesum"

namespace {

struct SumView
{
	SumView() = default;
	SumView(const SumView &) = delete;
	SumView &operator=(const SumView &) = delete;

	bool load(const char *relpath)
	{
		std::string data_file;
		if (!TF_Test::ensure_test_data(relpath, data_file))
			return false;
		doc = new PD_Document;
		if (doc->readFromFile(data_file.c_str(), IEFT_Unknown,
							  nullptr) != UT_OK) {
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
		view->setWindowSize(800, 600);
		return layout->countPages() > 0;
	}

	/* collected field values, in document order */
	std::string fieldValues() const
	{
		std::string out;
		fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
		for (; pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			for (fp_Run *pRun = pBlock->getFirstRun(); pRun;
				 pRun = pRun->getNextRun())
			{
				if (pRun->getType() != FPRUN_FIELD)
					continue;
				fp_FieldRun *pFRun =
					static_cast<fp_FieldRun *>(pRun);
				const UT_UCS4Char *v = pFRun->getValue();
				out += '[';
				for (const UT_UCS4Char *p = v; p && *p; ++p)
					if (*p < 0x80)
						out += static_cast<char>(*p);
				out += ']';
			}
		}
		return out;
	}

	~SumView()
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

} // namespace

TFTEST_MAIN("table sum fields compute laid out")
{
	SumView v;
	{
		tf_guard::call([&] {
			TFPASS(v.load("/test/wp/cov15/tablesum.abw"));
		}, 20000);
	}
	if (!v.doc)
		return;

	const std::string fields = v.fieldValues();
	fprintf(stderr, "tablesum fields: %s\n", fields.c_str());
	/* fixture: stray field outside the table -> "???";
	 * sum_rows over column 0 (1 + 4) -> 5;
	 * sum_cols across row 2 (field 5 + 7) -> 12 */
	TFPASS(fields.find("[???]") != std::string::npos);
	TFPASS(fields.find("[5]") != std::string::npos);
	TFPASS(fields.find("[12]") != std::string::npos);
}
