/* AbiSource Application Framework
 * Copyright (C) 2011-2022 Hubert Figuière
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

#include "src/af/util/xp/t/ut_bytebuf.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_types.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_locale.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_misc.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_vector.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_string.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_std_string.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_string_class.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_units.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_hyphen.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_uuid.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_bijection.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_encoding.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_growbuf.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_hash.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_path.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_raii.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_xml.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_base64.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_decompress.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_go_file.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_jpeg.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_Language.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_mbtowc.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_Script.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_svg.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_color.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_conversion.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_crc32.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_iconv.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_math.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_OverstrikingChars.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_stringbuf.t.cpp"
#undef TFSUITE
#include "src/af/util/xp/t/ut_timer.t.cpp"
#undef TFSUITE
#include "src/af/ev/xp/t/ev_Tables.t.cpp"
#undef TFSUITE
#include "src/af/gr/xp/t/gr_Primitives.t.cpp"
#undef TFSUITE
#include "src/af/xap/xp/t/xap_Prefs.t.cpp"
#undef TFSUITE
#include "src/af/xap/xp/t/xap_UpdateCheck.t.cpp"
#undef TFSUITE
#include "src/af/xap/xp/t/xap_EncodingManager.t.cpp"
#undef TFSUITE
#include "src/af/xap/xp/t/xap_Misc.t.cpp"
#undef TFSUITE
#include "src/af/xap/xp/t/xap_Dlgs.t.cpp"
#undef TFSUITE
#include "src/af/xap/xp/t/xap_App.t.cpp"
#undef TFSUITE
#include "src/af/xap/xp/t/xad_Document.t.cpp"
#undef TFSUITE
#include "src/text/ptbl/xp/t/pf_Fragments.t.cpp"
#undef TFSUITE
#include "src/text/ptbl/xp/t/pt_PieceTable.t.cpp"
#undef TFSUITE
#include "src/text/ptbl/xp/t/pp_PropertyMap.t.cpp"
#undef TFSUITE
#include "src/text/ptbl/xp/t/pp_Revision.t.cpp"
#undef TFSUITE
#include "src/text/ptbl/xp/t/pd_Revision.t.cpp"
#undef TFSUITE
#include "src/text/ptbl/xp/t/pd_RDFDoc.t.cpp"
#undef TFSUITE
#include "src/text/ptbl/xp/t/pt_DocEdits.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fl_AutoNum.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fl_TableStyles.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_ViewModes.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_FootnoteDelete.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_HdrFtrDelete.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_HdrFtrDblClick.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_MouseContext.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_FieldContext.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_TOCContext.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_FrameContext.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_ImageProps.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_PosObjectContext.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_EditOps.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_TableOps.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_RefsTOC.t.cpp"
#undef TFSUITE
#include "src/wp/ap/xp/t/ap_KeyBindings.t.cpp"
#undef TFSUITE
#include "src/wp/ap/xp/t/ap_TopRuler.t.cpp"
#undef TFSUITE
#include "src/wp/ap/xp/t/ap_EditMethods.t.cpp"
#undef TFSUITE
#include "src/wp/ap/xp/t/ap_MenuFns.t.cpp"
#undef TFSUITE
#include "src/wp/ap/xp/t/ap_Autosave.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ut_abwncrypt.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_abinova.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_tocstyle.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_pastelistener.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_clipcopy.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_PasteTag.t.cpp"
#undef TFSUITE
#include "src/text/fmt/xp/t/fv_SignatureLine.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_fixtures.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_math.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_sniffers.t.cpp"
#undef TFSUITE
#include "src/wp/impexp/xp/t/ie_xxe.t.cpp"
#undef TFSUITE
