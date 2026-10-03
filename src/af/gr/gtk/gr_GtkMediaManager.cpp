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

#include <cstring>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "gr_GtkMediaManager.h"
#include "gr_CairoGraphics.h"
#include "xad_Document.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"
#include "ut_bytebuf.h"
#include "ut_path.h"

GR_GtkMediaManager::GR_GtkMediaManager(GR_Graphics * pG,
                                       const char * objectType)
	: GR_EmbedManager(pG)
	, m_objectType(objectType && *objectType ? objectType : "media")
	, m_pDoc(nullptr)
{
}

GR_GtkMediaManager::~GR_GtkMediaManager()
{
	for (size_t i = 0; i < m_items.size(); ++i)
		delete m_items[i];
	for (const auto & f : m_tempFiles)
		g_unlink(f.c_str());
}

GR_EmbedManager * GR_GtkMediaManager::create(GR_Graphics * pG)
{
	return new GR_GtkMediaManager(pG, m_objectType.c_str());
}

const char * GR_GtkMediaManager::getObjectType(void) const
{
	return m_objectType.c_str();
}

const char * GR_GtkMediaManager::getMimeType(void) const
{
	return "application/octet-stream";
}

const char * GR_GtkMediaManager::getMimeTypeDescription(void) const
{
	return "Embedded media";
}

const char * GR_GtkMediaManager::getMimeTypeSuffix(void) const
{
	return ".bin";
}

bool GR_GtkMediaManager::isDefault(void)
{
	return false;
}

bool GR_GtkMediaManager::isEdittable(UT_sint32 /*uid*/)
{
	return true;
}

bool GR_GtkMediaManager::isResizeable(UT_sint32 /*uid*/)
{
	return true;
}

GR_GtkMediaManager::MediaItem * GR_GtkMediaManager::_item(UT_sint32 uid)
{
	if (uid < 0 || uid >= static_cast<UT_sint32>(m_items.size()))
		return nullptr;
	return m_items[uid];
}

UT_sint32 GR_GtkMediaManager::makeEmbedView(AD_Document * pDoc, UT_uint32 api,
                                          const char * szDataID)
{
	UT_sint32 uid = GR_EmbedManager::makeEmbedView(pDoc, api, szDataID);
	MediaItem * it = new MediaItem;
	if (szDataID)
		it->dataID = szDataID;
	m_pDoc = pDoc;
	m_items.push_back(it);
	return uid;
}

void GR_GtkMediaManager::releaseEmbedView(UT_sint32 uid)
{
	MediaItem * it = _item(uid);
	if (it) {
		delete it;
		m_items[uid] = nullptr;
	}
	GR_EmbedManager::releaseEmbedView(uid);
}

void GR_GtkMediaManager::setRun(UT_sint32 uid, fp_Run * run)
{
	MediaItem * it = _item(uid);
	if (it)
		it->run = run;
}

/* a small mime->suffix table so temp files keep an extension the
 * decoders/system handlers can recognize */
std::string GR_GtkMediaManager::_suffixForMime(const std::string & mime)
{
	static const struct { const char * mime; const char * ext; } map[] = {
		{"video/mp4", ".mp4"}, {"video/mpeg", ".mpg"},
		{"video/quicktime", ".mov"}, {"video/x-msvideo", ".avi"},
		{"video/x-matroska", ".mkv"}, {"video/webm", ".webm"},
		{"video/x-ms-wmv", ".wmv"}, {"video/ogg", ".ogv"},
		{"video/3gpp", ".3gp"}, {"video/x-flv", ".flv"},
		{"audio/mpeg", ".mp3"}, {"audio/mp4", ".m4a"},
		{"audio/ogg", ".ogg"}, {"audio/wav", ".wav"},
		{"audio/x-wav", ".wav"}, {"audio/flac", ".flac"},
		{"audio/aac", ".aac"}, {"audio/x-ms-wma", ".wma"},
		{"audio/webm", ".weba"}, {"audio/midi", ".mid"},
		{"application/pdf", ".pdf"}, {"text/plain", ".txt"},
		{"application/zip", ".zip"}
	};
	for (const auto & e : map)
		if (mime == e.mime)
			return e.ext;
	return ".bin";
}

/* media payloads live inside the document; GtkMediaFile and external
 * handlers both want a real file, so the data item is spilled to a
 * private tmp file (removed again in the destructor) */
std::string GR_GtkMediaManager::_writeTempFile(const std::string & dataID,
                                               const UT_ConstByteBufPtr & buf,
                                               const std::string & mime)
{
	std::string path = UT_createTmpFile("abinova-embed-" + dataID,
	                                    _suffixForMime(mime));
	if (path.empty())
		return "";
	GError * err = nullptr;
	if (!g_file_set_contents(path.c_str(),
	                         reinterpret_cast<const gchar *>(buf->getPointer(0)),
	                         buf->getLength(), &err))
	{
		UT_DEBUGMSG(("GR_GtkMediaManager: temp write failed: %s\n",
		             err ? err->message : "?"));
		if (err)
			g_error_free(err);
		g_unlink(path.c_str());
		return "";
	}
	m_tempFiles.push_back(path);
	return path;
}

static bool s_playMediaFile(const std::string & path)
{
	GtkWidget * win = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(win), UT_basename(path.c_str()));
	gtk_window_set_default_size(GTK_WINDOW(win), 480, 360);
	GtkWidget * video = gtk_video_new_for_filename(path.c_str());
	if (!video)
	{
		gtk_window_destroy(GTK_WINDOW(win));
		return false;
	}
	gtk_window_set_child(GTK_WINDOW(win), video);
	gtk_window_present(GTK_WINDOW(win));
	return true;
}

static bool s_openWithSystem(const std::string & path)
{
	gchar * uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
	if (!uri)
		return false;
	GError * err = nullptr;
	bool ok = g_app_info_launch_default_for_uri(uri, nullptr, &err);
	g_free(uri);
	if (!ok && err)
	{
		UT_DEBUGMSG(("GR_GtkMediaManager: open failed: %s\n", err->message));
		g_error_free(err);
	}
	return ok;
}

bool GR_GtkMediaManager::modify(UT_sint32 uid)
{
	MediaItem * it = _item(uid);
	UT_return_val_if_fail(it, false);
	if (it->dataID.empty() || !m_pDoc)
		return false;

	UT_ConstByteBufPtr buf;
	std::string mime;
	if (!m_pDoc->getDataItemDataByName(it->dataID.c_str(), buf, &mime, nullptr) ||
	    !buf || !buf->getLength())
		return false;

	std::string path = _writeTempFile(it->dataID, buf, mime);
	if (path.empty())
		return false;

	if (!mime.compare(0, 6, "audio/") || !mime.compare(0, 6, "video/"))
		return s_playMediaFile(path);
	return s_openWithSystem(path);
}

void GR_GtkMediaManager::render(UT_sint32 uid, UT_Rect & rec)
{
	/* like the math manager, non-default managers get rec.top as the
	 * baseline — the base renderer wants the box's top edge instead */
	UT_Rect box = rec;
	box.top -= rec.height;
	GR_EmbedManager::render(uid, box);
	if (m_objectType != "media" || rec.width <= 0 || rec.height <= 0)
		return;

	/* play badge over the poster so media reads as playable */
	GR_CairoGraphics * pUGG = static_cast<GR_CairoGraphics *>(getGraphics());
	UT_return_if_fail(pUGG);
	bool ownPaint = (pUGG->getPaintCount() == 0);
	if (ownPaint)
		pUGG->beginPaint();
	cairo_t * cr = pUGG->getCairo();
	if (cr)
	{
		double cx = pUGG->tdu(box.left + box.width / 2);
		double cy = pUGG->tdu(box.top + box.height / 2);
		double r = pUGG->tdu(MIN(rec.width, rec.height)) * 0.18;
		if (r > 0)
		{
			cairo_save(cr);
			cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
			cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
			cairo_fill(cr);
			double t = r * 0.55;
			cairo_move_to(cr, cx - t * 0.6, cy - t);
			cairo_line_to(cr, cx - t * 0.6, cy + t);
			cairo_line_to(cr, cx + t, cy);
			cairo_close_path(cr);
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
			cairo_fill(cr);
			cairo_restore(cr);
		}
	}
	if (ownPaint)
		pUGG->endPaint();
}
