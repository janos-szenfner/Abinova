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

/* Embedded audio/video/binary file objects. The document stores the
 * payload as a piece-table data item and a "snapshot-png-<dataid>"
 * poster renders it in-line; activating the object plays audio/video
 * through GTK4's GtkMediaFile/GtkVideo (GStreamer or platform
 * backends) or hands generic files to the system handler. */

#pragma once

#include <string>
#include <vector>
#include "gr_EmbedManager.h"
#include "ut_string_class.h"

class AD_Document;
class fp_Run;
class UT_ByteBuf;

class ABI_EXPORT GR_GtkMediaManager : public GR_EmbedManager
{
public:
	GR_GtkMediaManager(GR_Graphics * pG, const char * objectType = "media");
	virtual ~GR_GtkMediaManager();

	virtual GR_EmbedManager * create(GR_Graphics * pG) override;
	virtual const char *   getObjectType(void) const override;
	virtual const char *   getMimeType(void) const override;
	virtual const char *   getMimeTypeDescription(void) const override;
	virtual const char *   getMimeTypeSuffix(void) const override;
	virtual UT_sint32      makeEmbedView(AD_Document * pDoc, UT_uint32 api,
	                                     const char * szDataID) override;
	virtual void           releaseEmbedView(UT_sint32 uid) override;
	virtual bool           isDefault(void) override;
	virtual bool           isEdittable(UT_sint32 uid) override;
	virtual bool           isResizeable(UT_sint32 uid) override;
	virtual bool           modify(UT_sint32 uid) override;
	virtual void           render(UT_sint32 uid, UT_Rect & rec) override;
	virtual void           setRun(UT_sint32 uid, fp_Run * run) override;

private:
	struct MediaItem {
		std::string dataID;
		fp_Run *    run = nullptr;
	};

	MediaItem *          _item(UT_sint32 uid);
	std::string          _writeTempFile(const std::string & dataID,
	                                    const UT_ConstByteBufPtr & buf,
	                                    const std::string & mime);
	static std::string   _suffixForMime(const std::string & mime);

	std::vector<MediaItem *> m_items;
	std::vector<std::string> m_tempFiles;
	std::string          m_objectType;
	AD_Document *        m_pDoc;
};
