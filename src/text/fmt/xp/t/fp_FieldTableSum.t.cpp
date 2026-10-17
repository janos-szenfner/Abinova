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

	/* run the field-update pass over every block, twice, so formulas
	 * whose inputs are other computed fields settle */
	void recalcFields()
	{
		for (int pass = 0; pass < 2; ++pass)
		{
			for (fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
				 pBlock; pBlock = pBlock->getNextBlockInDocument())
				pBlock->recalculateFields(0);
		}
	}

	/* position just past the last character of the block whose entire
	 * text is `txt`; 0 when no block matches */
	PT_DocPosition endOfTextBlock(const char *txt) const
	{
		const size_t want = strlen(txt);
		for (fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
			 pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			UT_GrowBuf buf;
			if (!pBlock->getBlockBuf(&buf) ||
				buf.getLength() != want)
				continue;
			const UT_UCS4Char *p =
				reinterpret_cast<const UT_UCS4Char *>(
					buf.getPointer(0));
			bool match = true;
			for (size_t i = 0; i < want; ++i)
				if (p[i] != static_cast<UT_UCS4Char>(txt[i]))
					match = false;
			if (match)
				/* getPosition() already points at the first content
				 * char (strux offset included) */
				return pBlock->getPosition() +
					static_cast<PT_DocPosition>(buf.getLength());
		}
		return 0;
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

TFTEST_MAIN("table formula fields evaluate and update")
{
	SumView v;
	{
		tf_guard::call([&] {
			TFPASS(v.load("/test/wp/cov15/tableformula.abw"));
		}, 20000);
	}
	if (!v.doc)
		return;

	v.recalcFields();
	/* the \# picture format follows the locale's decimal separator
	 * (like sFormatDouble/Word) — normalize ',' to '.' so the
	 * expected strings are locale-independent; no fixture value
	 * legitimately contains a ',' */
	auto deLocale = [](std::string s) {
		for (char &c : s)
			if (c == ',')
				c = '.';
		return s;
	};
	const std::string fields = deLocale(v.fieldValues());
	fprintf(stderr, "tableformula fields: %s\n", fields.c_str());
	/* fixture expectations, in document order:
	 *   [???]   =SUM(ABOVE) outside a table
	 *   [30]    =SUM(ABOVE) in C2: cell above is 30
	 *   [50]    =SUM(ABOVE) in A3: contiguous 40+10
	 *   [30]    =SUM(A1:B1) = 10+20
	 *   [40]    =AVERAGE(LEFT): the two computed cells 50,30 -> 40
	 *   [50.00] =SUM(A1:A2) \# 0.00
	 *   [10]    =MIN(A1:C1)
	 *   [50]    =B1*2+C1/3
	 */
	const std::string want =
		"[???][30][50][30][40][50.00][10][50]";
	TFPASS(fields == want);

	/* change A1 "10" -> "100" and confirm the formulas recalculate */
	const PT_DocPosition pos = v.endOfTextBlock("10");
	TFPASS(pos != 0);
	if (pos == 0)
		return;
	TFPASS(v.doc->insertSpan(pos, std::string("0")));
	v.recalcFields();

	const std::string fields2 = deLocale(v.fieldValues());
	fprintf(stderr, "tableformula after edit: %s\n", fields2.c_str());
	/* A1=100 -> SUM(ABOVE)@A3=140, SUM(A1:B1)=120,
	 * AVERAGE(LEFT)@C3=130, SUM(A1:A2) \# 0.00=140.00,
	 * MIN(A1:C1)=20; =B1*2+C1/3 (50) and C2's SUM(ABOVE)
	 * (30) unchanged */
	const std::string want2 =
		"[???][30][140][120][130][140.00][20][50]";
	TFPASS(fields2 == want2);
}
