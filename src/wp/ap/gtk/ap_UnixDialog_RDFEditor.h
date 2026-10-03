/* AbiWord
 * Copyright (C) 2011 AbiSource, Inc.
 * Copyright (C) Ben Martin
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

#pragma once

#include "ap_Dialog_RDFEditor.h"
#include "fv_View.h"

class XAP_UnixFrame;
typedef struct _AbiRdfTripleRow AbiRdfTripleRow;

class AP_UnixDialog_RDFEditor: public AP_Dialog_RDFEditor
{
public:
	AP_UnixDialog_RDFEditor (XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_RDFEditor (void);

	static XAP_Dialog *static_constructor (XAP_DialogFactory *,
										   XAP_Dialog_Id id);

	virtual void runModeless(XAP_Frame *pFrame) override;
	virtual void notifyActiveFrame(XAP_Frame *pFrame) override;
	virtual void activate(void) override;
	virtual void destroy(void) override;

    void onExecuteClicked();
    void onShowAllClicked();
    void onDelClicked();
    void commitCellEdit( AbiRdfTripleRow *row,
                         const char *new_text,
                         int cidx );
    void selectRowForCellClick( AbiRdfTripleRow *row, guint modifiers );
    void onImportRDFXML();
    void onExportRDFXML();
    void onCursorChanged();

	const GtkWidget *getWindow (void) { return m_wDialog; }

    virtual void clear() override;
    virtual void addStatement(const PD_RDFStatement& st) override;
    virtual void setStatus(const std::string& msg) override;
    virtual void removeStatement(const PD_RDFStatement& st) override;
    virtual std::list<PD_RDFStatement> getSelection() override;
    virtual void setSelection(const std::list<PD_RDFStatement>& l) override;
    virtual void hideRestrictionXMLID(bool v) override;

    PD_RDFStatement next( const PD_RDFStatement& st );

protected:

	void _constructWindow 	  (XAP_Frame *pFrame);
	void _updateWindow		  (void);

private:

  enum: uint8_t
    {
        C_SUBJ_COLUMN = 0,
        C_PRED_COLUMN,
        C_OBJ_COLUMN,
	C_COLUMN_COUNT
    };

	GtkWidget *m_wDialog;
	GtkWidget *m_btClose;
    GtkWidget *m_btShowAll;
	GtkColumnView*   m_resultsView;
	GListStore*      m_resultsStore;
	GtkSortListModel* m_sortModel;
    GtkWidget *m_status;
    GSimpleAction *m_anewtriple;
    GSimpleAction *m_acopytriple;
    GSimpleAction *m_adeletetriple;
    GSimpleAction *m_aimportrdfxml;
    GSimpleAction *m_aexportrdfxml;
    GtkDropDown *m_selectedxmlid;
    GtkWidget   *m_restrictxmlidhidew;

    guint findRowPos( const PD_RDFStatement& st );
    guint rowPosition( AbiRdfTripleRow* row );
    PD_RDFStatement rowToStatement( AbiRdfTripleRow* row );

};
