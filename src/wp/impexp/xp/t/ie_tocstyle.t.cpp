#include "tf_test.h"
#include "pd_Document.h"
#include "pd_Style.h"
#include "pt_PieceTable.h"
#include "pp_AttrProp.h"

#define TFSUITE "wp.impexp.tocstyles"

TFTEST_MAIN("builtin Contents styles accept preset attributes")
{
	PD_Document * doc = new PD_Document();
	TFPASS(doc->newDocument() == UT_OK);

	PD_Style * pS = nullptr;
	TFPASS(doc->getStyle("Contents 1", &pS));
	TFPASS(doc->getStyle("Contents Header", &pS));

	PP_PropertyVector atts = {
		"props", "font-size:20pt; font-weight:bold"
	};
	TFPASS(doc->addStyleAttributes("Contents 1", atts));

	const PP_AttrProp * pAP = nullptr;
	pS = nullptr;
	doc->getStyle("Contents 1", &pS);
	TFPASS(pS != nullptr);
	doc->getAttrProp(pS->getIndexAP(), &pAP);
	const gchar * szProps = nullptr;
	TFPASS(pAP && pAP->getProperty("font-size", szProps));
	TFPASS(szProps && 0 == strcmp(szProps, "20pt"));

	doc->unref();
}
