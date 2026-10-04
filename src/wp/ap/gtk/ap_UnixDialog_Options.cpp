/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2003, 2009 Hubert Figuiere
 * Copyright (C) 2025-2026 Abinova contributors
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ap_Features.h"

#include "ut_types.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"
#include "xap_GtkComboBoxHelpers.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_Prefs.h"
#include "xap_Toolbar_Layouts.h"
#include "xap_EncodingManager.h"

#include "ap_Dialog_Id.h"
#include "ap_Prefs_SchemeIds.h"
#include "ie_exp.h"

#include "ap_Strings.h"

#include "ap_UnixDialog_Options.h"

/*****************************************************************/

#define WID(widget)   GTK_WIDGET(gtk_builder_get_object(builder, widget))

/*****************************************************************/


XAP_Dialog * AP_UnixDialog_Options::static_constructor ( XAP_DialogFactory * pFactory,
        XAP_Dialog_Id id )
{
    return new AP_UnixDialog_Options ( pFactory,id );
}

AP_UnixDialog_Options::AP_UnixDialog_Options ( XAP_DialogFactory * pDlgFactory,
        XAP_Dialog_Id id )
        : AP_Dialog_Options ( pDlgFactory, id )
{}

AP_UnixDialog_Options::~AP_UnixDialog_Options ( void )
{
}

/*****************************************************************/

void AP_UnixDialog_Options::runModal ( XAP_Frame * pFrame )
{
    // Build the window's widgets and arrange them
    GtkWidget *mainWindow = _constructWindow();
    UT_ASSERT ( mainWindow );

    // save for use with event
    m_pFrame = pFrame;

    // Populate the window's data items
    _populateWindowData();

    // Don't destroy the dialog if the user pressed defaults or help
    gint response;
    do
    {
        response = abiRunModalDialog ( GTK_DIALOG ( mainWindow ), pFrame,
                                       this, GTK_RESPONSE_CLOSE, FALSE );
    } while ( response != GTK_RESPONSE_CLOSE && response != GTK_RESPONSE_DELETE_EVENT );

    abiDestroyWidget ( mainWindow );
}

///
/// All this color selection code is stolen from the ap_UnixDialog_Background
/// dialog
///
void AP_UnixDialog_Options::s_real_color_changed(GdkRGBA & gdkcolor, AP_UnixDialog_Options * dlg)
{

	UT_RGBColor * rgbcolor = UT_UnixGdkRGBAToRGBColor(gdkcolor);
	UT_HashColor hash_color;
    strncpy ( dlg->m_CurrentTransparentColor, hash_color.setColor(*rgbcolor),
			  sizeof(dlg->m_CurrentTransparentColor) - 1 );
	dlg->m_CurrentTransparentColor[sizeof(dlg->m_CurrentTransparentColor) - 1] = 0;
	
    UT_DEBUGMSG ( ( "Changing Color [%s]\n", hash_color.c_str() ) );
	delete rgbcolor;

    if ( strcmp ( dlg->m_CurrentTransparentColor, "#ffffff" ) == 0 )
        gtk_widget_set_sensitive ( dlg->m_buttonColSel_Defaults, FALSE );
    else
        gtk_widget_set_sensitive ( dlg->m_buttonColSel_Defaults, TRUE );

    // Update document view through instant apply magic. Emitting the "clicked" 
	// signal will result in a loop and
    // many dialogs popping up. Hacky, because we directly call a callback.
    s_control_changed ( dlg->m_pushbuttonNewTransparentColor, dlg );
}

void AP_UnixDialog_Options::s_color_changed ( GtkColorChooser *csel,
                                              GdkRGBA         *color,
                                              gpointer data )
{
    AP_UnixDialog_Options * dlg = static_cast<AP_UnixDialog_Options *> ( data );
    UT_ASSERT ( csel && dlg );
    UT_UNUSED(csel);

    UT_DEBUGMSG(("s_color_changed\n"));
    s_real_color_changed(*color, dlg);
}


void AP_UnixDialog_Options::event_ChooseTransparentColor ( void )
{
    GtkWidget *dlg;

//
// Run the Background dialog over the options? No the title is wrong.
//
    GtkWidget *colorsel;
	std::string s;

    const XAP_StringSet * pSS = m_pApp->getStringSet();

    GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_Options_ColorSel.ui");

    dlg = WID ( "ap_UnixDialog_Options_ColorSel" );
    pSS->getValueUTF8 ( AP_STRING_ID_DLG_Options_Label_ChooseForTransparent, s );
    abiDialogSetTitle ( dlg, "%s", s.c_str() );

    colorsel = WID ( "csColorSel" );

    // quiet hacky. Fetch defaults button from colsel GtkBuilder UI file and store it inside
    // the main dialog, because we'll need this for sensitivity toggling
    m_buttonColSel_Defaults = WID ( "btnDefaults" );

    g_signal_connect ( G_OBJECT ( colorsel ), "color-activated",
                       G_CALLBACK ( s_color_changed ),
                       static_cast<gpointer> ( this ) );

    UT_RGBColor c;
    UT_parseColor ( m_CurrentTransparentColor,c );
	GdkRGBA *gcolor = UT_UnixRGBColorToGdkRGBA(c);

    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorsel), gcolor);
	gdk_rgba_free(gcolor);

    // run into the gtk main loop for this window. If the reponse is 0, the user pressed Defaults.
    // Don't destroy it if he did so.
    while (!abiRunModalDialog(GTK_DIALOG(dlg), m_pFrame, this,
                                          GTK_RESPONSE_OK, FALSE)) {
        // Answer was 0, so reset color to default
        strncpy(m_CurrentTransparentColor, "ffffff", sizeof(m_CurrentTransparentColor) - 1);
		m_CurrentTransparentColor[sizeof(m_CurrentTransparentColor) - 1] = 0;

        UT_parseColor (m_CurrentTransparentColor, c);
        gcolor = UT_UnixRGBColorToGdkRGBA(c);
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorsel), gcolor);
        gdk_rgba_free(gcolor);
    }

    GdkRGBA cc;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorsel), &cc);
    s_real_color_changed(cc, this);
//
// Finish up here after a close or window delete signal.
//
    abiDestroyWidget ( dlg );

	g_object_unref(reinterpret_cast<GObject*>((builder)));
}

/*****************************************************************/

void AP_UnixDialog_Options::_setupUnitMenu ( GtkWidget *optionmenu, const XAP_StringSet *pSS )
{
	GtkDropDown *combo = GTK_DROP_DOWN(optionmenu);
	UnitMenuContent content;
	_getUnitMenuContent(pSS, content);
	XAP_makeGtkDropDown(combo);

	for(UnitMenuContent::const_iterator iter = content.begin();
		iter != content.end(); ++iter) {
		XAP_appendDropDownTextAndInt(combo, iter->first.c_str(), iter->second);
	}
	gtk_drop_down_set_selected(combo, 0);
}

void AP_UnixDialog_Options::_constructWindowContents ( GtkBuilder * builder )
{
    const XAP_StringSet *pSS = m_pApp->getStringSet();
    //const UT_Vector & vec = m_pApp->getToolbarFactory()->getToolbarNames();

    GtkWidget *tmp;

    // Dialog

    m_windowMain = WID ( "ap_UnixDialog_Options" );

    m_stack = WID ( "stkMain" );

    // page titles shown by the stack sidebar come from the string set,
    // not the .ui, so they track the app language
    std::string stTitle;
    pSS->getValueUTF8 ( AP_STRING_ID_DLG_Options_TabLabel_Interface, stTitle );
    gtk_stack_page_set_title ( GTK_STACK_PAGE ( gtk_builder_get_object (builder, "pageInterface" ) ),
                               stTitle.c_str() );
    pSS->getValueUTF8 ( AP_STRING_ID_DLG_Options_Label_Documents, stTitle );
    gtk_stack_page_set_title ( GTK_STACK_PAGE ( gtk_builder_get_object (builder, "pageDocuments" ) ),
                               stTitle.c_str() );
    pSS->getValueUTF8 ( AP_STRING_ID_DLG_Options_TabLabel_SmartQuotes, stTitle );
    gtk_stack_page_set_title ( GTK_STACK_PAGE ( gtk_builder_get_object (builder, "pageSmartQuotes" ) ),
                               stTitle.c_str() );
    pSS->getValueUTF8 ( AP_STRING_ID_DLG_Spell_SpellTitle, stTitle );
    gtk_stack_page_set_title ( GTK_STACK_PAGE ( gtk_builder_get_object (builder, "pageSpelling" ) ),
                               stTitle.c_str() );

    m_buttonDefaults = WID ( "btnDefaults" );
    m_buttonClose = WID ( "btnClose" );


    // Interface

    tmp = WID ( "lblUserInterface" );
    localizeLabelMarkup ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_UI );

    tmp = WID ( "lblUnits" );
    localizeLabelUnderline ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_ViewUnits );

    m_menuUnits = WID ( "omUnits" );
    _setupUnitMenu ( m_menuUnits, pSS );

    m_pushbuttonNewTransparentColor = WID ( "btnScreenColor" );

    tmp = WID ( "lblScreenColor" );
    localizeLabelUnderline ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_ChooseForTransparent );

    tmp = WID ( "lblOverwrite" );
    localizeLabelUnderline ( tmp, pSS,
                             AP_STRING_ID_DLG_Options_Label_EnableOverwrite );

    m_switchEnableOverwrite = WID ( "swOverwrite" );

    // Documents

    // Auto Save

    tmp = WID ( "lblAutoSave" );
    localizeLabelMarkup ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_AutoSave );

    m_switchAutoSaveFile = WID ( "swAutoSave" );

    m_gridAutoSaveFile = WID ( "tblAutoSave" );

    tmp = WID ( "lblInterval" );
    localizeLabelUnderline ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_AutoSaveInterval );

    m_textAutoSaveFilePeriod = WID ( "spInterval" );

    tmp = WID ( "lblFileExt" );
    localizeLabelUnderline ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_FileExtension );

    m_textAutoSaveFileExt = WID ( "enFileExt" );

    tmp = WID ( "lblMinutes" );
    localizeLabel ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_Minutes );

    // RTL Text Layout
    tmp = WID ( "lblRTL" );
    localizeLabelMarkup ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_BiDiOptions );

    tmp = WID ( "lblDirectionRtl" );
    localizeLabelUnderline ( tmp, pSS,
                             AP_STRING_ID_DLG_Options_Label_DirectionRtl );

    m_switchOtherDirectionRtl = WID ( "swDefaultToRTL" );

    // Default file format - the label is translated directly in the .ui
    m_menuSaveFormat = WID ( "omSaveFormat" );
    _setupSaveFormatMenu ( m_menuSaveFormat );

    // Smart Quotes

    tmp = WID ( "lblSmartQuotesEnable" );
    localizeLabel ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_SmartQuotes );

    m_switchSmartQuotes = WID ( "swSmartQuotes" );

    tmp = WID ( "lblCustomQuoteStyle" );
    localizeLabel ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_CustomSmartQuotes );

    m_switchCustomSmartQuotes = WID ( "swCustomQuoteStyle" );

    tmp = WID ( "lblOuterQuoteStyle" );
    localizeLabelUnderline ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_OuterQuoteStyle );

    tmp = WID ( "lblInnerQuoteStyle" );
    localizeLabelUnderline ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_InnerQuoteStyle );

    m_omOuterQuoteStyle = WID ( "omOuterQuoteStyle" );
    m_omInnerQuoteStyle = WID ( "omInnerQuoteStyle" );

    _setupSmartQuotesCombos(m_omOuterQuoteStyle);
    _setupSmartQuotesCombos(m_omInnerQuoteStyle);

    // Spelling

    tmp = WID ( "lblAutoSpell" );
    localizeLabel ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_SpellCheckAsYouType );

    m_switchSpellCheckAuto = WID ( "swAutoSpell" );

    tmp = WID ( "lblSpellCheck" );
    localizeLabelMarkup ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_SpellCheckOptions );

    tmp = WID ( "lblSpellCaps" );
    localizeLabel ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_SpellCheckCaps );

    m_switchSpellCheckCaps = WID ( "swSpellCaps" );

    tmp = WID ( "lblSpellNumbers" );
    localizeLabel ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_SpellCheckNumbers );

    m_switchSpellCheckNumbers = WID ( "swSpellNumbers" );

    tmp = WID ( "lblSpellInternet" );
    localizeLabel ( tmp, pSS, AP_STRING_ID_DLG_Options_Label_SpellCheckInternet );

    m_switchSpellCheckInternet = WID ( "swSpellInternet" );

    //////////////////////////////////////////////////////////////////

    // start with the autosave sub-grid sensitivity matching the switch
    gtk_widget_set_sensitive ( m_gridAutoSaveFile,
                               gtk_switch_get_active (
                                   GTK_SWITCH ( m_switchAutoSaveFile ) ) );

    // to choose another color for the screen
    g_signal_connect ( G_OBJECT ( m_pushbuttonNewTransparentColor ),
                       "clicked",
                       G_CALLBACK ( s_chooseTransparentColor ),
                       static_cast<gpointer> ( this ) );

    _setPageName ( PAGE_ID_INTERFACE );
}

GtkWidget* AP_UnixDialog_Options::_constructWindow ()
{
    GtkWidget *mainWindow;
    const XAP_StringSet * pSS = m_pApp->getStringSet();

    GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_Options.ui");

    // Update member variables with the important widgets that
    // might need to be queried or altered later.

    _constructWindowContents ( builder );

    mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_Options"));
    UT_ASSERT(mainWindow);

    // set the dialog title
    std::string s;
    pSS->getValueUTF8(AP_STRING_ID_DLG_Options_OptionsTitle, s);
    abiDialogSetTitle(mainWindow, "%s", s.c_str());

    // the control buttons
    g_signal_connect ( G_OBJECT ( m_buttonDefaults ),
                       "clicked",
                       G_CALLBACK ( s_defaults_clicked ),
                       static_cast<gpointer> ( this ) );


    // create user data tControl -> stored in widgets
    for ( int i = 0; i < id_last; i++ )
    {
        GtkWidget *w = _lookupWidget ( static_cast<tControl> ( i ) );
        if ( ! ( w && GTK_IS_WIDGET ( w ) ) )
            continue;

        /* check to see if there is any data already stored there (note, will
         * not work if 0's is stored in multiple places  */
        UT_ASSERT ( g_object_get_data ( G_OBJECT ( w ), "tControl" ) == nullptr );

        g_object_set_data ( G_OBJECT ( w ), "tControl", reinterpret_cast<gpointer> ( i ) );
        if ( GTK_IS_DROP_DOWN ( w ) )
            g_signal_connect ( G_OBJECT ( w ),
                               "notify::selected",
                               G_CALLBACK ( s_dropdown_changed ),
                               static_cast<gpointer> ( this ) );
        else if ( GTK_IS_ENTRY ( w ) )
            g_signal_connect ( G_OBJECT ( w ),
                               "changed",
                               G_CALLBACK ( s_control_changed ),
                               static_cast<gpointer> ( this ) );
        else if ( GTK_IS_SWITCH ( w ) )
            g_signal_connect ( G_OBJECT ( w ),
                               "notify::active",
                               G_CALLBACK ( s_switch_changed ),
                               static_cast<gpointer> ( this ) );
        else if ( GTK_IS_STACK ( w ) )
            g_signal_connect ( G_OBJECT ( w ),
                               "notify::visible-child-name",
                               G_CALLBACK ( s_dropdown_changed ),
                               static_cast<gpointer> ( this ) );
        else if ( GTK_IS_TOGGLE_BUTTON ( w ) )
            g_signal_connect ( G_OBJECT ( w ),
                               "toggled",
                               G_CALLBACK ( s_control_changed ),
                               static_cast<gpointer> ( this ) );
        else if ( GTK_IS_SPIN_BUTTON ( w ) )
            g_signal_connect ( G_OBJECT ( w ),
                               "value-changed",
                               G_CALLBACK ( s_control_changed ),
                               static_cast<gpointer> ( this ) );
    }

	g_object_unref(G_OBJECT(builder));

    return mainWindow;
}

GtkWidget *AP_UnixDialog_Options::_lookupWidget ( tControl id )
{
    switch ( id )
    {
            // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // Smart quotes

        case id_CHECK_SMART_QUOTES_ENABLE:
            return m_switchSmartQuotes;

        case id_CHECK_CUSTOM_SMART_QUOTES:
            return m_switchCustomSmartQuotes;

        case id_LIST_VIEW_OUTER_QUOTE_STYLE:
            return m_omOuterQuoteStyle;

        case id_LIST_VIEW_INNER_QUOTE_STYLE:
            return m_omInnerQuoteStyle;

            // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // other
        case id_CHECK_OTHER_DEFAULT_DIRECTION_RTL:
            return m_switchOtherDirectionRtl;

        case id_CHECK_AUTO_SAVE_FILE:
            return m_switchAutoSaveFile;

        case id_TEXT_AUTO_SAVE_FILE_EXT:
            return m_textAutoSaveFileExt;

        case id_TEXT_AUTO_SAVE_FILE_PERIOD:
            return m_textAutoSaveFilePeriod;

            // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // view
        case id_LIST_VIEW_RULER_UNITS:
            return m_menuUnits;


        case id_PUSH_CHOOSE_COLOR_FOR_TRANSPARENT:
            return  m_pushbuttonNewTransparentColor;
        case id_CHECK_ENABLE_OVERWRITE:
            return m_switchEnableOverwrite;

            // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // spelling
        case id_CHECK_SPELL_AUTO:
            return m_switchSpellCheckAuto;

        case id_CHECK_SPELL_CAPS:
            return m_switchSpellCheckCaps;

        case id_CHECK_SPELL_NUMBERS:
            return m_switchSpellCheckNumbers;

        case id_CHECK_SPELL_INTERNET:
            return m_switchSpellCheckInternet;

            // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // general

        case id_BUTTON_DEFAULTS:
            return m_buttonDefaults;

        case id_NOTEBOOK:
            return m_stack;

            // not implemented
        case id_CHECK_VIEW_SHOW_STATUS_BAR:
        case id_CHECK_VIEW_SHOW_RULER:
        case id_CHECK_VIEW_UNPRINTABLE:
        case id_CHECK_ENABLE_SMOOTH_SCROLLING:
        case id_CHECK_VIEW_ALL:
        case id_CHECK_VIEW_HIDDEN_TEXT:
        case id_COMBO_PREFS_SCHEME:
        case id_CHECK_PREFS_AUTO_SAVE:

        case id_BUTTON_SAVE:
        case id_BUTTON_APPLY:
        case id_BUTTON_CANCEL:
        case id_BUTTON_OK:
            return nullptr;

        default:
            UT_ASSERT ( "Unknown Widget" );
            return nullptr;
    }

    UT_ASSERT ( UT_SHOULD_NOT_HAPPEN );
    return nullptr;
}

void AP_UnixDialog_Options::_controlEnable ( tControl id, bool value )
{
    GtkWidget *w = _lookupWidget ( id );

    if ( w && GTK_IS_WIDGET ( w ) )
        gtk_widget_set_sensitive ( w, value );
}


#define DEFINE_GET_SET_BOOL(button) \
    bool     AP_UnixDialog_Options::_gather##button(void) {    \
        UT_ASSERT(m_switch##button && GTK_IS_SWITCH(m_switch##button)); \
        return gtk_switch_get_active(        \
                GTK_SWITCH(m_switch##button) ); }   \
    void        AP_UnixDialog_Options::_set##button(bool b) { \
        UT_ASSERT(m_switch##button && GTK_IS_SWITCH(m_switch##button)); \
        gtk_switch_set_active (          \
                                                GTK_SWITCH(m_switch##button), b ); }

#define DEFINE_GET_SET_TEXT(widget) \
    char *  AP_UnixDialog_Options::_gather##widget() {    \
        UT_ASSERT(m_text##widget && GTK_IS_EDITABLE(m_text##widget)); \
        return gtk_editable_get_chars(GTK_EDITABLE(m_text##widget), 0, -1); }   \
    \
    void  AP_UnixDialog_Options::_set##widget(const char *t) { \
        int pos = 0;             \
        UT_ASSERT(m_text##widget && GTK_IS_EDITABLE(m_text##widget)); \
        gtk_editable_delete_text(GTK_EDITABLE(m_text##widget), 0, -1);    \
        gtk_editable_insert_text(GTK_EDITABLE(m_text##widget), t, strlen(t), &pos); \
    }

DEFINE_GET_SET_BOOL ( SmartQuotes )
DEFINE_GET_SET_BOOL ( CustomSmartQuotes )

DEFINE_GET_SET_BOOL ( OtherDirectionRtl )

DEFINE_GET_SET_BOOL ( AutoSaveFile )
DEFINE_GET_SET_BOOL ( EnableOverwrite )

DEFINE_GET_SET_BOOL ( SpellCheckAuto )
DEFINE_GET_SET_BOOL ( SpellCheckCaps )
DEFINE_GET_SET_BOOL ( SpellCheckNumbers )
DEFINE_GET_SET_BOOL ( SpellCheckInternet )

// dummy implementations. XP pref backend isn't very smart.
#define DEFINE_GET_SET_BOOL_DUMMY(Bool)     \
    bool AP_UnixDialog_Options::_gather##Bool(void) {   \
        return m_bool##Bool;     \
    }        \
    void AP_UnixDialog_Options::_set##Bool(bool b) {   \
        m_bool##Bool = b;     \
    }


DEFINE_GET_SET_BOOL_DUMMY ( EnableSmoothScrolling )
DEFINE_GET_SET_BOOL_DUMMY ( PrefsAutoSave )
DEFINE_GET_SET_BOOL_DUMMY ( ViewAll )
DEFINE_GET_SET_BOOL_DUMMY ( ViewHiddenText )
DEFINE_GET_SET_BOOL_DUMMY ( ViewShowRuler )
DEFINE_GET_SET_BOOL_DUMMY ( ViewShowStatusBar )
DEFINE_GET_SET_BOOL_DUMMY ( ViewUnprintable )

void AP_UnixDialog_Options::_gatherAutoSaveFileExt ( UT_String &stRetVal )
{
    UT_ASSERT ( m_textAutoSaveFileExt && GTK_IS_EDITABLE ( m_textAutoSaveFileExt ) );
    char *tmp = gtk_editable_get_chars ( GTK_EDITABLE ( m_textAutoSaveFileExt ), 0, -1 );
    stRetVal = tmp;
    g_free ( tmp );
}

void AP_UnixDialog_Options::_setAutoSaveFileExt ( const UT_String &stExt )
{
    int pos = 0;
    UT_ASSERT ( m_textAutoSaveFileExt && GTK_IS_EDITABLE ( m_textAutoSaveFileExt ) );
    gtk_editable_delete_text ( GTK_EDITABLE ( m_textAutoSaveFileExt ), 0, -1 );
    gtk_editable_insert_text ( GTK_EDITABLE ( m_textAutoSaveFileExt ), stExt.c_str(), stExt.size(), &pos );
}

void AP_UnixDialog_Options::_gatherAutoSaveFilePeriod ( UT_String &stRetVal )
{
    UT_ASSERT ( m_textAutoSaveFilePeriod && GTK_IS_SPIN_BUTTON ( m_textAutoSaveFilePeriod ) );
    char nb[12];
    int val = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON ( m_textAutoSaveFilePeriod ) );
    g_snprintf ( nb, 12, "%d", val );
    stRetVal = nb;
}

void AP_UnixDialog_Options::_setAutoSaveFilePeriod ( const UT_String &stPeriod )
{
    UT_ASSERT ( m_textAutoSaveFilePeriod && GTK_IS_EDITABLE ( m_textAutoSaveFilePeriod ) );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON ( m_textAutoSaveFilePeriod ), atoi ( stPeriod.c_str() ) );
}

void AP_UnixDialog_Options::_setupSaveFormatMenu ( GtkWidget *optionmenu )
{
	GtkDropDown *combo = GTK_DROP_DOWN(optionmenu);
	XAP_makeGtkDropDown(combo);

	// populate with every registered exporter; the row value is the
	// first suffix of the type's suffix list, which is what the
	// DefaultSaveFormat pref stores (".abw", ".rtf", ".txt", ...)
	const char * szDesc = nullptr;
	const char * szSuffixList = nullptr;
	IEFileType ieft = IEFT_Unknown;
	UT_uint32 k = 0;
	while (IE_Exp::enumerateDlgLabels(k, &szDesc, &szSuffixList, &ieft))
	{
		std::string suffix;
		if (szSuffixList)
		{
			const char * p = szSuffixList;
			if (*p == '*')
				p++;
			const char * e = strchr(p, ';');
			suffix = e ? std::string(p, static_cast<size_t>(e - p)) : std::string(p);
		}
		if (!suffix.empty() && szDesc)
			XAP_appendDropDownTextAndString(combo, szDesc, suffix.c_str());
		k++;
	}
	gtk_drop_down_set_selected(combo, 0);
}

void AP_UnixDialog_Options::_gatherDefaultSaveFormat ( UT_String &stRetVal )
{
	stRetVal.clear();
	UT_return_if_fail ( m_menuSaveFormat && GTK_IS_DROP_DOWN ( m_menuSaveFormat ) );
	const char * value =
		XAP_dropDownGetSelectedString(GTK_DROP_DOWN(m_menuSaveFormat));
	if (value)
		stRetVal = value;
}

void AP_UnixDialog_Options::_setDefaultSaveFormat ( const UT_String &stExt )
{
	UT_return_if_fail ( m_menuSaveFormat && GTK_IS_DROP_DOWN ( m_menuSaveFormat ) );
	if (!XAP_dropDownSetSelectedFromString(GTK_DROP_DOWN(m_menuSaveFormat),
										 stExt.c_str()))
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_menuSaveFormat), 0);
}

UT_Dimension AP_UnixDialog_Options::_gatherViewRulerUnits ( void )
{
    UT_ASSERT ( m_menuUnits && GTK_IS_DROP_DOWN ( m_menuUnits ) );
	return ( UT_Dimension ) XAP_dropDownGetSelectedInt(GTK_DROP_DOWN(m_menuUnits));
}

gint AP_UnixDialog_Options::_gatherOuterQuoteStyle ( void )
{
    UT_ASSERT ( m_omOuterQuoteStyle && GTK_IS_DROP_DOWN( m_omOuterQuoteStyle ) );
	return XAP_dropDownGetSelectedInt(GTK_DROP_DOWN(m_omOuterQuoteStyle));
}


gint AP_UnixDialog_Options::_gatherInnerQuoteStyle ( void )
{
    UT_ASSERT ( m_omInnerQuoteStyle && GTK_IS_DROP_DOWN ( m_omInnerQuoteStyle ) );
	return XAP_dropDownGetSelectedInt(GTK_DROP_DOWN(m_omInnerQuoteStyle));
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void AP_UnixDialog_Options::_setViewRulerUnits ( UT_Dimension dim )
{
    UT_ASSERT ( m_menuUnits && GTK_IS_DROP_DOWN ( m_menuUnits ) );

	XAP_dropDownSetSelectedFromInt(GTK_DROP_DOWN(m_menuUnits), dim);
}
void AP_UnixDialog_Options::_setOuterQuoteStyle ( gint nIndex )
{
    UT_ASSERT ( m_omOuterQuoteStyle && GTK_IS_DROP_DOWN ( m_omOuterQuoteStyle ) );
	XAP_dropDownSetSelectedFromInt(GTK_DROP_DOWN(m_omOuterQuoteStyle), nIndex);
}

void AP_UnixDialog_Options::_setInnerQuoteStyle ( gint nIndex )
{
    UT_ASSERT ( m_omInnerQuoteStyle && GTK_IS_DROP_DOWN ( m_omInnerQuoteStyle ) );
	XAP_dropDownSetSelectedFromInt(GTK_DROP_DOWN(m_omInnerQuoteStyle), nIndex);
}


#undef DEFINE_GET_SET_BOOL


void AP_UnixDialog_Options::_gatherPageName ( std::string &stRetVal )
{
    UT_ASSERT ( m_stack && GTK_IS_STACK ( m_stack ) );
    const char *name = gtk_stack_get_visible_child_name ( GTK_STACK ( m_stack ) );
    stRetVal = name ? name : "";
}

void AP_UnixDialog_Options::_setPageName ( const std::string &stName )
{
    UT_ASSERT ( m_stack && GTK_IS_STACK ( m_stack ) );
    if ( gtk_stack_get_child_by_name ( GTK_STACK ( m_stack ), stName.c_str() ) )
        gtk_stack_set_visible_child_name ( GTK_STACK ( m_stack ),
                                           stName.c_str() );
    else
        gtk_stack_set_visible_child_name ( GTK_STACK ( m_stack ),
                                           PAGE_ID_INTERFACE );
}

/*static*/ void AP_UnixDialog_Options::s_defaults_clicked ( GtkWidget *widget, gpointer data )
{
    AP_UnixDialog_Options * dlg = static_cast<AP_UnixDialog_Options *> ( data );
    UT_UNUSED ( widget );
    UT_ASSERT ( widget && dlg );
    dlg->_event_SetDefaults();

}

/*static*/ void AP_UnixDialog_Options::s_control_changed ( GtkWidget *widget, gpointer data )
{
    guint id;
    UT_DEBUGMSG ( ( "Control changed\n" ) );
    AP_UnixDialog_Options *dlg = static_cast<AP_UnixDialog_Options *> ( data );
    UT_ASSERT ( widget && dlg );

    if ( dlg->isInitialPopulationHappenning() )
    {
        return;
    }

    id = GPOINTER_TO_INT ( g_object_get_data ( G_OBJECT ( widget ), "tControl" ) );
    dlg->_storeDataForControl ( static_cast <tControl> ( id ) );
}

/*static*/ void AP_UnixDialog_Options::s_dropdown_changed ( GtkWidget *widget, GParamSpec * /*pspec*/, gpointer data )
{
	s_control_changed ( widget, data );
}

/*static*/ void AP_UnixDialog_Options::s_chooseTransparentColor ( GtkWidget *widget, gpointer data )
{
    AP_UnixDialog_Options * dlg = static_cast<AP_UnixDialog_Options *> ( data );
    UT_UNUSED ( widget );
    UT_ASSERT ( widget && dlg );
    dlg->event_ChooseTransparentColor();
}


// Switch state changes run the XP enable/disable logic (smart quotes
// combos), keep the autosave sub-grid sensitivity in sync, and store
// the preference through the generic control-changed path.
/*static*/ void AP_UnixDialog_Options::s_switch_changed ( GObject *w,
                                                        GParamSpec * /*pspec*/,
                                                        gpointer data )
{
    AP_UnixDialog_Options * dlg = static_cast<AP_UnixDialog_Options *> ( data );
    UT_ASSERT ( dlg );
    UT_ASSERT ( w && GTK_IS_SWITCH ( w ) );

    GtkWidget *widget = GTK_WIDGET ( w );
    int i = GPOINTER_TO_INT ( g_object_get_data ( G_OBJECT ( widget ), "tControl" ) );
    UT_DEBUGMSG ( ( "s_switch_changed: control id = %d\n", i ) );

    dlg->_enableDisableLogic ( ( AP_Dialog_Options::tControl ) i );

    if ( i == AP_Dialog_Options::id_CHECK_AUTO_SAVE_FILE )
        gtk_widget_set_sensitive ( dlg->m_gridAutoSaveFile,
                                   gtk_switch_get_active ( GTK_SWITCH ( w ) ) );

    s_control_changed ( widget, data );
}


void AP_UnixDialog_Options::_storeWindowData ( void )
{
    AP_Dialog_Options::_storeWindowData();
}

void AP_UnixDialog_Options::_setupSmartQuotesCombos(  GtkWidget *optionmenu  )
{
	GtkDropDown * combo = GTK_DROP_DOWN(optionmenu);

	XAP_makeGtkDropDown(combo);

    UT_UCS4Char wszDisplayString[4];
	for (size_t i = 0; XAP_EncodingManager::smartQuoteStyles[i].leftQuote != static_cast<UT_UCS4Char>(0); ++i)
	{
		wszDisplayString[0] = XAP_EncodingManager::smartQuoteStyles[i].leftQuote;
		wszDisplayString[1] = static_cast<gunichar>('O');
		wszDisplayString[2] = XAP_EncodingManager::smartQuoteStyles[i].rightQuote;
		wszDisplayString[3] = static_cast<gunichar>(0);
        gchar* szDisplayStringUTF8 = g_ucs4_to_utf8 ( wszDisplayString, -1, nullptr, nullptr, nullptr );
		XAP_appendDropDownTextAndInt(combo, szDisplayStringUTF8, i);
        g_free ( szDisplayStringUTF8 );
	}
	gtk_drop_down_set_selected(combo, 0);
}
